/*
 * Traceshark - a visualizer for visualizing ftrace and perf traces
 * Copyright (C) 2015, 2016  Viktor Rosendahl <viktor.rosendahl@gmail.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QTextStream>
#include <QDateTime>

#include "ui/cursor.h"
#include "ui/eventinfodialog.h"
#include "ui/eventswidget.h"
#include "analyzer/traceanalyzer.h"
#include "ui/infowidget.h"
#include "ui/legendgraph.h"
#include "ui/licensedialog.h"
#include "ui/mainwindow.h"
#include "ui/migrationline.h"
#include "ui/taskgraph.h"
#include "ui/taskrangeallocator.h"
#include "parser/traceevent.h"
#include "ui/traceplot.h"
#include "misc/traceshark.h"
#include "threads/workqueue.h"
#include "threads/workitem.h"
#include "qcustomplot/qcustomplot.h"

MainWindow::MainWindow():
	tracePlot(nullptr)
{
	analyzer = new TraceAnalyzer;

	//setCentralWidget(traceLabel);

	createActions();
	createToolBars();
	createMenus();

	plotWidget = new QWidget(this);
	plotLayout = new QVBoxLayout(plotWidget);
	setCentralWidget(plotWidget);

	/* createTracePlot needs to have plotWidget created */
	createTracePlot();

	tsconnect(tracePlot, mouseWheel(QWheelEvent*), this, mouseWheel());
	tsconnect(tracePlot->xAxis, rangeChanged(QCPRange), tracePlot->xAxis2,
		  setRange(QCPRange));
	tsconnect(tracePlot, mousePress(QMouseEvent*), this, mousePress());
	tsconnect(tracePlot,selectionChangedByUser() , this,
		  selectionChanged());
	tsconnect(tracePlot, plottableClick(QCPAbstractPlottable *,
					    QMouseEvent *), this,
		  plottableClicked(QCPAbstractPlottable*, QMouseEvent*));
	tsconnect(tracePlot, legendDoubleClick(QCPLegend*,
					       QCPAbstractLegendItem*,
					       QMouseEvent*), this,
		  legendDoubleClick(QCPLegend*, QCPAbstractLegendItem*));
	eventsWidget = new EventsWidget(this);
	addDockWidget(Qt::BottomDockWidgetArea, eventsWidget);

	infoWidget = new InfoWidget(this);
	addDockWidget(Qt::TopDockWidgetArea, infoWidget);

	cursors[TShark::RED_CURSOR] = nullptr;
	cursors[TShark::BLUE_CURSOR] = nullptr;

	licenseDialog = new LicenseDialog();
	eventInfoDialog = new EventInfoDialog();

	tsconnect(tracePlot, mouseDoubleClick(QMouseEvent*),
		  this, plotDoubleClicked(QMouseEvent*));
	tsconnect(infoWidget, valueChanged(double, int),
		  this, infoValueChanged(double, int));
	tsconnect(infoWidget, addTaskGraph(unsigned int), this,
		  addTaskGraph(unsigned int));
	tsconnect(infoWidget, findWakeup(unsigned int), this,
		  showWakeup(unsigned int));
	tsconnect(infoWidget, removeTaskGraph(unsigned int), this,
		  removeTaskGraph(unsigned int));
	tsconnect(eventsWidget, timeSelected(double), this,
		  moveActiveCursor(double));
	tsconnect(eventsWidget, infoDoubleClicked(const TraceEvent &),
		  this, showEventInfo(const TraceEvent &));

	setupSettings();
}

void MainWindow::createTracePlot()
{
	QString mainLayerName = QString("main");
	QString cursorLayerName = QString("cursor");
	QCPLayer *mainLayer;

	tracePlot = new TracePlot(plotWidget);
	taskRangeAllocator = new TaskRangeAllocator(schedHeight
						    + schedSpacing);
	taskRangeAllocator->setStart(bugWorkAroundOffset);

	mainLayer = tracePlot->layer(mainLayerName);

	tracePlot->addLayer(cursorLayerName, mainLayer, QCustomPlot::limAbove);
	cursorLayer = tracePlot->layer(cursorLayerName);

	tracePlot->setCurrentLayer(mainLayerName);

	tracePlot->setAutoAddPlottableToLegend(false);
	tracePlot->hide();
	plotLayout->addWidget(tracePlot);

	tracePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom |
				   QCP::iSelectAxes | QCP::iSelectLegend |
				   QCP::iSelectPlottables);

	analyzer->setQCustomPlot(tracePlot);
}

MainWindow::~MainWindow()
{
	closeTrace();
	delete analyzer;
	delete tracePlot;
	delete taskRangeAllocator;
	delete licenseDialog;
	delete eventInfoDialog;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	/* Here is a great place to save settings, if we ever want to do it */
	event->accept();
	/* event->ignore() could be used to refuse to close the window */
}

