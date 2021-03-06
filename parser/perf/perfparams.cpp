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

#include "parser/perf/perfparams.h"

char *perf_sched_switch_oldname_strdup(const TraceEvent &event, MemPool *pool)
{
	return __perf_sched_switch_oldname_strdup(event, pool);
}

char *perf_sched_switch_newname_strdup(const TraceEvent &event, MemPool *pool)
{
	return __perf_sched_switch_newname_strdup(event, pool);
}

char *perf_sched_wakeup_name_strdup(const TraceEvent &event, MemPool *pool)
{
	return __perf_sched_wakeup_name_strdup(event, pool);
}

char *perf_sched_process_fork_childname_strdup(const TraceEvent &event,
					       MemPool *pool)
{
	return __perf_sched_process_fork_childname_strdup(event, pool);
}
