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

#include "../workload_type.h"
#include "../random.h"

#ifndef ARRAY_WALK_H
#define ARRAY_WALK_H

#define STRIDE 64/sizeof(int)	//bytes

struct array_walk_data {
	int *array_unaligned;
	int *array;
	long num_elements;
	long next_element;
	uint32_t rand;		//current random value (only used by array_random.h)
};

#define malloc_aligned(size) malloc((size) + 64)
#define malloc_align(ptr) ((ptr) + ((unsigned long)(ptr) % 64))

static void *array_walk_init_group(long group_wss)
{
	struct array_walk_data *data = malloc(sizeof(struct array_walk_data));
	assert(data);
	data->next_element = 0;
	data->num_elements = group_wss / sizeof(int);
	data->array_unaligned =
	    (int *)malloc_aligned(sizeof(int) * data->num_elements);
	data->array = (int *)malloc_aligned(sizeof(int) * data->num_elements);
	assert(data->array);
	return data;
}

/*
 * Initialize a copy of the group_data struct so that each thread has
 * its own and we don't cause race conditions.
 */
static void *array_walk_init_task(void *group_data, long task_wss)
{
	struct array_walk_data *data = malloc(sizeof(struct array_walk_data));
	*data = *(struct array_walk_data *)group_data;
	return data;
}

/*
 * Free the memory malloc-ed for the list in array_walk_init_task()
 */
static void array_walk_cleanup_task(void *group_data, void *task_data)
{
	struct array_walk_data *data = (struct array_walk_data *)task_data;
	free(data);
}

static void array_walk_cleanup_group(void *task_data)
{
	struct array_walk_data *data = (struct array_walk_data *)task_data;
	free(data->array_unaligned);
	data->array_unaligned = 0;
	data->array = 0;
	free(data);
}

static void array_walk_do_work(void *group_data, void *task_data,
			       unsigned long iterations)
{
	unsigned long i;
	struct array_walk_data *data = (struct array_walk_data *)task_data;
	for (i = 0; i < iterations; i++) {
		data->array[data->next_element]++;
		data->next_element = (data->next_element + STRIDE) % data->num_elements;	//stride forwards STRIDE elements
	}
}

static const char array_walk_name[] = "array_walk";
static struct workload array_walk_workload = {
	.name = array_walk_name,
	.capabilities = WORKLOAD_CAP_GROUP_WSS,

	.init_global = 0,
	.init_group = array_walk_init_group,
	.init_task = array_walk_init_task,
	.cleanup_global = 0,
	.cleanup_group = array_walk_cleanup_group,
	.cleanup_task = array_walk_cleanup_task,
	.do_work = array_walk_do_work,

	.min_task_wss = 0,
	.max_task_wss = 0,
	.min_group_wss = DEFAULT_MIN_GROUP_WSS,
	.max_group_wss = DEFAULT_MAX_GROUP_WSS
};

#endif				/* ARRAY_WALK_H */
