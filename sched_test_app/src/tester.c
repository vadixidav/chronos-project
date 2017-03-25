/***************************************************************************
 *   Copyright (C) 2009-2012 Virginia Tech Real-Time Systems Lab           *
 *                                                                         *
 *   Original version written by Matthew Dellinger                         *
 *   mdelling@vt.edu                                                       *
 *                                                                         *
 *   Re-written by Aaron Lindsay                                           *
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

#include <sys/resource.h>

#include "tester.h"
#include "salloc.h"
#include "hardware.h"

/*
 * This is the one copy of the test struct which gets passed around everywhere
 * to communicate options, global variables to the pieces of the application
 * which need them.
 */
static struct test tester = {
	.options = 0,
	.workload = 0,
	.task_list = 0,
	.tasks = 0,
	.num_tasks = 0,
	.locks = 0,
	.num_locks = -1,
	.num_processors = 0,
	.domain_masks = 0,
	.global_start_time = 0
};

/*
 * Initialize all the locks for the taskset.
 */
static void initialize_test_locks()
{
	int i;
	tester.locks =
	    (chronos_mutex_t *) salloc(sizeof(chronos_mutex_t) *
				       tester.num_locks);
	if (!tester.locks)
		fatal_error("Failed to allocate memory.");
	for (i = 0; i < tester.num_locks; i++)
		chronos_mutex_init(&tester.locks[i]);

	if (tester.num_locks && !(tester.options->locking & LOCKING))
		warning("Locking not enabled, "
			"but the taskset file specifies locks. Ignoring locks.");

	if (!tester.num_locks && (tester.options->locking & LOCKING)) {
		warning("Locking enabled, "
			"but the taskset file doesn't specify locks. Turning locking off.");
		tester.options->locking = NO_LOCKING;
	}
}

static void cleanup_test_locks()
{
	sfree(tester.locks);
	tester.locks = 0;
	tester.num_locks = 0;
}

/*
 * Read in the taskset file, initializing data structures as we go.
 * This function handles initializing the test struct as well as all the
 * task structs, locks, scheduling domain maps, period, utilization, etc.
 */
static void init_tasks(char *taskset_filename)
{
	int ret, i, seen_group;
	int c;
	struct task *curr;
	FILE *f;

	//get the number of processors and create that many domain_masks
	tester.num_processors = get_num_processors();

	//initialize domain masks
	tester.domain_masks =
	    (unsigned long *)salloc(sizeof(unsigned long) *
				    tester.num_processors);
	if (!tester.domain_masks)
		fatal_error("Failed to allocate memory.");
	for (i = 0; i < tester.num_processors; i++) {
		MASK_ZERO(tester.domain_masks[i]);
	}

	//open the file from the passed-in filename
	f = fopen(taskset_filename, "r");
	if (!f)
		fatal_error("Failed to open taskset file.");

	//read lines from the file and initialize locks/tasks as we go
	seen_group = 0;
	while ((c = getc(f)) != EOF) {
		//ignore comments
		if (c == '#') {
			//snarf rest of line
			while (c != EOF && c != '\n')
				c = getc(f);

			//read the line which specifies the number of locks (only one such line is allowed)
		} else if (c == 'L' && tester.num_locks == -1) {
			ret = fscanf(f, " %d ", &tester.num_locks);
			if (ret == 0 || ret == EOF)
				fatal_error("Ill-formed taskset file: "
					    "number of locks on 'L' line improperly formatted.");
			initialize_test_locks();

			//read lines which specify a task in the taskset
		} else if (c == 'T') {
			if (tester.num_locks < 0)	//number of locks must preceed first task
				fatal_error("Ill-formed taskset file: "
					    "the number of locks must preceed the first task definition.");
			if (seen_group)
				fatal_error("Ill-formed taskset file: "
					    "no task definition may follow any group line.");
			init_task(&tester, f);
		} else if (c == 'G') {
			seen_group = 1;
			init_group(&tester, f);
		} else
			fatal_error("Ill-formed taskset file: "
				    "line doesn't begin with either '#', 'T', 'L', or 'G'.");
	}

	fclose(f);

	if (tester.num_locks < 0)
		fatal_error("Number of locks not found in taskset file.");

	if (tester.num_tasks <= 0)
		fatal_error("No tasks found in taskset file");

	//create an array from the task list so access to the tasks is constant-time
	tester.tasks =
	    (struct task **)salloc(sizeof(struct task *) * tester.num_tasks);
	if (!tester.tasks)
		fatal_error("Failed to allocate memory.");
	//go through the list of tasks and point the array elements at them
	i = 0;
	curr = tester.task_list;
	while (curr) {
		tester.tasks[i] = curr;
		curr = curr->next;
		i++;
	}

	//initialize the abort device
	if (init_aborts(&tester.abort_data))
		fatal_error("Failed to initialize abort device.");
}

