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
#include <sys/resource.h>
#include <errno.h>

#include "../utils.h"
#include "../tester_types.h"
#include "../workload.h"
#include "../hardware.h"

#include "slope.h"

#define SLOPE_MEASURING_PRIORITY 98
#define POLLUTER_PRIORITY 99
#define INVALIDATOR_PRIORITY 99

/*
 * Sleep for @us microseconds, continuing to sleep if we were interrupted
 */
static void microsleep(unsigned int us)
{
	struct timespec sleep, remaining;
	int ret;

	sleep.tv_sec = 0;
	sleep.tv_nsec = us * 1000;

	do {
		ret = nanosleep(&sleep, &remaining);
		sleep = remaining;
	} while (ret && remaining.tv_nsec != 0);
}

/*
 * Function to pollute the cpu passed in with as much cache garbage as possible,
 * while sleeping for a few microseconds to let another task on this cpu get
 * *something* done.
 */
#define POLLUTER_ARRAY_SIZE DEFAULT_MAX_TASK_WSS/sizeof(int)
#define POLLUTER_ARRAY_STRIDE 64	//64b ~cache line size on most architectures

struct polluter_struct {
	int cpu;		//the cpu to run on
	int started;		//set this to 1 once we're officially polluting the waters
	int done;		//when this gets set to 1, exit
	unsigned int sleep_us;	//sleep for this many microseconds each time
};

static struct polluter_struct polluter = {.started = 0,.done = 0 };

static pthread_t polluter_thread;

void *pollute(void *arg)
{
	struct polluter_struct *p = (struct polluter_struct *)arg;
	unsigned long mask;
	struct sched_param param;
	int *array;

	array = (int *)malloc(sizeof(int) * POLLUTER_ARRAY_SIZE);
	if (!array)
		fatal_error("Failed to allocate memory "
			    "for cache polluter array");

	//pin thread to cpu
	MASK_ZERO(mask);	//clear the CPU mask
	MASK_SET(mask, p->cpu);	//add CPU 0 to the CPU mask
	sched_setaffinity(0, sizeof(mask), (cpu_set_t *) & mask);

	//set us as the highest real-time priority
	param.sched_priority = POLLUTER_PRIORITY;
	sched_setscheduler(0, SCHED_FIFO, &param);

	p->started = 1;
	while (!p->done) {
		int i;
		for (i = 0; i < POLLUTER_ARRAY_SIZE; i += POLLUTER_ARRAY_STRIDE)
			array[i]++;
		microsleep(p->sleep_us);
	}

	free(array);

	return 0;
}

static void start_polluter()
{
	//setup the polluter task on this processor and wait for it to start
	polluter.cpu = 0;
	polluter.done = 0;
	polluter.started = 0;
	polluter.sleep_us = SLOPE_GEN_INTERRUPT_EVERY;

	if (pthread_create(&polluter_thread, NULL, pollute, (void *)&polluter))
		fatal_error("Failed to pthread_create cache polluter task.");

	while (!polluter.started)
		microsleep(100);
}

static void stop_polluter()
{
	polluter.done = 1;
	if (pthread_join(polluter_thread, NULL))
		fatal_error("Failed to pthread_join cache polluter task.");
}

/*
 * Function to invalidate the cache lines with the data from the
 * slope-calculator thread's workload data and cause general bus contention.
 */
struct invalidator_struct {
	int cpu;
	int started;		//set this to 1 once we're officially polluting the waters
	int done;		//when this gets set to 1, exit
	struct workload *w;
	unsigned int task_wss, group_wss;
	void *workload_data, *workload_tg_data;
	pthread_t thread;
};
void *invalidate(void *arg)
{
	struct invalidator_struct *i = (struct invalidator_struct *)arg;
	unsigned long mask;
	struct sched_param param;

	//pin thread to cpu
	MASK_ZERO(mask);	//clear the CPU mask
	MASK_SET(mask, i->cpu);	//add CPU 0 to the CPU mask
	sched_setaffinity(0, sizeof(mask), (cpu_set_t *) & mask);

	//set us as the highest real-time priority
	param.sched_priority = INVALIDATOR_PRIORITY;
	sched_setscheduler(0, SCHED_FIFO, &param);

	i->started = 1;
	while (!i->done)
		i->w->do_work(i->workload_tg_data, i->workload_data,
			      DEFAULT_AVG_SLOPE_ITERS);

	return 0;
}

