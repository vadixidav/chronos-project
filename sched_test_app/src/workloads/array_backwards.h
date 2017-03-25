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
#include "array_walk.h"

#ifndef ARRAY_BACKWARDS_H
#define ARRAY_BACKWARDS_H

static void array_backwards_do_work(void *group_data, void *task_data,
				    unsigned long iterations)
{
	unsigned long i;
	struct array_walk_data *data = (struct array_walk_data *)task_data;
	for (i = 0; i < iterations; i++) {
		data->array[data->next_element]++;
		data->next_element = (data->num_elements + data->next_element - STRIDE) % data->num_elements;	//stride backwards STRIDE elements
	}
}

static const char array_backwards_name[] = "array_backwards";
static struct workload array_backwards_workload = {
	.name = array_backwards_name,
	.capabilities = WORKLOAD_CAP_GROUP_WSS,

	.init_global = 0,
	.init_group = array_walk_init_group,
	.init_task = array_walk_init_task,
	.cleanup_global = 0,
	.cleanup_group = array_walk_cleanup_group,
	.cleanup_task = array_walk_cleanup_task,
	.do_work = array_backwards_do_work,

	.min_task_wss = 0,
	.max_task_wss = 0,
	.min_group_wss = DEFAULT_MIN_GROUP_WSS,
	.max_group_wss = DEFAULT_MAX_GROUP_WSS
};

#endif				/* ARRAY_BACKWARDS_H */