void MainWindow::openTrace()
{
	QString name = QFileDialog::getOpenFileName(this);
	if (!name.isEmpty()) {
		eventsWidget->beginResetModel();
		eventsWidget->setEvents(nullptr);
		eventsWidget->endResetModel();
		if (analyzer->isOpen())
			analyzer->close();
		loadTraceFile(name);
	}
	if (analyzer->isOpen()) {
		QTextStream qout(stdout);
		qout.setRealNumberPrecision(6);
		qout.setRealNumberNotation(QTextStream::FixedNotation);
		quint64 start, process, layout, rescale, showt, eventsw;
		quint64 scursor, tshow;

		clearPlot();
		start = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

		processTrace();
		process = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

		computeLayout();
		layout = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

		eventsWidget->beginResetModel();
		eventsWidget->setEvents(&analyzer->events);
		eventsWidget->endResetModel();
		eventsw = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

		setupCursors();
		scursor = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

		rescaleTrace();
		rescale = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

		showTrace();
		showt = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

		tracePlot->show();
		tshow = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

		qout << "processTrace() took "
		     << (double) (process - start) / 1000;
		qout << " s\n";
		qout << "computeLayout() took "
		     << (double) (layout - process) / 1000;
		qout << " s\n";
		qout << "updating EventsWidget took "
		     << (double) (eventsw - layout) / 1000;
		qout << " s\n";
		qout << "setupCursors() took "
		     << (double) (scursor - eventsw) / 1000;
		qout << " s\n";
		qout << "rescaleTrace() took "
		     << (double) (rescale - scursor) / 1000;
		qout << " s\n";
		qout << "showTrace() took "
		     << (double) (showt - rescale) / 1000;
		qout << " s\n";
		qout << "tracePlot->show() took "
		     << (double) (tshow - showt) / 1000;
		qout << " s\n";
		tracePlot->legend->setVisible(true);
	}
}

void MainWindow::processTrace()
{
	analyzer->processTrace();
}

void MainWindow::computeLayout()
{
	unsigned int cpu;
	unsigned int nrCPUs;
	unsigned int offset;
	QString label;
	double inc, o, p;
	double start, end;
	MigrationLine *line;
	QColor color;

	start = analyzer->getStartTime();
	end = analyzer->getEndTime();

	bottom = bugWorkAroundOffset;
	offset = bottom + migrateSectionOffset;

	ticks.resize(0);
	tickLabels.resize(0);
	nrCPUs = analyzer->getNrCPUs();

	analyzer->setMigrationOffset(offset);
	inc = nrCPUs * 315 + 67.5;
	analyzer->setMigrationScale(inc);

	/* add labels and lines here for the migration graph */
	color = QColor(135, 206, 250); /* Light sky blue */
	label = QString("fork/exit");
	ticks.append(offset);
	line = new MigrationLine(start, end, offset, color, tracePlot);
	tracePlot->addItem(line);
	tickLabels.append(label);
	o = offset;
	p = inc / nrCPUs ;
	for (cpu = 0; cpu < nrCPUs; cpu++) {
		o += p;
		label = QString("cpu") + QString::number(cpu);
		ticks.append(o);
		tickLabels.append(label);
		line = new MigrationLine(start, end, o, color, tracePlot);
		tracePlot->addItem(line);
	}

	offset += inc;
	offset += p;

	offset += schedSectionOffset;

	/* Set the offset and scale of the scheduling graphs */
	for (cpu = 0; cpu < nrCPUs; cpu++) {
		analyzer->setSchedOffset(cpu, offset);
		analyzer->setSchedScale(cpu, schedHeight);
		label = QString("cpu") + QString::number(cpu);
		ticks.append(offset);
		tickLabels.append(label);
		offset += schedHeight + schedSpacing;
	}

	offset += cpuSectionOffset;

	for (cpu = 0; cpu < nrCPUs; cpu++) {
		analyzer->setCpuFreqOffset(cpu, offset);
		analyzer->setCpuIdleOffset(cpu, offset);
		analyzer->setCpuFreqScale(cpu, cpuHeight);
		analyzer->setCpuIdleScale(cpu, cpuHeight);
		label = QString("cpu") + QString::number(cpu);
		ticks.append(offset);
		tickLabels.append(label);
		offset += cpuHeight + cpuSpacing;
	}

	top = offset;
}

void MainWindow::rescaleTrace()
{
	analyzer->doScale();
}

void MainWindow::clearPlot()
{
	cursors[TShark::RED_CURSOR] = nullptr;
	cursors[TShark::BLUE_CURSOR] = nullptr;
	tracePlot->clearItems();
	tracePlot->clearPlottables();
	tracePlot->hide();
	taskRangeAllocator->clearAll();
	infoWidget->setTime(0, TShark::RED_CURSOR);
	infoWidget->setTime(0, TShark::BLUE_CURSOR);
}