static void cleanup_tasks()
{
	int i;

	sfree(tester.domain_masks);
	tester.domain_masks = 0;

	for (i = 0; i < tester.num_tasks; i++) {
		if (tester.tasks[i]->my_locks)
			sfree(tester.tasks[i]->my_locks);
		sfree(tester.tasks[i]);
	}
	tester.task_list = 0;
	sfree(tester.tasks);
	tester.tasks = 0;
	tester.num_tasks = 0;
}

/*
 * Zero out the counters before the next run
 */
static void clear_counters()
{
	tester.sys_total_release = 0;
	tester.sys_met_release = 0;
	tester.sys_total_util = 0;
	tester.sys_met_util = 0;
	tester.sys_abort_count = 0;
	tester.max_tardiness = 0;
}

/*
 * Get the scheduler constant needed to pass into the call to set_scheduler,
 * combining the constant for the scheduler itself, as well as other scheduling
 * features selected by the same constant.
 */
static int get_scheduler(struct test_app_opts *options)
{
	int sched = options->scheduler;
	if (options->priority_inheritance)
		sched |= SCHED_FLAG_PI;
	if (options->enable_hua)
		sched |= SCHED_FLAG_HUA;
	if (options->deadlock_prevention)
		sched |= SCHED_FLAG_NO_DEADLOCKS;
	return sched;
}

static int get_priority(struct test_app_opts *options)
{
	if (options->scheduler & SCHED_GLOBAL_MASK)
		return TASK_RUN_PRIO;
	else
		return -1;
}

/* Calculate the time required to lock and unlock a resource */
void *get_rt_lock_time(void *p)
{
	chronos_mutex_t r;

	struct sched_param param;
	param.sched_priority = TASK_RUN_PRIO;

	sched_setscheduler(0, SCHED_FIFO, &param);

	chronos_mutex_init(&r);
	chronos_mutex_lock(&r);
	chronos_mutex_unlock(&r);
	chronos_mutex_destroy(&r);

	return NULL;
}

/*
 * Calculate the time required to lock and unlock a resource in a real-time task.
 */
void calc_lock_time()
{
	pthread_t t1;
	struct timespec start_time, end_time;
	unsigned long time = 0, temp_time = 0;
	struct sched_param param;
	int i, prio;

	prio = getpriority(PRIO_PROCESS, 0);

	param.sched_priority = MAIN_PRIO;
	sched_setscheduler(0, SCHED_FIFO, &param);

	for (i = 0; i < 100; i++) {
		clock_gettime(CLOCK_REALTIME, &start_time);
		pthread_create(&t1, NULL, get_rt_lock_time, NULL);
		pthread_join(t1, NULL);
		clock_gettime(CLOCK_REALTIME, &end_time);

		temp_time = timespec_subtract_ns(&start_time, &end_time);
		if (temp_time > time)
			time = temp_time;
	}

	tester.lock_time = time / THOUSAND;
	param.sched_priority = prio;
	sched_setscheduler(0, SCHED_OTHER, &param);
}

/*
 * Print statistics from the last run of the tester.
 */
