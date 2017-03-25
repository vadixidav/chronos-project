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

#include <math.h>
#include <stdio.h>
#include <pthread.h>

#include <chronos/chronos.h>

#include "task.h"

/*
 * Update the locked and unlocked execution times for a task based on
 * the total execution time for the task, and the percentage of that
 * execution time we are supposed to hold the locks for (cs_length).
 * Note: We also must modify the execution time by 'usage' percentage if
 * we're not using the entire execution time.
 */
void set_lock_usage(struct task *t, struct test *tester)
{
	double usage = ((double)tester->options->cpu_usage) / 100;
	double cs = ((double)tester->options->cs_length) / 100;
	t->unlocked_usage = t->exec_time * (usage - (usage * cs));
	t->locked_usage = t->exec_time * usage * cs;
}

/*
 * Calculate the number of releases this task will have in the runtime specified.
 */
void calculate_releases(struct task *t, struct test *tester)
{
	//runtime is set to the running time of the overall test in microseconds (options->run_time is in seconds)
	unsigned int runtime = tester->options->run_time * MILLION;

	/*
	 * The number of releases this task will have is the floor of the total runtime
	 * divided by this task's period. I.e. the number of tasks which we gather
	 * statistics for is the highest number of tasks which will fit into the
	 * runtime without going over it.
	 */
	t->max_releases = runtime / t->period;

	/*
	 * Keep track of when this division has a remainder. If it does, we set
	 * extra_release so we know we have to run this task one more time to ensure
	 * other tasks have the proper amount of utility when they are finishing their
	 * runs.
	 */
	t->extra_release = ((runtime % t->period) != 0);
}

/*
 * Get the abort pointer for the current thread - what we can check periodically
 * to see if we have been aborted by the scheduler.
 */
static void setup_aborts(struct task *t)
{
	t->abort_pointer = get_abort_ptr(&t->tester->abort_data);
	if (!t->abort_pointer)
		fatal_error("Failed to initialize abort pointer for task.");
}

/*
 * Find the next deadline for this task (based on the start time for all tasks,
 * this task's period, and the job number of this task) and store the result in
 * the supplied timespec struct.
 */
static void find_job_deadline(struct task *t, struct timespec *deadline)
{
	unsigned long long nsec, carry;
	unsigned long offset = (t->num_releases + 1) * t->period;	//the offset from the start time this deadline is
	struct timespec *start_time = t->tester->global_start_time;	//the global start time for all tasks

	nsec = start_time->tv_nsec + (unsigned long long)(offset * THOUSAND);
	carry = nsec / BILLION;

	deadline->tv_nsec = nsec % BILLION;
	deadline->tv_sec = start_time->tv_sec + carry;
}

/*
 * Initialize the HUA abort handler for this task, if applicable.
 */
static void setup_hua_abort_handler(struct task *t)
{
	if (t->tester->options->enable_hua && t->hua_utility > 0) {
		add_abort_handler_selfnodeadline(t->hua_utility,
						 t->tester->lock_time *
						 t->tester->num_locks);
	}
}

/*
 * Execute the workload, lock/unlock locks, update runtime statistics, etc.
 * This function handles everything that needs to be done for a single instance
 * of a task (a job). count_stats is true if we should accrue statistics from
 * this run, and false if we are running this workload only to provide the
 * proper amount of concurrent utility while the other runs finish.
 */