void MainWindow::showTrace()
{
	unsigned int cpu;
	double start, end;
	int precision = 7;
	double extra = 0;
	QColor color;

	start = analyzer->getStartTime();
	end = analyzer->getEndTime();

	if (end >= 10)
		extra = floor (log(end) / log(10));

	precision += (int) extra;

	tracePlot->yAxis->setRange(QCPRange(bottom, top));
	tracePlot->xAxis->setRange(QCPRange(start, end));
	tracePlot->xAxis->setNumberPrecision(precision);
	tracePlot->yAxis->setTicks(false);
	tracePlot->yAxis->setAutoTicks(false);
	tracePlot->yAxis->setAutoTickLabels(false);
	tracePlot->yAxis->setTickVector(ticks);
	tracePlot->yAxis->setTickVectorLabels(tickLabels);
	tracePlot->yAxis->setTickLabels(true);
	tracePlot->yAxis->setTicks(true);

	/* Show CPU frequency and idle graphs */
	for (cpu = 0; cpu <= analyzer->getMaxCPU(); cpu++) {
		QPen pen = QPen();

		QCPGraph *graph = new QCPGraph(tracePlot->xAxis,
					       tracePlot->yAxis);
		QString name = QString(tr("cpuidle")) + QString::number(cpu);
		pen.setColor(Qt::green);
		graph->setPen(pen);
		graph->setName(name);
		graph->setAdaptiveSampling(true);
		tracePlot->addPlottable(graph);
		graph->setLineStyle(QCPGraph::lsStepLeft);
		graph->setData(analyzer->cpuIdle[cpu].timev,
			       analyzer->cpuIdle[cpu].scaledData);

		graph = new QCPGraph(tracePlot->xAxis, tracePlot->yAxis);
		name = QString(tr("cpufreq")) + QString::number(cpu);
		graph->setName(name);
		graph->setAdaptiveSampling(true);
		tracePlot->addPlottable(graph);
		graph->setLineStyle(QCPGraph::lsStepLeft);
		graph->setData(analyzer->cpuFreq[cpu].timev,
			       analyzer->cpuFreq[cpu].scaledData);
	}

	/* Show scheduling graphs */
	for (cpu = 0; cpu <= analyzer->getMaxCPU(); cpu++) {
		DEFINE_CPUTASKMAP_ITERATOR(iter) = analyzer->
			cpuTaskMaps[cpu].begin();
		while(iter != analyzer->cpuTaskMaps[cpu].end()) {
			CPUTask &task = iter.value();
			iter++;

			addSchedGraph(task);
			addHorizontalWakeupGraph(task);
			addWakeupGraph(task);
			addStillRunningGraph(task);
		}
	}
	tracePlot->replot();
}

void MainWindow::setupCursors()
{
	double start, end, red, blue;

	start = analyzer->getStartTime();
	end = analyzer->getEndTime();

	cursors[TShark::RED_CURSOR] = new Cursor(tracePlot, Qt::red);
	cursors[TShark::BLUE_CURSOR] = new Cursor(tracePlot, Qt::blue);

	tracePlot->addItem(cursors[TShark::RED_CURSOR]);
	tracePlot->addItem(cursors[TShark::BLUE_CURSOR]);

	cursors[TShark::RED_CURSOR]->setLayer(cursorLayer);
	cursors[TShark::BLUE_CURSOR]->setLayer(cursorLayer);

	red = (start + end) / 2;
	cursors[TShark::RED_CURSOR]->setPosition(red);
	infoWidget->setTime(red, TShark::RED_CURSOR);
	blue = (start + end) / 2 + (end - start) / 10;
	cursors[TShark::BLUE_CURSOR]->setPosition(blue);
	infoWidget->setTime(blue, TShark::BLUE_CURSOR);
	/* Fixme:
	 * For some reason the EventsWidget doesn't want to make its first 
	 * scroll to somewhere in the middle of the trace. As a work around
	 * we first scroll to the beginning and to the end, and then to 
	 * where we want */
	eventsWidget->scrollTo(start);
	eventsWidget->scrollTo(end);
	eventsWidget->scrollTo(red);
}

void MainWindow::setupSettings()
{
	settings[Setting::HORIZONTAL_WAKEUP].isEnabled = false;
	settings[Setting::HORIZONTAL_WAKEUP].name =
		tr("Show horizontal wakeup");
}