#define MAX_INVALIDATORS 4
static struct invalidator_struct invalidators[MAX_INVALIDATORS];
static int active_invalidators = 0;

static void start_invalidators(struct workload *w, unsigned int task_wss,
			       unsigned int group_wss, void *workload_tg_data)
{
	int i;
	int num_cpus = get_num_processors();

	if (num_cpus - 1 > MAX_INVALIDATORS)
		active_invalidators = MAX_INVALIDATORS;
	else
		active_invalidators = num_cpus - 1;

	for (i = 0; i < active_invalidators; i++) {
		struct invalidator_struct *inv = &invalidators[i];
		//place invalidators on evenly-spaced processors starting at the opposite end
		//of the range to try to hit processors which don't share any cache
		inv->cpu =
		    (num_cpus - 1) - i * (num_cpus - 1) / active_invalidators;
		inv->done = 0;
		inv->started = 0;
		inv->w = w;
		inv->task_wss = task_wss;
		inv->group_wss = group_wss;
		inv->workload_tg_data = workload_tg_data;
		if (w->init_task)
			inv->workload_data =
			    w->init_task(workload_tg_data, task_wss);

		if (pthread_create(&inv->thread, NULL, invalidate, (void *)inv))
			fatal_error("Failed to pthread_create "
				    "cache invalidator task.");

		while (!inv->started)
			microsleep(100);
	}
}

static void stop_invalidators()
{
	int i;
	for (i = 0; i < active_invalidators; i++) {
		struct invalidator_struct *inv = &invalidators[i];
		inv->done = 1;

		if (pthread_join(inv->thread, NULL))
			fatal_error("Failed to pthread_join "
				    "cache invalidator task.");

		if (inv->w->cleanup_task)
			inv->w->cleanup_task(inv->workload_tg_data,
					     inv->workload_data);
	}
}

/*
 * Execute the workload for a certain number of iterations, and return the
 * number of nanoseconds it took.
 */
static unsigned long measure_run(struct workload *w, unsigned long iterations,
				 void *workload_data, void *workload_tg_data)
{
	struct timespec starttime, endtime;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &starttime);
	w->do_work(workload_tg_data, workload_data, iterations);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endtime);

	//convert the difference in times to nanoseconds and return it
	return timespec_subtract_ns(&starttime, &endtime);
}

/*
 * Calculate the average slope for a workload at a particular WSS
 */
static double calc_slope_average(int workload, unsigned int task_wss,
				 unsigned int group_wss, int verbose, int iters)
{
	struct workload *w = get_workload_struct(workload);
	void *workload_data = 0, *workload_tg_data = 0;
	double sum, estimated_slope;
	unsigned long run_nsec;
	int i, num_runs;

	//initialize the per-thread-group data section of the workload
	if (w->init_group)
		workload_tg_data = w->init_group(group_wss);

	//initialize the per-task data section of the workload
	if (w->init_task)
		workload_data = w->init_task(workload_tg_data, task_wss);

	//run workload once for a fairly large number of iterations, and use this
	//number of iterations to estimate a very loose slope number
	start_invalidators(w, task_wss, group_wss, workload_tg_data);
	run_nsec = measure_run(w, DEFAULT_AVG_SLOPE_ITERS,
			       workload_data, workload_tg_data);
	estimated_slope = (1000.0 * DEFAULT_AVG_SLOPE_ITERS) / run_nsec;	//one thousand is the conversion from nanoseconds to microseconds
	stop_invalidators();

	//initialize the overall sum
	sum = 0;
	num_runs = 0;

	//run the workload a bunch of times, and take the worst of the resulting slopes
	for (i = 0; i < iters; i++) {
		double slope;
		unsigned long iterations =
		    (SLOPE_GEN_MIN_TIME +
		     i * ((SLOPE_GEN_MAX_TIME - SLOPE_GEN_MIN_TIME) /
			  iters)) * estimated_slope;
		num_runs++;

		start_invalidators(w, task_wss, group_wss, workload_tg_data);
		run_nsec = measure_run(w, iterations,
				       workload_data, workload_tg_data);
		stop_invalidators();

		slope = (1000.0 * iterations) / run_nsec;
		sum += slope;
		estimated_slope = sum / num_runs;
		if (verbose) {
			printf("\t\titers: %ld, us: %ld, "
			       "slope: %f (average: %f)\n",
			       iterations, run_nsec / 1000,
			       slope, estimated_slope);
		}

		microsleep(250 * 1000);	//sleep for 1/4 second
	}

	//cleanup the per-task data section of the workload
	if (w->cleanup_task)
		w->cleanup_task(workload_tg_data, workload_data);

	//cleanup the per-task data section of the workload
	if (w->cleanup_group)
		w->cleanup_group(workload_tg_data);

	return sum / num_runs;
}

