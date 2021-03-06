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

#include "threads/workqueue.h"
#include <QThread>

#define DEFAULT_NR_CPUS (6) /* Isn't this what most people are running now? */

WorkQueue::WorkQueue():
	error(false)
{
	int cpus, i;
	cpus = QThread::idealThreadCount();
	nrThreads = cpus > 0 ? cpus:DEFAULT_NR_CPUS;
	threads = new WorkThread<WorkQueue>[nrThreads]();
	for (i = 0; i < nrThreads; i++)
		threads[i].setObjFn(this, &WorkQueue::ThreadRun);
}

WorkQueue::~WorkQueue()
{
	delete[] threads;
}

void WorkQueue::addWorkItem(AbstractWorkItem *item)
{
	queue.append(item);
}

void WorkQueue::addDefaultWorkItem(AbstractWorkItem *item)
{
	defaultQueue.append(item);
}

void WorkQueue::setWorkItemsDefault()
{
	queue = defaultQueue;
}

void WorkQueue::start()
{
	int i;
	int qs = queue.size();

	error = false;
	nrStarted = TSMIN(qs, nrThreads);
	for (i = 0; i < nrStarted; i++)
		threads[i].start();
}

bool WorkQueue::wait()
{
	int i;
	for (i = 0; i < nrStarted; i++)
		threads[i].wait();
	return error;
}

void WorkQueue::ThreadRun() {
	AbstractWorkItem *current;
	bool empty;

	do {
		bool rval = false;
		queueMutex.lock();
		empty = queue.isEmpty();
		if (!empty)
			current = queue.takeFirst();
		queueMutex.unlock();
		if (!empty)
			rval = current->__runWork();
		if (rval) {
			errorMutex.lock();
			error |= rval;
			errorMutex.unlock();
		}
	} while(!empty);
}
