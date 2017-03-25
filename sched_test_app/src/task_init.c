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

#include <assert.h>

#include "task.h"
#include "salloc.h"

/*
 * Given a comma-separated list of processor IDs in string cpus[], initalize
 * a scheduling domain mask which contains all these processors, and if it is not
 * already in the global list of scheduling domains, add it.
 */
static void initialize_task_cpus(struct test *tester, struct task *t,
				 char cpus[])
{
	char tmp[32];
	char *c;
	int i, j = 0;
	int cpu;
	unsigned long mask = 0;

	//zero the task's cpu affinity mask
	MASK_ZERO(t->cpu_mask);

	/*
	 * If we specified 'all' of the cpus, add them all and return.
	 */
	if (cpus[0] == 'a' && cpus[1] == 'l' && cpus[2] == 'l') {
		for (i = 0; i < tester->num_processors; i++) {
			MASK_SET(mask, i);	//add cpu to domain mask
			MASK_SET(t->cpu_mask, i);	//add cpu to task affinity mask
		}
	} else {
		/*
		 * If we specified some subset of the cpus, parse which ones and add them to
		 * the appropriate domain mask
		 */
		c = cpus;
		while (1) {
			i = 0;
			//read the next integer into tmp
			do {
				tmp[i++] = c[j++];
			} while (tmp[i - 1] >= '0' && tmp[i - 1] <= '9');
			tmp[i] = '\0';

			//read the cpu as an integer, and add it to the mask
			cpu = atoi(tmp);
			if (cpu >= tester->num_processors) {
				printf("Error: The processor specified (%d) "
				       "is larger than the number of procssors on this system(%d). Exiting.\n",
				       cpu, tester->num_processors);
				exit(1);
			}
			MASK_SET(mask, cpu);	//add cpu to domain mask
			MASK_SET(t->cpu_mask, cpu);	//add cpu to task affinity mask

			//if the bad character wasn't a comma, we're done here
			if (c[j - 1] != ',')
				break;
		}
	}

	if (!mask)
		fatal_error("Empty domain mask");

	//add domain mask to list of masks if doesn't already exist
	//FIXME This doesn't enforce that scheduling domains are non-overlapping. Is this a problem?
	for (i = 0; i < tester->num_processors; i++) {
		if (tester->domain_masks[i] == mask)
			return;
		else if (tester->domain_masks[i] == 0) {
			tester->domain_masks[i] = mask;
			return;
		}
	}

	//if we reached here, it means that we ran off the end of the array of domain masks. Oops.
	fatal_error("You have more domain masks than processors. "
		    "This isn't necessarily a bad thing, we just don't currently support it. "
		    "Contact one of our developers (http://chronoslinux.org) "
		    "and let us know you need this capability. Exiting.");
}

/*
 * Given a comma-separated list in string locks[], initalize this task's locks
 * array to point to those locks in the global locks list to which they refer.
 */
static void initialize_task_locks(struct test *tester, struct task *t,
				  char locks[])
{
	char tmp[32];
	char *l;
	int i, j = 0;
	int lock_no;

	//initialize our locks array
	t->my_locks =
	    (chronos_mutex_t **) salloc(sizeof(chronos_mutex_t *) *
					tester->num_locks);
	if (!t->my_locks)
		fatal_error("Failed to allocate memory.");

	/*
	 * If we specified 'all' of the locks, add them all and return.
	 */
	if (locks[0] == 'a' && locks[1] == 'l' && locks[2] == 'l') {
		t->num_my_locks = tester->num_locks;
		for (i = 0; i < tester->num_locks; i++)
			t->my_locks[i] = &tester->locks[i];
		return;
	}

	/*
	 * If we specified some subset of the locks, parse which ones and add them to
	 * the task's array of pointers to locks.
	 */
	l = locks;
	while (1) {
		i = 0;
		//read the next integer into tmp
		do {
			tmp[i++] = l[j++];
		} while (tmp[i - 1] >= '0' && tmp[i - 1] <= '9');
		tmp[i] = '\0';

		//read the lock as an integer, and get the pointer to the global locks array
		lock_no = atoi(tmp);
		t->my_locks[t->num_my_locks] = &tester->locks[lock_no];
		t->num_my_locks++;

		//if the bad character wasn't a comma, we're done here
		if (l[j - 1] != ',')
			break;
	}
}

static void set_period_ts(struct task *t)
{
	t->period_ts.tv_sec = t->period / MILLION;
	t->period_ts.tv_nsec =
	    t->period * THOUSAND - t->period_ts.tv_sec * MILLION * THOUSAND;
}