/*
 * Calculate the WCET slope for a workload at a particular WSS
 */
#define UNDERESTIMATE_BY 0.01	//take 1% off worst measured time for WCET
static double calc_slope_wcet(int workload, unsigned int task_wss,
			      unsigned int group_wss, int verbose, int iters)
{
	struct workload *w = get_workload_struct(workload);
	void *workload_data = 0, *workload_tg_data = 0;
	unsigned long run_nsec;
	double estimated_slope, wcet_slope;
	int i;

	//initialize the per-thread-group data section of the workload
	if (w->init_group)
		workload_tg_data = w->init_group(group_wss);

	//initialize the per-task data section of the workload
	if (w->init_task)
		workload_data = w->init_task(workload_tg_data, task_wss);

	//run workload once for a fairly large number of iterations, and use this
	//number of iterations to estimate a very loose slope number
	start_invalidators(w, task_wss, group_wss, workload_tg_data);
	start_polluter();	//start the polluter task
	run_nsec = measure_run(w, DEFAULT_WCET_SLOPE_ITERS,
			       workload_data, workload_tg_data);
	estimated_slope = (1000.0 * DEFAULT_WCET_SLOPE_ITERS) / run_nsec;	//one thousand is the conversion from nanoseconds to microseconds
	stop_polluter();	//stop the polluter task
	stop_invalidators();

	//initialize the overall sum and initial WCET slope value
	wcet_slope = -1.0;

	//run the workload a bunch of times, and take the worst of the resulting slopes
	for (i = 0; i < iters; i++) {
		double slope;
		unsigned long iterations =
		    (SLOPE_GEN_MIN_TIME +
		     i * ((SLOPE_GEN_MAX_TIME - SLOPE_GEN_MIN_TIME) /
			  iters)) * estimated_slope;

		start_invalidators(w, task_wss, group_wss, workload_tg_data);
		start_polluter();	//start the polluter task
		run_nsec = measure_run(w, iterations,
				       workload_data, workload_tg_data);
		stop_polluter();	//stop the polluter task
		stop_invalidators();

		slope = (1000.0 * iterations) / run_nsec;
		if (slope < wcet_slope || wcet_slope < 0.0)
			wcet_slope = slope;
		if (verbose) {
			printf("\t\titers: %ld, us: %ld, "
			       "slope: %f (worst: %f)\n",
			       iterations, run_nsec / 1000, slope, wcet_slope);
		}

		microsleep(250 * 1000);	//sleep for 1/4 second
	}

	//cleanup the per-task data section of the workload
	if (w->cleanup_task)
		w->cleanup_task(workload_tg_data, workload_data);

	//cleanup the per-task data section of the workload
	if (w->cleanup_group)
		w->cleanup_group(workload_tg_data);

	//decrease this final slope by 1% just to be on the safe side
	return wcet_slope * (1 - UNDERESTIMATE_BY);
}

/*
 * Find the average and worst-case slopes. Slope is defined as the number of
 * iterations per microsecond. For example, a slope of 10 means that this
 * particular workload on this particular machine can always execute 10
 * iterations in less than one microsecond.
 */