static void print_results()
{
	if (tester.options->output_format == OUTPUT_VERBOSE) {
		printf("set start_time: %ld sec %ld nsec\n",
		       tester.global_start_time->tv_sec,
		       tester.global_start_time->tv_nsec);
		printf("set end_time: %ld sec %ld nsec\n",
		       tester.global_start_time->tv_sec +
		       tester.options->run_time,
		       tester.global_start_time->tv_nsec);
		printf("total tasks: %d,", tester.sys_total_release);
		printf("total deadlines met: %d,", tester.sys_met_release);
		printf("total possible utility: %d,", tester.sys_total_util);
		printf("total utility accrued: %d,", tester.sys_met_util);
		printf("total tasks aborted: %d\n", tester.sys_abort_count);
	} else if (tester.options->output_format == OUTPUT_EXCEL) {
		char *sched_name = get_sched_name(tester.options->scheduler);
		if (sched_name)
			printf("%s,", sched_name);
		printf("%d,", tester.options->cpu_usage);
		printf("=%d/%d,", tester.sys_met_release,
		       tester.sys_total_release);
		printf("=%d/%d,", tester.sys_met_util, tester.sys_total_util);
		printf("=%ld\n", tester.max_tardiness);
	} else if (tester.options->output_format == OUTPUT_GNUPLOT) {
		printf("%f %f %f %ld\n", ((double)tester.options->cpu_usage) / 100, ((double)tester.sys_met_release) / ((double)tester.sys_total_release),	//deadline satisfaction ratio
		       ((double)tester.sys_met_util) / ((double)tester.sys_total_util),	//accrued utility ratio
		       tester.max_tardiness);
	} else {
		char *sched_name = get_sched_name(tester.options->scheduler);
		printf("%.2f", ((double)tester.options->cpu_usage) / 100);
		if (sched_name)
			printf("/%s", sched_name);
		printf(": Tasks: %d/%d, Utility: %d/%d, "
		       "Aborted: %d, Tardiness %ld\n", tester.sys_met_release,
		       tester.sys_total_release, tester.sys_met_util,
		       tester.sys_total_util, tester.sys_abort_count,
		       tester.max_tardiness);
	}
}

/*
 * Setup for and run one complete run of a taskset in the test application. This
 * includes initializing the real-time priorities, spawning all threads, joining
 * them, and tabulating their statistics, and printing the results.
 */
