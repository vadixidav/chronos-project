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

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

struct node {
	int value;
	struct node *next;
};

struct linked_list_data {
	struct node *head;
	struct node *next_node;
	long num_elements;
};

/*
 * Insert node n into the list begininning at data->head in order of increasing
 * value.
 */
static void insert_node(struct linked_list_data *data, struct node *n)
{
	struct node *cur = data->head;
	struct node *last = 0;

	if (!data->head) {
		data->head = n;
		return;
	}

	while (cur && cur->value > n->value) {
		last = cur;
		cur = cur->next;
	}

	if (!cur) {		//means we hit the end of the list
		last->next = n;
	} else if (!last) {	//i.e. our value is less than head's
		n->next = data->head;
		data->head = n;
	} else {		//our value is > last's but <= cur's
		n->next = cur;
		last->next = n;
	}
}

/*
 * Initialize the linked list by creating enough elements to fill group_wss with
 * random values, then insert them in increasing order of value into the list.
 */
static void *linked_list_init_group(long group_wss)
{
	int i;
	struct linked_list_data *data = malloc(sizeof(struct linked_list_data));
	uint32_t rand;
	assert(data);

	init_lfsrandom32(&rand);

	data->num_elements = group_wss / sizeof(struct node);
	data->head = 0;

	//insert all nodes
	for (i = 0; i < data->num_elements; i++) {
		struct node *n = (struct node *)malloc(sizeof(struct node));
		assert(n);
		n->value = (int)lfsrandom32(&rand);
		n->next = 0;
		insert_node(data, n);
	}

	data->next_node = data->head;

	return data;
}

/*
 * Initialize a copy of the group_data struct so that each thread has
 * its own and we don't cause race conditions.
 */
static void *linked_list_init_task(void *group_data, long task_wss)
{
	struct linked_list_data *data = malloc(sizeof(struct linked_list_data));
	*data = *(struct linked_list_data *)group_data;
	return data;
}

/*
 * Free the memory malloc-ed for the list in linked_list_init_task()
 */
static void linked_list_cleanup_task(void *group_data, void *task_data)
{
	struct linked_list_data *data = (struct linked_list_data *)task_data;
	free(data);
}

/*
 * Free the memory malloc-ed for the list in linked_list_init_group()
 */
static void linked_list_cleanup_group(void *group_data)
{
	struct linked_list_data *data = (struct linked_list_data *)group_data;

	while (data->head != 0) {
		struct node *tmp = data->head;
		data->head = data->head->next;
		free(tmp);
	}

	free(data);
}

/*
 * Walk the linked list, one element at a time, incrementing the list element's
 * value as we go. This makes sure we'll get both a memory read and write
 * operation for each iteration.
 */
static void linked_list_do_work(void *group_data, void *task_data,
				unsigned long iterations)
{
	unsigned long i;
	struct linked_list_data *data = (struct linked_list_data *)task_data;

	for (i = 0; i < iterations; i++) {
		data->next_node = data->next_node->next;
		if (!data->next_node)
			data->next_node = data->head;
		data->next_node->value++;
	}
}

static const char linked_list_name[] = "linked_list";
static struct workload linked_list_workload = {
	.name = linked_list_name,
	.capabilities = WORKLOAD_CAP_GROUP_WSS,

	.init_global = 0,
	.init_group = linked_list_init_group,
	.init_task = linked_list_init_task,
	.cleanup_global = 0,
	.cleanup_group = linked_list_cleanup_group,
	.cleanup_task = linked_list_cleanup_task,
	.do_work = linked_list_do_work,

	.min_task_wss = 0,
	.max_task_wss = 0,
	.min_group_wss = DEFAULT_MIN_GROUP_WSS,
	.max_group_wss = (1024 * 512)	//512k
};

#endif