static void update_slopes(const int workload, const int verbose)
{
	//scheduling/affinity stuff
	int prev_scheduler, prev_prio;
	struct sched_param param;
	unsigned long prev_mask, mask;
	int no_wss_variation, i;
	struct workload *w = get_workload_struct(workload);

	//Should all the slopes in the loop be combined? this is true for any
	//workload which doesn't have a variable WSS.
	no_wss_variation = !(w->capabilities &
			     (WORKLOAD_CAP_TASK_WSS | WORKLOAD_CAP_GROUP_WSS));

	//clear the slope files so we don't have old values hanging around
	clear_workload_slope(workload, TIMING_AVERAGE);

	//get the current affinity for this process
	sched_getaffinity(0, sizeof(prev_mask), (cpu_set_t *) & prev_mask);
	//pin process to CPU 0
	MASK_ZERO(mask);	//clear the CPU mask
	MASK_SET(mask, 0);	//add CPU 0 to the CPU mask
	sched_setaffinity(0, sizeof(mask), (cpu_set_t *) & mask);

	//get the current scheduler and priority
	prev_scheduler = sched_getscheduler(0);	//get the previous scheduler, so we can go back to that when we're done
	prev_prio = getpriority(PRIO_PROCESS, 0);
	//set our real-time priority
	param.sched_priority = SLOPE_MEASURING_PRIORITY;
	sched_setscheduler(0, SCHED_FIFO, &param);

	//initialize the workload global data
	if (w->init_global)
		w->init_global();

	//loop through all values for working set size, getting the slope at each value
	for (i = 0; i < SLOPE_GEN_WSS_SAMPLES; i++) {
		unsigned int task_wss, group_wss;
		double average_slope;
		int iters = SLOPE_GEN_SAMPLES;

		//if we're running a workload which doesn't support for different
		//WSS's, it doesn't make sense to run it with them
		if (no_wss_variation)
			iters *= SLOPE_GEN_WSS_SAMPLES;

		task_wss = w->min_task_wss +
		    i * (w->max_task_wss - w->min_task_wss) /
		    SLOPE_GEN_WSS_SAMPLES;
		group_wss = w->min_group_wss +
		    i * (w->max_group_wss - w->min_group_wss) /
		    SLOPE_GEN_WSS_SAMPLES;

		average_slope = calc_slope_average(workload, task_wss,
						   group_wss, verbose, iters);
		if (put_workload_slope(workload, TIMING_AVERAGE,
				       average_slope, task_wss, group_wss))
			fatal_error("Unable to write average slope");

		//break if we're not doing any WSS variations
		if (no_wss_variation)
			break;
	}

	//tear down the global workload data
	if (w->cleanup_global)
		w->cleanup_global();

	//set affinity to what it was before we set it to CPU 0
	sched_setaffinity(0, sizeof(prev_mask), (cpu_set_t *) & prev_mask);

	//set priority and scheduler to what it was before
	param.sched_priority = prev_prio;
	sched_setscheduler(0, prev_scheduler, &param);
}

/*
 * Check the slopes for this workload. If force is zero (false), only
 * regenerate the slope value if it is not already defined. If force is true,
 * regenerate all the slopes for this workload.
 */
void check_slopes(int workload, const int force, const int verbose)
{
	const char *workload_name = get_workload_name(workload);
	double average_slope;

	//if the slope is not accessible, complain about access errors
	if (!workload_slope_file_accessible(workload, TIMING_AVERAGE)) {
		fatal_error("Could not access the slope files. "
			    "Check to make sure you are running as root (or sudo).");
	}
	//get the existing slopes if they do, in fact, exist
	average_slope =
	    get_workload_slope(workload, TIMING_AVERAGE, DEFAULT_MIN_TASK_WSS,
			       DEFAULT_MIN_GROUP_WSS);

	if (average_slope <= 0.0 || force) {
		printf("Updating slopes for workload: %s...\n", workload_name);
		update_slopes(workload, verbose);
		printf("Complete.\n");
	} else {
		printf("Slopes already calculated for workload: %s\n",
		       workload_name);
	}
}
