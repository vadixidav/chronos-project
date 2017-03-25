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

#ifndef BST_H
#define BST_H

struct bst_node {
	int value;		/* Used for inserting and searching */
	int false_value;	/* Used to have something to update */
	struct bst_node *left;
	struct bst_node *right;
};

struct bst_data {
	struct bst_node *head;
	struct bst_node *next_node;	//the node we're at while trying to find 'finding_value'
	int finding_value;	//the value we're currently trying to find
	long num_elements;
	uint32_t rand;
};

/*
 * Insert node n into the BST begininning at data->head in order of increasing
 * value.
 */
static void _insert_bst_node(struct bst_node *head, struct bst_node *n)
{
	if (n->value < head->value) {
		if (head->left)
			_insert_bst_node(head->left, n);
		else
			head->left = n;
	} else {
		if (head->right)
			_insert_bst_node(head->right, n);
		else
			head->right = n;
	}
}

static void insert_bst_node(struct bst_data *data, struct bst_node *n)
{
	if (!data->head) {
		data->head = n;
		return;
	}

	_insert_bst_node(data->head, n);
}

/*
 * Initialize the BST by creating enough elements to fill group_wss with
 * random values, then insert them in increasing order of value.
 */
static void *bst_init_group(long group_wss)
{
	int i;
	struct bst_data *data = malloc(sizeof(struct bst_data));
	assert(data);

	init_lfsrandom32(&data->rand);

	data->num_elements = group_wss / sizeof(struct bst_node);
	data->head = 0;

	//insert all nodes
	for (i = 0; i < data->num_elements; i++) {
		struct bst_node *n =
		    (struct bst_node *)malloc(sizeof(struct bst_node));
		assert(n);
		n->value = (int)lfsrandom32(&data->rand);
		n->left = 0;
		n->right = 0;
		insert_bst_node(data, n);
	}

	data->next_node = data->head;
	data->finding_value = (int)lfsrandom32(&data->rand);

	return data;
}

/*
 * Initialize a copy of the group_data struct so that each thread has
 * its own and we don't cause race conditions.
 */
static void *bst_init_task(void *group_data, long task_wss)
{
	struct bst_data *data = malloc(sizeof(struct bst_data));
	*data = *(struct bst_data *)group_data;
	init_lfsrandom32(&data->rand);
	return data;
}

/*
 * Free the memory malloc-ed for the list in bst_init_task()
 */
static void bst_cleanup_task(void *group_data, void *task_data)
{
	struct bst_data *data = (struct bst_data *)task_data;
	free(data);
}

/*
 * Free the memory malloc-ed for the list in bst_init_group()
 */
static void _bst_cleanup_node(struct bst_node *n)
{
	if (n->right) {
		_bst_cleanup_node(n->right);
		free(n->right);
	}

	if (n->left) {
		_bst_cleanup_node(n->left);
		free(n->left);
	}
}

static void bst_cleanup_group(void *group_data)
{
	struct bst_data *data = (struct bst_data *)group_data;

	if (data->head) {
		_bst_cleanup_node(data->head);
		free(data->head);
	}

	free(data);
}

/*
 * Walk the BST, searching for a random value.
 */
static void bst_do_work(void *group_data, void *task_data,
			unsigned long iterations)
{
	unsigned long i;
	struct bst_data *data = (struct bst_data *)task_data;

	for (i = 0; i < iterations; i++) {
		/* If we found the value and reset it to 0, start over */
		if (!data->finding_value) {
			data->finding_value = (int)lfsrandom32(&data->rand);
			data->next_node = data->head;
		} else {
			struct bst_node *n = 0;

			/* Decide which way we need to go (if any) */
			if (data->finding_value < data->next_node->value)
				n = data->next_node->left;
			else if (data->finding_value > data->next_node->value)
				n = data->next_node->right;

			/* If we found the value, update it; if we ran past the last value,
			 * update the false_value on the last node we saw */
			if (!n || data->finding_value == data->next_node->value) {
				data->finding_value = 0;
				data->next_node->false_value++;
			} else {
				/* If we didn't find it or hit the end, just keep on truckin' */
				data->next_node = n;
			}
		}
	}
}

static const char bst_name[] = "bst";
static struct workload bst_workload = {
	.name = bst_name,
	.capabilities = WORKLOAD_CAP_GROUP_WSS,

	.init_global = 0,
	.init_group = bst_init_group,
	.init_task = bst_init_task,
	.cleanup_global = 0,
	.cleanup_group = bst_cleanup_group,
	.cleanup_task = bst_cleanup_task,
	.do_work = bst_do_work,

	.min_task_wss = 0,
	.max_task_wss = 0,
	.min_group_wss = DEFAULT_MIN_GROUP_WSS,
	.max_group_wss = (1024 * 512)	//512k
};

#endif