/*
 * Initialize the thread group leader (task->group_leader) for this task.
 * If we are the first task to be read in from the taskset file in this group,
 * we're the leader. Otherwise, find another task in our group and
 * update our pointer to point to the leader.
 */
static void init_group_leader(struct test *tester, struct task *task)
{
	struct task *curr = tester->task_list;
	assert(curr);

	while (curr) {
		if (curr != task && same_group(curr, task)) {
			assert(curr->group_leader);
			task->group_leader = curr->group_leader;
			return;
		}
		curr = curr->next;
	}

	//If noone else is the group leader for our task group, make us the leader
	task->group_leader = task;
}

/*
 * Read the line for one task from the taskset file and create a task struct from it
 */
void init_task(struct test *tester, FILE * f)
{
	char cpus[1024], locks[1024];
	int tmp;
	int num_fields, locks_present, hua_present;

	//initialize the task's struct
	struct task *t = new_task(tester);

	//get the first four fields
	num_fields =
	    fscanf(f, "%s %d %ld %ld %ld %d ", cpus, &t->thread_group,
		   &t->task_wss, &t->period, &t->exec_time, &t->utility);
	if (num_fields != 6)
		fatal_error("Ill-formed taskset file: "
			    "missing one or more required fields for task "
			    "(number of allowed cpus, thread group, task WSS, period in us, execution time in us, and task utility).");

	//ensure the WSS is within the bounds of what we allow
	if (!(tester->workload->capabilities & WORKLOAD_CAP_TASK_WSS))
		t->task_wss = 0;
	else if (t->task_wss < tester->workload->min_task_wss)
		t->task_wss = tester->workload->min_task_wss;
	else if (t->task_wss > tester->workload->max_task_wss)
		t->task_wss = tester->workload->max_task_wss;

	set_period_ts(t);	//initialize the period as a timespec, since this is required to be passed into the syscall later

	initialize_task_cpus(tester, t, cpus);

	//attempt to read last two fields, one at a time
	//first, list of locks
	tmp = getc(f);
	if (tmp == EOF)
		goto add_task;
	else if (tmp == 'T' || tmp == '#' || tmp == 'G') {
		ungetc(tmp, f);
		goto add_task;
	} else {
		ungetc(tmp, f);
		locks_present = fscanf(f, " %s ", locks);
		if (locks_present != 1)
			fatal_error("Ill-formed taskset file: "
				    "improper task lock list format.");
		initialize_task_locks(tester, t, locks);
	}

	//if we got here, attempt to get HUA utility as well
	tmp = getc(f);
	if (tmp == EOF)
		goto add_task;
	else if (tmp < '0' || tmp > '9') {
		ungetc(tmp, f);
		goto add_task;
	} else {
		ungetc(tmp, f);
		hua_present = fscanf(f, " %d ", &t->hua_utility);
		if (hua_present != 1)
			fatal_error("Ill-formed taskset file: "
				    "improper task HUA format.");
	}

 add_task:
	//add task to the beginning of the list
	t->next = tester->task_list;
	tester->task_list = t;
	tester->num_tasks++;

	init_group_leader(tester, t);
}

/*
 * Read the line for one thread group from the taskset file and initialize
 * things accordingly.
 */
void init_group(struct test *tester, FILE * f)
{
	int num_fields, thread_group;
	long group_wss;
	struct task *curr;

	//read the fields in from the taskset file
	num_fields = fscanf(f, "%d %ld ", &thread_group, &group_wss);
	if (num_fields != 2)
		fatal_error("Ill-formed taskset file: "
			    "missing one or more required fields for group (group number, group WSS).");

	//ensure the WSS is within the bounds of what we allow
	if (!(tester->workload->capabilities & WORKLOAD_CAP_GROUP_WSS))
		group_wss = 0;
	else if (group_wss < tester->workload->min_group_wss)
		group_wss = tester->workload->min_group_wss;
	else if (group_wss > tester->workload->max_group_wss)
		group_wss = tester->workload->max_group_wss;

	//search for the group_leader for this thread group, and add this information to its struct
	curr = tester->task_list;
	while (curr) {
		if (group_leader(curr) && curr->thread_group == thread_group) {
			curr->group_wss = group_wss;
			return;
		}
		curr = curr->next;
	}

	//if we didn't return by now, we haven't found this task's group leader
	fatal_error("Ill-formed taskset file: "
		    "group line specifies empty group (i.e. no tasks are in it).");
}
