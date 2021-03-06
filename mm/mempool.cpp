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

#include "mm/mempool.h"

extern "C" {
#include <sys/mman.h>
}

extern "C" {
#include <unistd.h>
}

MemPool::MemPool(unsigned int nr_pages, unsigned long objsize)
{
	poolSize = nr_pages * sysconf(_SC_PAGESIZE);
	objSize = objsize;
	next = nullptr;
	memory = nullptr;
	if (!newMap()) {
		used = 0;
		poolSize = 0;
	}
}

MemPool::~MemPool()
{
	int i;
	int len = exhaustList.size();
	for (i = 0; i < len; i++) {
		munmap(exhaustList[i], poolSize);
	}
	if (memory != nullptr)
		munmap(memory, poolSize);
}

bool MemPool::addMemory()
{
	exhaustList.append(memory);
	if (newMap())
		return true;
	exhaustList.removeLast();
	return false;
}

void MemPool::reset()
{
	int i;
	int len = exhaustList.size();
	for (i = 0; i < len; i++)
		munmap(exhaustList[i], poolSize);
	exhaustList.clear();
	used = 0ULL;
	next = memory;
}
