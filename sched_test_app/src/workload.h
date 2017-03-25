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

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "tester_types.h"
#include "workload_type.h"

//the header files of the workloads themselves
#include "workloads/burn_loop.h"
#include "workloads/array_walk.h"
#include "workloads/array_backwards.h"
#include "workloads/array_random.h"
#include "workloads/linked_list.h"
#include "workloads/bst.h"

#ifndef WORKLOAD_H
#define WORKLOAD_H

/*
 * Below are the generic functions which will handle calling the
 * workload-specific functions in an instance of the above struct workload.
 */
void workload_init_global(struct test *tester);
void workload_init_group(struct task *t);
void workload_init_task(struct task *t);
void workload_cleanup_global(struct test *tester);
void workload_cleanup_group(struct task *t);
void workload_cleanup_task(struct task *t);
int workload_do_work(struct task *t, unsigned long usage_ts);

/*
 * Functions to write and access the slope values for a particular
 * workload/timing method pair. A negative return value for either implies an
 * error occurred (most likely because the user does not have permissions to
 * create/modify/read the file.
 */
int workload_slope_file_accessible(const int workload, const int timing_method);
int clear_workload_slope(const int workload, const int timing_method);
double get_workload_slope(const int workload, const int timing_method,
			  const unsigned int task_wss,
			  const unsigned int group_wss);
int put_workload_slope(const int workload, const int timing_method,
		       const double slope, const unsigned int task_wss,
		       const unsigned int group_wss);

/*
 * Structures and functions to determine which workload we're currently working
 * with - return a pointer to that workload's collection of functions.
 */
#define NUM_WORKLOADS 6

#define WORKLOAD_BURN_LOOP        0
#define WORKLOAD_ARRAY_WALK       1
#define WORKLOAD_ARRAY_BACKWARDS  2
#define WORKLOAD_ARRAY_RANDOM     3
#define WORKLOAD_LINKED_LIST      4
#define WORKLOAD_BST              5

struct workload_listing {
	int mask;
	struct workload *w;
};

/*
 * The list of all the workloads, with their names, integer constants, and sets
 * of functions.
 */
static struct workload_listing workloads[] = {
	{WORKLOAD_BURN_LOOP, &burn_loop_workload},
	{WORKLOAD_ARRAY_WALK, &array_walk_workload},
	{WORKLOAD_ARRAY_BACKWARDS, &array_backwards_workload},
	{WORKLOAD_ARRAY_RANDOM, &array_random_workload},
	{WORKLOAD_LINKED_LIST, &linked_list_workload},
	{WORKLOAD_BST, &bst_workload}
};

static inline int workload_wss_valid_bounds(struct workload *w)
{
	if (w->capabilities & WORKLOAD_CAP_TASK_WSS) {
		if (w->min_task_wss < DEFAULT_MIN_TASK_WSS)
			return 0;
		else if (w->max_task_wss > DEFAULT_MAX_TASK_WSS)
			return 0;
	}
	if (w->capabilities & WORKLOAD_CAP_GROUP_WSS) {
		if (w->min_group_wss < DEFAULT_MIN_GROUP_WSS)
			return 0;
		else if (w->max_group_wss > DEFAULT_MAX_GROUP_WSS)
			return 0;
	}
	return 1;
}

/*
 * Find a workload given its name. Return -1 if not found.
 */
static inline int find_workload(char workload_name[])
{
	int i;
	for (i = 0; i < NUM_WORKLOADS; i++) {
		if (!strcmp(workload_name, workloads[i].w->name))
			return workloads[i].mask;
	}
	return -1;
}

/*
 * Return the workload's struct of associated functions given its integer
 * constant.
 */
static inline struct workload *get_workload_struct(int workload_mask)
{
	int i;
	for (i = 0; i < NUM_WORKLOADS; i++) {
		if (workload_mask == workloads[i].mask) {
			if (!workload_wss_valid_bounds(workloads[i].w))
				fatal_error("Workload bounds "
					    "are outside the allowable range.\n");
			return workloads[i].w;
		}
	}
	return 0;
}

/*
 * Return a workload's name as a string given its integer constant.
 */
static inline const char *get_workload_name(int workload_mask)
{
	int i;
	for (i = 0; i < NUM_WORKLOADS; i++) {
		if (workload_mask == workloads[i].mask)
			return workloads[i].w->name;
	}
	return 0;
}

#endif				/* WORKLOAD_H */
