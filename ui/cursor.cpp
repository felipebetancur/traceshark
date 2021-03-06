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

#include <QColor>
#include <QPen>
#include "ui/cursor.h"

Cursor::Cursor(QCustomPlot *parent, const QColor &color):
	QCPItemLine(parent)
{
	QPen pen;
	pen.setStyle(Qt::DashLine);
	pen.setWidth(2);
	pen.setColor(color);
	setPen(pen);
	setPosition(0);
	position = 0;
}

void Cursor::setPosition(double pos)
{
	start->setCoords(pos, -10000000000000000);
	end->setCoords(pos, +10000000000000000);
	parentPlot()->replot();
	position = pos;
}

double Cursor::getPosition()
{
	return position;
}