void MainWindow::addSchedGraph(CPUTask &cpuTask)
{
	/* Add scheduling graph */
	TaskGraph *graph = new TaskGraph(tracePlot->xAxis, tracePlot->yAxis);
	QColor color = analyzer->getTaskColor(cpuTask.pid);
	Task *task = analyzer->findTask(cpuTask.pid);
	QPen pen = QPen();

	pen.setColor(color);
	graph->setPen(pen);
	graph->setTask(task);
	tracePlot->addPlottable(graph);
	graph->setLineStyle(QCPGraph::lsStepLeft);
	graph->setAdaptiveSampling(true);
	graph->setData(cpuTask.schedTimev, cpuTask.scaledSchedData);
	cpuTask.graph = graph; /* Save a pointer to the graph object in the task */
}

void MainWindow::addHorizontalWakeupGraph(CPUTask &task)
{
	if (!settings[Setting::HORIZONTAL_WAKEUP].isEnabled)
		return;

	/* Add wakeup graph on top of scheduling */
	QCPGraph *graph = new QCPGraph(tracePlot->xAxis, tracePlot->yAxis);
	tracePlot->addPlottable(graph);
	QCPScatterStyle style = QCPScatterStyle(QCPScatterStyle::ssDot);
	QColor color = analyzer->getTaskColor(task.pid);
	QPen pen = QPen();

	pen.setColor(color);
	style.setPen(pen);
	graph->setScatterStyle(style);
	graph->setLineStyle(QCPGraph::lsNone);
	graph->setAdaptiveSampling(true);
	graph->setDataKeyError(task.wakeTimev, task.wakeHeight,
			       task.wakeDelay, task.wakeZero);
	graph->setErrorType(QCPGraph::etKey);
	graph->setErrorBarSize(4);
	graph->setErrorPen(pen);
}

void MainWindow::addWakeupGraph(CPUTask &task)
{
	/* Add wakeup graph on top of scheduling */
	QCPGraph *graph = new QCPGraph(tracePlot->xAxis, tracePlot->yAxis);
	tracePlot->addPlottable(graph);
	QCPScatterStyle style = QCPScatterStyle(QCPScatterStyle::ssDot);
	QColor color = analyzer->getTaskColor(task.pid);
	QPen pen = QPen();

	pen.setColor(color);
	style.setPen(pen);
	graph->setScatterStyle(style);
	graph->setLineStyle(QCPGraph::lsNone);
	graph->setAdaptiveSampling(true);
	graph->setDataValueError(task.wakeTimev, task.wakeHeight, task.wakeZero,
				 task.verticalDelay);
	graph->setErrorType(QCPGraph::etValue);
	graph->setErrorBarSize(4);
	graph->setErrorPen(pen);
}

void MainWindow::addStillRunningGraph(CPUTask &task)
{
	/* Add still running graph on top of the other two...*/
	if (task.runningTimev.size() == 0)
		return;
	QCPGraph *graph = new QCPGraph(tracePlot->xAxis, tracePlot->yAxis);
	QString name = QString(tr("is runnable"));
	graph->setName(name);
	tracePlot->addPlottable(graph);
	QCPScatterStyle style = QCPScatterStyle(QCPScatterStyle::ssCircle, 5);
	QPen pen = QPen();

	pen.setColor(Qt::red);
	style.setPen(pen);
	graph->setScatterStyle(style);
	graph->setLineStyle(QCPGraph::lsNone);
	graph->setAdaptiveSampling(true);
	graph->setData(task.runningTimev, task.scaledRunningData);
}

void MainWindow::closeTrace()
{
	eventsWidget->beginResetModel();
	eventsWidget->setEvents(nullptr);
	eventsWidget->endResetModel();
	clearPlot();
	if(analyzer->isOpen())
		analyzer->close();
	infoWidget->clear();
}

void MainWindow::saveScreenshot()
{
	QFileDialog dialog(this);
	QStringList fileNameList;
	QString fileName;
	QString pngSuffix = QString(".png");
	QString bmpSuffix = QString(".bmp");
	QString jpgSuffix = QString(".jpg");
	QString pdfSuffix = QString(".pdf");
	QString pdfCreator = QString("traceshark ");
	QString pdfTitle;

	pdfCreator += QString(TRACESHARK_VERSION_STRING);

	if (!analyzer->isOpen())
		return;

	switch (analyzer->getTraceType()) {
	case TRACE_TYPE_FTRACE:
		pdfTitle = tr("Ftrace rendered by ");
		break;
	case TRACE_TYPE_PERF:
		pdfTitle = tr("Perf events rendered by ");
		break;
	default:
		pdfTitle = tr("Uknown garbage rendered by ");
		break;
	}

	pdfTitle += pdfCreator;

	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setNameFilter(tr("Images (*.png *.bmp *.jpg *.pdf)"));
	dialog.setViewMode(QFileDialog::Detail);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setDefaultSuffix(QString("png"));

	if (dialog.exec())
		fileNameList = dialog.selectedFiles();

	if (fileNameList.size() != 1)
		return;

	fileName = fileNameList.at(0);

	if (fileName.endsWith(pngSuffix, Qt::CaseInsensitive)) {
		tracePlot->savePng(fileName);
	} else if (fileName.endsWith(bmpSuffix, Qt::CaseInsensitive)) {
		tracePlot->saveBmp(fileName);
	} else if (fileName.endsWith(jpgSuffix, Qt::CaseInsensitive)) {
		tracePlot->saveJpg(fileName);
	} else if (fileName.endsWith(pdfSuffix, Qt::CaseInsensitive)) {
		tracePlot->savePdf(fileName, false, 0, 0, pdfCreator, pdfTitle);
	}
}

