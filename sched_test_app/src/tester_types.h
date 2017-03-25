/***************************************************************************
 *   Copyright (C) 2009-2012 Virginia Tech Real-Time Systems Lab           *
 *                                                                         *
 *   Written by Aaron Lindsay                                              *
 *   aaron@aclindsay.com                                                   *
 *                                                                         *
 *   Based on code written by Matthew Dellinger                            *
 *   mdelling@vt.edu                                                       *
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

#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <chronos/chronos.h>
#include <chronos/chronos_aborts.h>

#include "utils.h"
#include "salloc.h"

#ifndef TESTER_TYPES_H
#define TESTER_TYPES_H

//constants which specify the workload timing method
#define TIMING_AVERAGE 0
#define TIMING_WCET    1
#define TIMING_TIMER   2

//defines for output formatting
#define OUTPUT_LOG     0
#define OUTPUT_EXCEL   1
#define OUTPUT_GNUPLOT 2
#define OUTPUT_VERBOSE 3

//defines for locking type and patterns
#define NO_LOCKING     0
#define LOCKING        1
//2 intentionally skipped - see next line
#define NESTED_LOCKING 3	//(can be logically or-ed w/ LOCKING and will still be true, since NESTED_LOCKING is a superset of LOCKING)

/*
 * Holds data necessary for each thread group in our internal mini threading library
 */
typedef struct tgroup_data {
	pid_t pid;
} tgroup_t;

/*
 * Struct to hold all the command-line options passed in.
 */
struct test_app_opts {
//required fields
	int output_format;	//One of OUTPUT_LOG, OUTPUT_EXCEL, OUTPUT_GNUPLOT, or OUTPUT_VERBOSE
	int cpu_usage;		//percent of each task's exec time to actually execute it for
	int run_time;		//total time to run the test for (in seconds)
	int scheduler;		//real-time scheduling algorithm to use
	char *taskset_filename;	//filename of the taskset file

//optional options
	int workload;		//one of the workload numbers
	int timing_method;	//one of the timing method
	int priority_inheritance;	//enable priority inheritance
	int enable_hua;		//enable HUA abort handlers
	int deadlock_prevention;	//enable deadlock-prevention
	int no_run;		//don't run the test, just find the hyper-period

	int locking;		//enable locking. One of NO_LOCKING, LOCKING, NESTED_LOCKING.
	int cs_length;		//lock critical section length (as a percentage of the total execution time of tasks)

//batch mode options
	int batch_mode;		//off by default (0)
	int end_usage;		//the usage to stop at for batch mode (set same as cpu_usage for non-batch mode)
	int interval;		//how much to increment for each iteration/interval in batch mode
};

/*
 * The struct which holds all the data needed for each task being executed.
 */
struct task {
	struct test *tester;	//link to the tester struct which owns us

	union {
		pthread_t p;
		tgroup_t tg;
	} thread;		//the thread that we run in

	char *abort_pointer;	//the pointer this task should check to see if it has been aborted

	void *workload_data;	//pointer initialized by a workload to hold task-specific data for that workload
	void *workload_tg_data;	//pointer initialized by a workload to hold thread group-specific data for that workload

	double cached_slope;	//cached slope for current workload (don't want to fetch it from disk every period)

	struct task *next;	//next task in linked list
	int task_id, thread_id;
	int num_my_locks;
	chronos_mutex_t **my_locks;	//array where the first num_my_locks elements are the indices of locks we must lock

	unsigned long cpu_mask;
	unsigned int thread_group;
	struct task *group_leader;

	//real-time properties set from taskset file
	unsigned long period;
	struct timespec period_ts;	//same as period, except in timespec form. This is needed to pass it in to the syscall.
	unsigned long exec_time;
	unsigned int utility;
	unsigned int hua_utility;
	long task_wss;		//size of this task's WSS in bytes. -1 if unspecified.
	long group_wss;		//size of the memory shared by tasks in this group in bytes (only valid if this task is a group leader)

	//variables controlling execution - initialized from a combination of the taskset file parameters and the command-line arguments
	unsigned long locked_usage;	//microseconds to burn while locked
	unsigned long unlocked_usage;	//microseconds to burn while unlocked
	int max_releases;	//the number of releases this task will have
	int extra_release;	//true if this task needs an extra release, where the runtime statistics are not calculated

	//actual execution statistics
	struct timespec local_start_time;	//the time this thread observes it started its thread
	unsigned int num_releases;
	unsigned int num_aborted;
	unsigned int deadlines_met;
	unsigned int utility_accrued;
	long max_tardiness;
};

/*
 * Malloc and initialize a new task struct.
 */
static inline struct task *new_task(struct test *tester)
{
	struct task *t = (struct task *)salloc(sizeof(struct task));
	if (!t) {
		printf("Error allocating memory. Exiting.\n");
		exit(1);
	}
	t->tester = tester;
	t->next = 0;
	t->task_id = 0;
	t->thread_id = 0;
	t->my_locks = 0;
	MASK_ZERO(t->cpu_mask);
	t->thread_group = 0;
	t->group_leader = 0;
	t->period = 0;
	t->exec_time = 0;
	t->utility = 0;
	t->hua_utility = 0;
	t->task_wss = -1;
	t->group_wss = -1;
	t->num_releases = 0;
	t->num_aborted = 0;
	t->deadlines_met = 0;
	t->utility_accrued = 0;
	t->max_tardiness = 0;
	return t;
}

/*
 * Clear a task's runtime statistics. This is called before a new run to make sure we don't have residual statistics from a previous run.
 */
static inline void clear_task_stats(struct task *t)
{
	t->num_releases = 0;
	t->num_aborted = 0;
	t->deadlines_met = 0;
	t->utility_accrued = 0;
	t->max_tardiness = 0;
}

/*
 * Struct to hold the global data for the test application.
 */
struct test {
	struct test_app_opts *options;

	struct workload *workload;	//the current workload being executed

	struct task *task_list;	//initial list (linked by next pointer)
	struct task **tasks;	//later, we build an array for constant-time access
	unsigned int num_tasks;	//number of tasks in tasks array

	chronos_mutex_t *locks;	//array of locks
	int num_locks;		//number of locks in locks array

	unsigned long lock_time;	//the amount of time it takes (in microseconds) to pthread_create, lock a lock, unlock it, and pthread_join

	chronos_aborts_t abort_data;

	unsigned int num_processors;
	unsigned long *domain_masks;

	pthread_barrier_t *barrier;
	struct timespec *global_start_time;

	/* System level performance statistics */
	int sys_total_release;	// The total number of tasks
	int sys_met_release;	// The number of tasks that met deadlines
	int sys_total_util;	// The total utility of all tasks
	int sys_met_util;	// The total utility of all tasks that met deadlines
	int sys_abort_count;	// The number of threads aborted
	long max_tardiness;	// The highest tardiness of any task
};

#endif				/*TESTER_TYPES_H */
