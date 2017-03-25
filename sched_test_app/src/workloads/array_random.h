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

#include <time.h>

#include "../workload_type.h"
#include "array_walk.h"

#ifndef ARRAY_RANDOM_H
#define ARRAY_RANDOM_H

static void array_random_init_global()
{
	struct timespec now;
	//seed the random number generator
	clock_gettime(CLOCK_MONOTONIC, &now);
	srand(now.tv_nsec);
}

/*
 * This only differs from array_walk_init_task in that we initialize the random
 * number for this particular task, so it's not the same as all the others.
 */
static void *array_random_init_task(void *group_data, long task_wss)
{
	struct array_walk_data *data =
	    array_walk_init_task(group_data, task_wss);
	init_lfsrandom32(&data->rand);
	return data;
}

static void array_random_do_work(void *group_data, void *task_data,
				 unsigned long iterations)
{
	unsigned long i;
	struct array_walk_data *data = (struct array_walk_data *)task_data;
	for (i = 0; i < iterations; i++) {
		data->array[data->next_element]++;
		data->next_element = lfsrandom32(&data->rand) % data->num_elements;	//visit random element next
	}
}

static const char array_random_name[] = "array_random";
static struct workload array_random_workload = {
	.name = array_random_name,
	.capabilities = WORKLOAD_CAP_GROUP_WSS,

	.init_global = array_random_init_global,
	.init_group = array_walk_init_group,
	.init_task = array_random_init_task,
	.cleanup_global = 0,
	.cleanup_group = array_walk_cleanup_group,
	.cleanup_task = array_walk_cleanup_task,
	.do_work = array_random_do_work,

	.min_task_wss = 0,
	.max_task_wss = 0,
	.min_group_wss = DEFAULT_MIN_GROUP_WSS,
	.max_group_wss = DEFAULT_MAX_GROUP_WSS
};

#endif				/* ARRAY_RANDOM_H */