void MainWindow::about()
{
	QString textAboutCaption;
	QString textAbout;

	textAboutCaption = QMessageBox::tr(
	       "<h1>About Traceshark</h1>"
	       "<p>This is version %1.</p>"
		).arg(QLatin1String(TRACESHARK_VERSION_STRING));
	textAbout = QMessageBox::tr(
	       "<p>Copyright &copy; 2014, 2015, 2016 Viktor Rosendahl"
	       "<p>This program comes with ABSOLUTELY NO WARRANTY; details below."
	       "<p>This is free software, and you are welcome to redistribute it"
	       " under certain conditions; select \"License\" under the \"Help\""
	       " menu for details."

	       "<h2>15. Disclaimer of Warranty.</h2>"
	       "<p>THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT "
	       "PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN OTHERWISE STATED IN "
	       "WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES PROVIDE "
	       "THE PROGRAM \"AS IS\" WITHOUT WARRANTY OF ANY KIND, EITHER "
	       "EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE "
	       "IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A "
	       "PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND "
	       "PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE PROGRAM "
	       "PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY "
	       "SERVICING, REPAIR OR CORRECTION."

	       "<h2>16. Limitation of Liability.</h2>"
	       "<p>IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED "
	       "TO IN WRITING WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY "
	       "WHO MODIFIES AND/OR CONVEYS THE PROGRAM AS PERMITTED ABOVE, "
	       "BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, "
	       "INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR "
	       "INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED TO "
	       "LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES "
	       "SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM "
	       "TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER OR "
	       "OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH "
	       "DAMAGES."

	       "<h2>17. Interpretation of Sections 15 and 16.</h2>"
	       "<p>If the disclaimer of warranty and limitation of "
	       "liability provided above cannot be given local legal effect "
	       "according to their terms, reviewing courts shall apply local "
	       "law that most closely approximates an absolute waiver of all "
	       "civil liability in connection with the Program, unless a "
	       "warranty or assumption of liability accompanies a copy of the "
	       "Program in return for a fee.");
	QMessageBox *msgBox = new QMessageBox(this);
	msgBox->setAttribute(Qt::WA_DeleteOnClose);
	msgBox->setWindowTitle(tr("About Traceshark"));
	msgBox->setText(textAboutCaption);
	msgBox->setInformativeText(textAbout);

	QPixmap pm(QLatin1String(":/traceshark/images/shark.png"));
	if (!pm.isNull())
		msgBox->setIconPixmap(pm);
	msgBox->show();
}

void MainWindow::aboutQCustomPlot()
{
	QString textAboutCaption;
	QString textAbout;

	textAboutCaption = QMessageBox::tr(
	       "<h1>About QCustomPlot</h1>"
	       "<p>This program contains version %1 of QCustomPlot.</p>"
		).arg(QLatin1String(QCUSTOMPLOT_VERSION_STRING));
	textAbout = QMessageBox::tr(
	       "<p>Copyright &copy; 2011-2015 Emanuel Eichhammer"
	       "<p>QCustomPlot is licensed under GNU General Public License as "
	       "published by the Free Software Foundation, either version 3 of "
	       " the License, or (at your option) any later version.</p>"
	       "<p>See <a href=\"%1/\">%1</a> for more information about QCustomPlot.</p>"
	       "<p>This program comes with ABSOLUTELY NO WARRANTY; select \"License\" under the \"Help\""
	       " menu for details."
	       "<p>This is free software, and you are welcome to redistribute it"
	       " under certain conditions; see the license for details.").arg(QLatin1String("http://qcustomplot.com"));
	QMessageBox *msgBox = new QMessageBox(this);
	msgBox->setAttribute(Qt::WA_DeleteOnClose);
	msgBox->setWindowTitle(tr("About QCustomPlot"));
	msgBox->setText(textAboutCaption);
	msgBox->setInformativeText(textAbout);

	QPixmap pm(QLatin1String(":/traceshark/images/qcp-logo.png"));
	if (!pm.isNull())
		msgBox->setIconPixmap(pm);
	msgBox->show();
}

void MainWindow::license()
{
	// Figure out some way to display the whole GPL nicely here
	licenseDialog->show();
}

