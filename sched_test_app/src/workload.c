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

#include "workload.h"
#include "task.h"

/*
 * Initialize any global memory, structs, or variables needed for this workload.
 */
void workload_init_global(struct test *tester)
{
	struct workload *w;
	assert(tester && tester->workload);
	w = tester->workload;
	if (w->init_global)
		w->init_global();
}

/*
 * Initialize any memory, structs, or variables needed for this workload which
 * are specific to the thread group of which this task is the leader.
 */
void workload_init_group(struct task *t)
{
	long group_wss;
	struct workload *w;
	assert(t && t->tester && t->tester->workload);
	assert(group_leader(t));

	w = t->tester->workload;
	group_wss = t->group_wss;

	if (w->init_group)
		t->workload_tg_data = w->init_group(group_wss);
}

/*
 * Initialize any memory, structs, or variables needed for this workload, which
 * are specific to this task, and not the workload as a whole.
 *
 * Also fetch the slope and store it in t->cached_slope so we don't have to wait
 * on fetching it from disk during a RT segment.
 */
void workload_init_task(struct task *t)
{
	long task_wss, group_wss;
	struct workload *w;
	assert(t && t->tester && t->tester->workload);

	task_wss = t->task_wss;
	group_wss = t->group_leader->group_wss;

	w = t->tester->workload;
	if (w->init_task)
		t->workload_data =
		    w->init_task(t->group_leader->workload_tg_data, task_wss);

	switch (t->tester->options->timing_method) {
	case TIMING_AVERAGE:
	case TIMING_TIMER:
		t->cached_slope =
		    get_workload_slope(t->tester->options->workload,
				       TIMING_AVERAGE, task_wss, group_wss);
		if (t->cached_slope <= 0.0)
			fatal_error("Could not open slope file "
				    "to read average slope.");
		break;
	case TIMING_WCET:
		t->cached_slope =
		    get_workload_slope(t->tester->options->workload,
				       TIMING_WCET, task_wss, group_wss);
		if (t->cached_slope <= 0.0)
			fatal_error("Could not open slope file "
				    "to read WCET slope.");
		break;
	default:
		fatal_error("Invalid timing method.\n");
	}
}

/*
 * Cleanup any global memory, structs, or variables needed for this workload.
 */
void workload_cleanup_global(struct test *tester)
{
	struct workload *w;
	assert(tester && tester->workload);
	w = tester->workload;
	if (w->cleanup_global)
		w->cleanup_global();
}

/*
 * Cleanup any memory, structs, or variables needed for this workload which
 * are specific to the thread group of which task is the leader.
 */
void workload_cleanup_group(struct task *t)
{
	struct workload *w;
	assert(t && t->tester && t->tester->workload);
	assert(group_leader(t));
	w = t->tester->workload;
	if (w->cleanup_group)
		w->cleanup_group(t->workload_tg_data);
	t->workload_tg_data = 0;
}

/*
 * Cleanup any memory, structs, or variables needed for this workload, which
 * are specific to this thread, and not the workload as a whole.
 */
void workload_cleanup_task(struct task *t)
{
	struct workload *w;
	assert(t && t->tester && t->tester->workload);
	w = t->tester->workload;
	if (w->cleanup_task)
		w->cleanup_task(t->group_leader->workload_tg_data,
				t->workload_data);
	t->workload_data = 0;
}

/*
 * The functions do_work_average, do_work_wcet, and do_work_timer are the three
 * implemented timing methods. These are the mechanisms used to *attempt* to
 * ensure that a particular workload is run for the 'correct' amount of time
 * according to a particular task's usage/utilization per period.
 */

/*
 * Execute a number of iterations based on the 'slope' and the usage (in
 * microseconds) passed in. Returns non-zero if aborted.
 */
#define ABORT_CHECK_US 1	//check the abort pointer every this many microseconds
static int do_work_slope(struct task *t, unsigned long usage, double exec_slope)
{
	/*
	 * Note: it is important that this operation is truncated, not rounded, because
	 * exec_slope could be WCET. We always want to underestimate, not overestimate.
	 */
	unsigned long iterations = usage * exec_slope;
	unsigned long iters_per_abort_check = exec_slope * ABORT_CHECK_US;
	unsigned long i;

	for (i = 0;
	     !(*t->abort_pointer) && (i + iters_per_abort_check < iterations);
	     i += iters_per_abort_check)
		t->tester->workload->do_work(t->group_leader->workload_tg_data,
					     t->workload_data,
					     iters_per_abort_check);

	if (!*t->abort_pointer && i < iterations)
		t->tester->workload->do_work(t->group_leader->workload_tg_data,
					     t->workload_data, iterations - i);

	return (*t->abort_pointer) != 0;	//return non-zero if this task has been aborted
}

