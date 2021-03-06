/*
 * Traceshark - a visualizer for visualizing ftrace and perf traces
 * Copyright (C) 2016  Viktor Rosendahl <viktor.rosendahl@gmail.com>
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

#include "analyzer/task.h"
#include "analyzer/traceanalyzer.h"
#include "misc/traceshark.h"

TaskName::TaskName():
	str(nullptr), prev(nullptr)
{}

Task::Task():
	AbstractTask(), taskName(nullptr), exitStatus(STATUS_ALIVE),
	lastWakeUP(0), lastSleepEntry(0), wakeUpGraph(nullptr),
	runningGraph(nullptr)
{}

Task::~Task()
{
	TaskName *prev;
	TaskName *current = taskName;
	while(current != nullptr) {
		prev = current->prev;
		delete current;
		current = prev;
	}
}

void Task::addName(char *name)
{
	TaskName *newName = new TaskName();
	newName->str = name;
	newName->prev = taskName;
	taskName = newName;
}

QString Task::getDisplayName()
{
	QString nameStr;

	if (taskName != nullptr) {
		nameStr += QString(taskName->str);
		if (taskName->prev != nullptr) {
			nameStr += QString("(")
				+ QString(taskName->prev->str)
				+ QString(")");
			if (taskName->prev->prev != nullptr)
				nameStr += QString("...");
		}
	}
	return nameStr;
}

QString Task::getLastName()
{
	QString empty;

	if (taskName != nullptr)
		return QString(taskName->str);
	return empty;
}