void MainWindow::mouseWheel()
{
	bool xSelected = tracePlot->yAxis->selectedParts().
		testFlag(QCPAxis::spAxis);
	bool ySelected = tracePlot->yAxis->selectedParts().
		testFlag(QCPAxis::spAxis);

	if (xSelected && ySelected) /* This is not possible but would be cool */
		tracePlot->axisRect()->setRangeZoom(Qt::Vertical |
						    Qt::Horizontal);
	else if (ySelected)
		tracePlot->axisRect()->setRangeZoom(Qt::Vertical);
	else
		tracePlot->axisRect()->setRangeZoom(Qt::Horizontal);
}

void MainWindow::mousePress()
{
	bool xSelected = tracePlot->yAxis->selectedParts().
		testFlag(QCPAxis::spAxis);
	bool ySelected = tracePlot->yAxis->selectedParts().
		testFlag(QCPAxis::spAxis);

	if (xSelected && ySelected) /* This is not possible but would be cool */
		tracePlot->axisRect()->setRangeDrag(Qt::Vertical |
						    Qt::Horizontal);
	else if (ySelected)
		tracePlot->axisRect()->setRangeDrag(Qt::Vertical);
	else
		tracePlot->axisRect()->setRangeDrag(Qt::Horizontal);
}

void MainWindow::plotDoubleClicked(QMouseEvent *event)
{
	int cursorIdx;
	QVariant details;
	QCPLayerable *clickedLayerable;
	QCPLegend *legend;
	QCPAbstractLegendItem *legendItem;

	/* Let's filter out double clicks on the legend or its items */
	clickedLayerable = tracePlot->getLayerableAt(event->pos(), false,
						     &details);
	if (clickedLayerable != nullptr) {
		legend = qobject_cast<QCPLegend*>(clickedLayerable);
		if (legend != nullptr)
			return;
		legendItem = qobject_cast<QCPAbstractLegendItem*>
			(clickedLayerable);
		if (legendItem != nullptr)
			return;
	}

	cursorIdx = infoWidget->getCursorIdx();
	if (cursorIdx != TShark::RED_CURSOR && cursorIdx != TShark::BLUE_CURSOR)
		return;

	Cursor *cursor = cursors[cursorIdx];
	if (cursor != nullptr) {
		double pixel = (double) event->x();
		double coord = tracePlot->xAxis->pixelToCoord(pixel);
		cursor->setPosition(coord);
		eventsWidget->scrollTo(coord);
		infoWidget->setTime(coord, cursorIdx);
	}
}

void MainWindow::infoValueChanged(double value, int nr)
{
	Cursor *cursor;
	if (nr == TShark::RED_CURSOR || nr == TShark::BLUE_CURSOR) {
		cursor = cursors[nr];
		if (cursor != nullptr)
			cursor->setPosition(value);
		eventsWidget->scrollTo(value);
	}
}

void MainWindow::moveActiveCursor(double time)
{
	int cursorIdx;

	cursorIdx = infoWidget->getCursorIdx();
	if (cursorIdx != TShark::RED_CURSOR && cursorIdx != TShark::BLUE_CURSOR)
		return;

	Cursor *cursor = cursors[cursorIdx];
	if (cursor != nullptr) {
		cursor->setPosition(time);
		infoWidget->setTime(time, cursorIdx);
	}
}

void MainWindow::showEventInfo(const TraceEvent &event)
{
	eventInfoDialog->show(event);
}

void MainWindow::createActions()
{
	openAction = new QAction(tr("&Open"), this);
	openAction->setShortcuts(QKeySequence::Open);
	openAction->setStatusTip(tr("Open a trace file..."));
	tsconnect(openAction, triggered(), this, openTrace());

	closeAction = new QAction(tr("&Close"), this);
	closeAction->setShortcuts(QKeySequence::Close);
	closeAction->setStatusTip(tr("Close the trace"));
	tsconnect(closeAction, triggered(), this, closeTrace());

	saveAction = new QAction(tr("&Save screenshot as..."), this);
	saveAction->setShortcuts(QKeySequence::SaveAs);
	saveAction->setStatusTip(tr("Save screenshot to an image file"));
	tsconnect(saveAction, triggered(), this, saveScreenshot());

	exitAction = new QAction(tr("E&xit"), this);
	exitAction->setShortcuts(QKeySequence::Quit);
	exitAction->setStatusTip(tr("Exit traceshark"));
	tsconnect(exitAction, triggered(), this, close());

	aboutQtAction = new QAction(tr("About &Qt"), this);
	aboutQtAction->setStatusTip(tr("Show info about Qt"));
	tsconnect(aboutQtAction, triggered(), qApp, aboutQt());

	aboutAction = new QAction(tr("&About Traceshark"), this);
	aboutAction->setStatusTip(tr("Show info about Traceshark"));
	tsconnect(aboutAction, triggered(), this, about());

	aboutQCPAction = new QAction(tr("About QCustom&Plot"), this);
	aboutAction->setStatusTip(tr("Show info about QCustomPlot"));
	tsconnect(aboutQCPAction, triggered(), this, aboutQCustomPlot());

	licenseAction = new QAction(tr("&License"), this);
	aboutAction->setStatusTip(tr("Show the license of Traceshark"));
	tsconnect(licenseAction, triggered(), this, license());
}

