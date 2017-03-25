/***************************************************************************
 *   Copyright (C) 2009-2012 Virginia Tech Real-Time Systems Lab           *
 *                                                                         *
 *   Written by Aaron Lindsay                                              *
 *   aaron@aclindsay.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef WORKLOAD_TYPE_H
#define WORKLOAD_TYPE_H

/*
 * This struct is what is used to hold the functions for a workload in a way so
 * they can be generically called without having to do a select-case every time
 * on the workload type.
 */
struct workload {
	/*
	 * The workload's name. This is used to calculate the slope filenames for this
	 * workload, in addition to serving as an identifier (may be printed in console
	 * messages about this workload).
	 */
	const char *name;

	/*
	 * This is a series of flags (defined below) which specify what this workload's
	 * capabilities are.
	 */
	unsigned int capabilities;

	/*
	 * The min/max working set sizes this workload allows per-task and per-group.
	 */
	unsigned int min_task_wss, max_task_wss, min_group_wss, max_group_wss;

	/*
	 * Initialize the global state needed for this workload. This should include
	 * initializing anything that is not per-task. It is assumed that any such state
	 * will be stored in static variables in that workload's execution unit, so no
	 * pointer is returned as is the case for init_task, below.
	 */
	void (*init_global) ();

	/*
	 * Initialize the per-thread group state needed for this workload. Return a
	 * void pointer to any per-thread group data which needs to be passed in
	 * whenever do_work is called for a task in this group. 'group_wss' specifies
	 * the size of the memory in bytes that tasks in this group should share. The
	 * details of what this means are left up to the workload implementation.
	 */
	void *(*init_group) (long group_wss);

	/*
	 * Initialize the per-task state needed for this workload. Return a void pointer
	 * to any per-task data which needs to be passed in whenever do_work is called
	 * for a task. The void pointer passed in is the same one returned from an
	 * earlier call to init_group by this task's thread group leader. 'task_wss'
	 * specifies the amount of memory in bytes that will be used by this task (and
	 * this task only). The total working set size of this task will be
	 * `task_wss + group_wss`, where group_wss is the value passed into the call to
	 * init_group() for this task's thread group.
	 */
	void *(*init_task) (void *, long task_wss);

	/*
	 * Do the reverse of init_global. Free any memory malloc-ed there, etc.
	 */
	void (*cleanup_global) ();

	/*
	 * Do the reverse of init_group. Free any memory malloc-ed there, etc. The
	 * pointer passed in is the same pointer returned by init_group earlier.
	 */
	void (*cleanup_group) (void *);

	/*
	 * Do the reverse of init_task. Free any memory malloc-ed there, etc. The
	 * first void pointer argument is the same pointer returned by init_group for
	 * this task's group leader. The second void pointer is the same one returned
	 * by an earlier call to init_task for this task.
	 */
	void (*cleanup_task) (void *, void *);

	/*
	 * This function is the "heart" of the workload. The first pointer passed in is
	 * the same pointer returned earlier by init_group, when called by this task's
	 * group leader. The second argument is the pointer returned by the call to
	 * init_task by this task. The unsigned long argument is the number of
	 * iterations this workload should complete of whatever work it is doing. The
	 * word 'iterations' is used somewhat loosely - the main thing that matters is
	 * that the relationship between the number passed in and the amount of work
	 * done when calling this function must be linear. It is preferable that each
	 * 'iteration' take a fairly small amount of time (i.e. less than a
	 * microsecond, preferably 10ths of a microsecond, or even nanoseconds).
	 */
	void (*do_work) (void *, void *, unsigned long);
};

/*
 * Workload capability flags
 */
#define WORKLOAD_CAP_EMPTY 0	//no capabilities
#define WORKLOAD_CAP_TASK_WSS 1	//this workload uses task-specific data
#define WORKLOAD_CAP_GROUP_WSS 2	//this workload shares data with other tasks in its thread group

/*
 * Default min/max task and group WSS values, in bytes
 */
#define DEFAULT_MIN_TASK_WSS (1024*64)	//64k
#define DEFAULT_MAX_TASK_WSS (1024*1024*10)	//10Mb
#define DEFAULT_MIN_GROUP_WSS (1024*64)	//64k
#define DEFAULT_MAX_GROUP_WSS (1024*1024*10)	//10Mb

#endif				/* WORKLOAD_TYPE_H */