static void task_instance(struct task *t, int count_stats)
{
	struct timespec deadline, end_time;
	long long tardiness;
	int aborted = 0;	//orred with the return value of the workload_do_work function calls

	find_job_deadline(t, &deadline);	//find the deadline for this taskset

	setup_hua_abort_handler(t);

	begin_rtseg_self(TASK_RUN_PRIO, t->utility, &deadline, &t->period_ts, t->unlocked_usage + t->locked_usage);	//enter real-time segment

	if (t->tester->options->locking & NESTED_LOCKING) {	//do nested locking, if applicable
		int last, lock_num;
		for (lock_num = 0; lock_num < t->num_my_locks; lock_num++) {
			if (chronos_mutex_lock(t->my_locks[lock_num]) == -1)
				break;
			else
				last = lock_num;
		}

		aborted |= workload_do_work(t, t->locked_usage);

		for (lock_num = last; lock_num >= 0; lock_num--) {
			chronos_mutex_unlock(t->my_locks[lock_num]);
		}

	} else if (t->tester->options->locking & LOCKING) {	//do non-nested locking, if applicable
		int lock_num;
		for (lock_num = 0; lock_num < t->num_my_locks; lock_num++) {
			if (chronos_mutex_lock(t->my_locks[lock_num]) == -1)
				break;
			aborted |= workload_do_work(t, t->locked_usage);
			chronos_mutex_unlock(t->my_locks[lock_num]);
		}
	}

	aborted |= workload_do_work(t, t->unlocked_usage);	//do unlocked workload time

	clock_gettime(CLOCK_REALTIME, &end_time);	//get the endtime

	end_rtseg_self(TASK_CLEANUP_PRIO);	//end the real-time segment

	/*
	 * Is this run for the sole purpose of making sure the other 'real' runs have
	 * the appropriate amount of concurrent utility? If it is, return and don't
	 * count the statistics for this run.
	 */
	if (!count_stats)
		return;

	tardiness = timespec_subtract_us(&deadline, &end_time);	//calculate tardiness from deadline and endtime

	//tabulate statistics about this run
	if (aborted) {		//have we been aborted?
		t->num_aborted++;
	} else if (tardiness >= 0) {	//otherwise, did we meet our deadline?
		t->deadlines_met++;	//increment deadlines_met, if we met ours
		t->utility_accrued += t->utility;	//add to utility_accrued however much we accrued
		//sleep for however much time remains before the next release of this task
		if (tardiness > 0)
			usleep(tardiness);
	} else {		//if we got here, we blew our deadline?
		//figure out if our tardiness was worse than anyone else's so far
		if (tardiness < t->max_tardiness)	//this is reverse from what it ought to be
			t->max_tardiness = tardiness;
	}
	//TODO maybe keep a figure on average tardiness? (would this be useful?)
}

/*
 * The function called from pthread_create to start a task. This function is
 * ultimately responsible for entering and exiting real-time segments, calling
 * the appropriate workload for the correct amount of time, and leaving the
 * segment, then recording scheduling statistics.
 */
void *start_task(void *arg)
{
	struct sched_param param;
	struct task *t = (struct task *)arg;

	//do some initialization before we actually signal we're ready to start our real-time task
	t->thread_id = gettid();	//get our thread's tid
	setup_aborts(t);	//grab our pointer which we can query to make sure we're not aborted yet

	//set the affinity of this thread to whatever was specified in the taskset file
	if (sched_setaffinity(0, sizeof(t->cpu_mask),
			      (cpu_set_t *) & t->cpu_mask))
		fatal_error("Failed to set processor affinity of a task.");

	workload_init_task(t);	//initialize any local data the workload needs

	//increase priority to TASK_START_PRIO
	param.sched_priority = TASK_START_PRIO;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

	pthread_barrier_wait(t->tester->barrier);	//wait on the barrier for all threads to arrive

	//get the start time and CAS on the start-time so all the threads agree on when they got here
	clock_gettime(CLOCK_REALTIME, &t->local_start_time);
	//Note: the next line is a gcc-specific CAS extension. This is NOT portable to a different compiler. (FIXME?)
	__sync_bool_compare_and_swap(&t->tester->global_start_time, 0,
				     &t->local_start_time);

	/* for each release of this task, call task_instance to do the heavy-lifting
	 * (actually execute the workload, lock/unlock locks, update runtime
	 * statistics, etc.)
	 */
	for (t->num_releases = 0; t->num_releases < t->max_releases;
	     t->num_releases++)
		task_instance(t, 1 /*count the statistics for this run */ );

	/*
	 * Run the task one period after it technically hit its last one, but don't
	 * measure the statistics. This makes sure all other tasks get the full load
	 * for all of their periods.
	 */
	if (t->extra_release)
		task_instance(t, 0 /*DON'T count the statistics */ );

	workload_cleanup_task(t);	//clean up any local data for the workload

	return NULL;
}

/*
 * Create all other tasks in our thread group, start our own task, then join the
 * other threads in our thread group before returning.
 */
int start_task_group(void *arg)
{
	int i;
	struct task *t = (struct task *)arg;
	struct test *tester = t->tester;

	workload_init_group(t);	//initialize the group-specific workload data for this group

	//start all the other threads in our group
	for (i = 0; i < tester->num_tasks; i++) {
		if (same_group(t, tester->tasks[i])
		    && !group_leader(tester->tasks[i]))
			if (pthread_create(&tester->tasks[i]->thread.p, NULL,
					   start_task,
					   (void *)tester->tasks[i]))
				fatal_error("Failed to pthread_create "
					    "one of the task group threads.");
	}

	start_task(arg);

	//wait until the other threads in our group are done
	for (i = 0; i < tester->num_tasks; i++) {
		if (same_group(t, tester->tasks[i])
		    && !group_leader(tester->tasks[i]))
			if (pthread_join(tester->tasks[i]->thread.p, NULL))
				fatal_error("Failed to pthread_join "
					    "one of the task group threads.");
	}

	workload_cleanup_group(t);	//clean up the group-specific workload data for this group

	return 0;
}