/*
 * Execute a number of iterations based on the average-case 'slope' and the
 * usage (in microseconds) passed in. Returns non-zero if aborted.
 */
static int do_work_average(struct task *t, unsigned long usage)
{
	double exec_slope = t->cached_slope;
	return do_work_slope(t, usage, exec_slope);
}

/*
 * Execute a number of iterations based on the worst-case 'slope' and the
 * usage (in microseconds) passed in. Returns non-zero if aborted.
 */
static int do_work_wcet(struct task *t, unsigned long usage)
{
	double exec_slope = t->cached_slope;
	return do_work_slope(t, usage, exec_slope);
}

/*
 * Get the resolution of the CLOCK_THREAD_CPUTIME clock, in seconds.
 */
#define timespec_to_s(ts) ts.tv_sec + ts.tv_nsec * 0.000000001	/*nanoseconds to seconds: *10^-9 */
static inline double get_thread_cputime_res()
{
	static double thread_cputime_res = 0.0;
	struct timespec res;

	if (thread_cputime_res != 0.0)
		return thread_cputime_res;

	if (clock_getres(CLOCK_THREAD_CPUTIME_ID, &res))
		fatal_error("Error reading clock resolution.");
	thread_cputime_res = timespec_to_s(res);
	return thread_cputime_res;
}

/*
 * Get the time according to the CLOCK_THREAD_CPUTIME clock, in seconds.
 */
static inline double get_thread_cputime()
{
	struct timespec now;
	//TODO according to `man clock_gettime`, CLOCK_THERAD_CPUTIME_ID can be invalid when a process is migrated to another CPU. That sounds like a problem.
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now))
		fatal_error("Error reading clock.");
	return timespec_to_s(now);
}

/*
 * Execute iterations of the workload, periodically checking a timer to ensure
 * we don't execute for more than usage microseconds. Returns non-zero if
 * aborted.
 */
#define UNDERESTIMATE_BY 0.000005	//percentage of total execution time to underestimate execution time by
static int do_work_timer(struct task *t, unsigned long usage)
{
	int last_iters;
	double diff = 0, end_time, now, last = 0;

	//get average slope, and calculate the number of iterations per abort check based on that
	double exec_slope = t->cached_slope;
	unsigned long iters_per_abort_check = exec_slope * ABORT_CHECK_US;

	now = get_thread_cputime();
	end_time =
	    now +
	    (usage * 0.000001 /*microseconds to seconds: *10^-6 */ ) * (1.0 -
									UNDERESTIMATE_BY);

	while (!*t->abort_pointer
	       && now + diff + get_thread_cputime_res() < end_time) {
		t->tester->workload->do_work(t->group_leader->workload_tg_data,
					     t->workload_data,
					     iters_per_abort_check);

		last = now;
		now = get_thread_cputime();
		if (!last)
			diff = now - last;
		else
			diff = (diff + 2.0 * (now - last)) / 3.0;
	}

	//the loop didn't even execute once, bail out
	if (!diff)
		goto out;

	last_iters = (int)((end_time - now) / diff
			   * iters_per_abort_check * (1.0 - UNDERESTIMATE_BY));
	if (last_iters > 0)
		t->tester->workload->do_work(t->group_leader->workload_tg_data,
					     t->workload_data, last_iters);

 out:
	return (*t->abort_pointer) != 0;	//return non-zero if this task has been aborted
}

/*
 * This is the main workload function, which does work for 'usage' microseconds
 * according to the task's workload and timing mode. The return value is 0 if
 * the workload executes all the way through without being aborted, and non-zero
 * if the task has been aborted.
 */
int workload_do_work(struct task *t, unsigned long usage)
{
	assert(t && t->tester && t->tester->workload
	       && t->tester->workload->do_work);

	//see which timing control method we're using and call the do_work function specific to that timing method
	if (t->tester->options->timing_method == TIMING_AVERAGE)
		return do_work_average(t, usage);
	else if (t->tester->options->timing_method == TIMING_TIMER)
		return do_work_timer(t, usage);
	else if (t->tester->options->timing_method == TIMING_WCET)
		return do_work_wcet(t, usage);

	fatal_error("Invalid timing method.\n");	//if we got here, we have an invalid timing method, so die
	return 0;		//will never, ever get here due to the previous line, but it makes the compiler shut up
}