void MainWindow::createToolBars()
{
}

void MainWindow::createMenus()
{
	fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(openAction);
	fileMenu->addAction(closeAction);
	fileMenu->addAction(saveAction);
	fileMenu->addSeparator();
	fileMenu->addAction(exitAction);

	viewMenu = menuBar()->addMenu(tr("&View"));

	helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(aboutAction);
	helpMenu->addAction(aboutQCPAction);
	helpMenu->addAction(aboutQtAction);
	helpMenu->addAction(licenseAction);
}


void MainWindow::loadTraceFile(QString &fileName)
{
	qint64 start, stop;
	QTextStream qout(stdout);

	qout.setRealNumberPrecision(6);
	qout.setRealNumberNotation(QTextStream::FixedNotation);

	qout << "opening " << fileName << "\n";
	
	start = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
	if (!analyzer->open(fileName)) {
		qout << "failed to open " << fileName << "\n";
	}
	stop = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

	stop = stop - start;

	qout << "Loading took " << (double) stop / 1000 << " s\n";
	qout.flush();
#if 0
	int i, s;
	unsigned int j;
	s = analyzer->events.size();
	for (i = 0; i < s; i++) {
		TraceEvent &event = analyzer->events[i];
		qout << event.taskName->ptr << " " << event.pid << " " <<
			event.time << " " << event.eventName->ptr;
		for (j = 0; j < event.argc; j++) {
			qout << " " << event.argv[j]->ptr;
		}
		qout << "\n";
        }
#endif
}

void MainWindow::plottableClicked(QCPAbstractPlottable *plottable,
				  QMouseEvent * /* event */)
{
	TaskGraph *graph;

	graph = qobject_cast<TaskGraph *>(plottable);
	if (graph == nullptr)
		return;
	if (graph->selected())
		infoWidget->setTaskGraph(graph);
	else
		infoWidget->removeTaskGraph();
}

void MainWindow::selectionChanged()
{
	infoWidget->checkGraphSelection();
}

void MainWindow::legendDoubleClick(QCPLegend * /* legend */,
				   QCPAbstractLegendItem *abstractItem)
{
	QCPPlottableLegendItem *plottableItem;
	QCPAbstractPlottable *plottable;
	LegendGraph *legendGraph;

	plottableItem = qobject_cast<QCPPlottableLegendItem*>(abstractItem);
	if (plottableItem == nullptr)
		return;
	plottable = plottableItem->plottable();
	legendGraph = qobject_cast<LegendGraph*>(plottable);
	if (legendGraph == nullptr)
		return;
	legendGraph->removeFromLegend();
	/* Inform the TaskInfo class (inside InfoWidget) that the pid has
	 * been removed. This is needed because InfoWidget keeps track of this
	 * for the purpose of preventing the same pid being added twice from
	 * different LegendGraphs, there might be "identical" LegendGraphs
	 * when the same pid has migrated between CPUs */
	infoWidget->pidRemoved(legendGraph->pid);
}