static void run()
{
	int i;
	struct sched_param param, old_param;
	unsigned long main_mask = 0;
	pthread_barrierattr_t barrierattr;

	clear_counters();	//clear performance counters

	pthread_barrierattr_setpshared(&barrierattr, PTHREAD_PROCESS_SHARED);	//allow access to this barrier from any process with access to the memory holding it
	pthread_barrier_init(tester.barrier, &barrierattr, tester.num_tasks);	//initialize barrier

	//set outselves as a real-time task
	sched_getparam(0, &old_param);
	param.sched_priority = MAIN_PRIO;
	if (sched_setscheduler(0, SCHED_FIFO, &param) == -1)
		fatal_error("sched_setscheduler() failed.");

	//set task affinity of this main thread to the first processor
	MASK_ZERO(main_mask);
	MASK_SET(main_mask, 0);
	if (sched_setaffinity(0, sizeof(main_mask),
			      (cpu_set_t *) & main_mask) < 0)
		fatal_error("sched_setaffinity() failed.");

	//set up the correct scheduler on all the domains we need
	for (i = 0; i < tester.num_processors &&
	     tester.domain_masks[i] != 0; i++) {
		if (set_scheduler(get_scheduler(tester.options),
				  get_priority(tester.options),
				  tester.domain_masks[i]))
			fatal_error("Selection of RT scheduler failed! "
				    "Is the scheduler loaded?");
	}

	//some debugging/verbose info
	if (tester.options->output_format == OUTPUT_VERBOSE) {
		printf("Percent of each task's exec time to use: %f\n",
		       ((double)tester.options->cpu_usage) / 100);
		printf("Critical section length: %d\n",
		       tester.options->cs_length);
	}
	//generate timing info for each task
	for (i = 0; i < tester.num_tasks; i++) {
		clear_task_stats(tester.tasks[i]);
		set_lock_usage(tester.tasks[i], &tester);
		calculate_releases(tester.tasks[i], &tester);

		if (tester.options->output_format == OUTPUT_VERBOSE)
			printf("period: %ld usec \t "
			       "usage: %ld unlocked + %ld locked usec\n",
			       tester.tasks[i]->period,
			       tester.tasks[i]->unlocked_usage,
			       tester.tasks[i]->locked_usage);
	}

	//initialize the global_start_time pointer to 0 so it can be reset again by the 'winning' thread
	tester.global_start_time = 0;

	//start all the thread groups
	for (i = 0; i < tester.num_tasks; i++) {
		if (group_leader(tester.tasks[i]))
			if (tgroup_create(&tester.tasks[i]->thread.tg,
					  start_task_group,
					  (void *)tester.tasks[i]))
				fatal_error("Failed to tgroup_create "
					    "one of the task group leader threads.");

	}

	//wait until all the thread groups are done
	for (i = 0; i < tester.num_tasks; i++) {
		if (group_leader(tester.tasks[i])) {
			int ret, status = 0;
			ret = tgroup_join(&tester.tasks[i]->thread.tg, &status);
			if (ret)
				fatal_error("Failed to tgroup_join "
					    "one of the task group leader processes.");
			if (status)
				fatal_error("tgroup_join joined task group "
					    "leader process with non-zero status.");
		}
	}

	//return us to a normal scheduler and priority
	sched_setscheduler(0, SCHED_OTHER, &old_param);

	//accumulate statistics from individual tasks
	for (i = 0; i < tester.num_tasks; i++) {
		long tardiness;
		tester.sys_total_release += tester.tasks[i]->max_releases;
		tester.sys_met_release += tester.tasks[i]->deadlines_met;
		tester.sys_total_util +=
		    tester.tasks[i]->max_releases * tester.tasks[i]->utility;
		tester.sys_met_util += tester.tasks[i]->utility_accrued;
		tester.sys_abort_count += tester.tasks[i]->num_aborted;

		//if this task's tardiness is less than the current maximum, update it
		//Note: negative tardiness is 'greater' because tardiness is calculated as (deadline - end_time)
		tardiness = tester.tasks[i]->max_tardiness;
		if (tardiness < tester.max_tardiness)
			tester.max_tardiness = tardiness;
	}

	print_results();	//display all statistics corresponding to the output options

	pthread_barrier_destroy(tester.barrier);	//destroy barrier
}

/*
 * Given the options passed in on the command line, initialize the application
 * and call each of the individual runs (will only be one if batch mode is not
 * enabled).
 */
void run_test(struct test_app_opts *options)
{
	tester.options = options;

	//intiailize the memory for the barrier
	tester.barrier = salloc(sizeof(pthread_barrier_t));
	if (!tester.barrier)
		fatal_error("pthread_barrier_t memory allocation failed.");

	tester.workload = get_workload_struct(tester.options->workload);
	workload_init_global(&tester);	// initialize any global state the current workload has (allocating global memory, etc.)

	//initialize tasks based on the taskset file
	init_tasks(options->taskset_filename);

	//if requested, find the hyper-period and return
	if (tester.options->no_run) {
		printf("No run (-z) flag enabled\n");
		printf("Hyper-period is: %ld seconds\n",
		       taskset_lcm(&tester) / MILLION);
		return;
	}

	if (options->enable_hua)
		calc_lock_time();	//calculate how long locking takes (needed to calculate HUA abort handler timing information)

	//actually run the tests
	for (; options->cpu_usage <= options->end_usage;
	     options->cpu_usage += options->interval) {
		run();
		if (options->cpu_usage < options->end_usage)
			sleep(1);
	}

	cleanup_test_locks();
	cleanup_tasks();
	sfree(tester.barrier);
}