void MainWindow::addTaskGraph(unsigned int pid)
{
	/* Add a unified scheduling graph for pid */
	bool isNew;
	TaskRange *taskRange;
	TaskGraph *taskGraph;
	unsigned int cpu;
	CPUTask *cpuTask;

	taskRange = taskRangeAllocator->getTaskRange(pid, isNew);

	if (!isNew || taskRange == nullptr)
		return;

	Task *task = analyzer->findTask(pid);
	QColor color = analyzer->getTaskColor(pid);

	if (task == nullptr) {
		taskRangeAllocator->putTaskRange(taskRange);
		return;
	}

	for (cpu = 0; cpu < analyzer->getNrCPUs(); cpu++) {
		cpuTask = analyzer->findCPUTask(pid, cpu);
		if (cpuTask != nullptr)
			break;
	}
	if (cpuTask == nullptr || cpuTask->graph == nullptr) {
		taskRangeAllocator->putTaskRange(taskRange);
		return;
	}

	bottom = taskRangeAllocator->getBottom();

	taskGraph = new TaskGraph(tracePlot->xAxis, tracePlot->yAxis);
	taskGraph->setTaskGraphForLegend(cpuTask->graph);
	QPen pen = QPen();

	pen.setColor(color);
	taskGraph->setPen(pen);
	taskGraph->setTask(task);
	tracePlot->addPlottable(taskGraph);
	taskGraph->setLineStyle(QCPGraph::lsStepLeft);
	taskGraph->setAdaptiveSampling(true);

	task->offset = taskRange->lower;
	task->scale = schedHeight;
	task->doScale();
	task->doScaleWakeup();
	task->doScaleRunning();

	taskGraph->setData(task->schedTimev, task->scaledSchedData);
	task->graph = taskGraph;

	/* Add the horizontal wakeup graph as well */
	QCPGraph *graph = new QCPGraph(tracePlot->xAxis, tracePlot->yAxis);
	tracePlot->addPlottable(graph);
	QCPScatterStyle style = QCPScatterStyle(QCPScatterStyle::ssDot);
	style.setPen(pen);
	graph->setScatterStyle(style);
	graph->setLineStyle(QCPGraph::lsNone);
	graph->setAdaptiveSampling(true);
	graph->setDataKeyError(task->wakeTimev, task->wakeHeight,
			       task->wakeDelay, task->wakeZero);
	graph->setErrorType(QCPGraph::etKey);
	graph->setErrorBarSize(4);
	graph->setErrorPen(pen);
	task->wakeUpGraph = graph;

	/* Add the still running graph on top of the other two... */
	QString name = QString(tr("is runnable"));
	QCPScatterStyle rstyle = QCPScatterStyle(QCPScatterStyle::ssCircle, 5);
	if (task->runningTimev.size() == 0) {
		task->runningGraph = nullptr;
		goto out;
	}
	graph = new QCPGraph(tracePlot->xAxis, tracePlot->yAxis);
	graph->setName(name);
	tracePlot->addPlottable(graph);

	pen.setColor(Qt::red);
	rstyle.setPen(pen);
	graph->setScatterStyle(rstyle);
	graph->setLineStyle(QCPGraph::lsNone);
	graph->setAdaptiveSampling(true);
	graph->setData(task->runningTimev, task->scaledRunningData);
	task->runningGraph = graph;
out:
	/* We only modify the lower part of the range to show the newly
	 * added unified task graph. */
	QCPRange range = tracePlot->yAxis->range();
	tracePlot->yAxis->setRange(QCPRange(bottom, range.upper));

	tracePlot->replot();
}

void MainWindow::removeTaskGraph(unsigned int pid)
{
	Task *task = analyzer->findTask(pid);

	if (task == nullptr)
		return;

	if (task->graph != nullptr) {
		tracePlot->removePlottable(task->graph);
		task->graph = nullptr;
	}

	if (task->wakeUpGraph != nullptr) {
		tracePlot->removePlottable(task->wakeUpGraph);
		task->wakeUpGraph = nullptr;
	}

	if (task->runningGraph != nullptr) {
		tracePlot->removePlottable(task->runningGraph);
		task->runningGraph = nullptr;
	}

	taskRangeAllocator->putTaskRange(pid);
	bottom = taskRangeAllocator->getBottom();

	QCPRange range = tracePlot->yAxis->range();
	tracePlot->yAxis->setRange(QCPRange(bottom, range.upper));

	tracePlot->replot();
}

void MainWindow::showWakeup(unsigned int pid)
{
	int activeIdx = infoWidget->getCursorIdx();
	int inactiveIdx;
	int wakeUpIndex;
	int schedIndex;

	if (activeIdx != TShark::RED_CURSOR &&
	    activeIdx != TShark::BLUE_CURSOR) {
		return;
	}

	inactiveIdx = TShark::RED_CURSOR;
	if (activeIdx == inactiveIdx)
		inactiveIdx = TShark::BLUE_CURSOR;

	Cursor *activeCursor = cursors[activeIdx];
	Cursor *inactiveCursor = cursors[inactiveIdx];

	if (activeCursor == nullptr || inactiveCursor == nullptr)
		return;

	/* The time of the active cursor is taken to be the time that the
	 * user is interested in, i.e. finding the previous wake up event
	 * relative to */
	double zerotime = activeCursor->getPosition();
	TraceEvent *schedevent = analyzer->findPreviousSchedEvent(zerotime, pid,
								  &schedIndex);
	if (schedevent == nullptr)
		return;

	TraceEvent *wakeupevent = analyzer->
		findPreviousWakeupEvent(schedIndex, pid, &wakeUpIndex);
	if (wakeupevent == nullptr)
		return;
	/* This is what we do, we move the *inactive* cursor to the wakeup
	 * event, move the *active* cursor to the scheduling event and then
	 * finally scroll the events widget to the same time */
	inactiveCursor->setPosition(wakeupevent->time);
	activeCursor->setPosition(schedevent->time);
	infoWidget->setTime(wakeupevent->time, inactiveIdx);
	infoWidget->setTime(schedevent->time, activeIdx);
	eventsWidget->scrollTo(wakeUpIndex);
}
