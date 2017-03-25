/***************************************************************************
 *   Copyright (C) 2009-2012 Virginia Tech Real-Time Systems Lab           *
 *                                                                         *
 *   Written by Matthew Dellinger                                          *
 *   mdelling@vt.edu                                                       *
 *                                                                         *
 *   Modified by Aaron Lindsay                                             *
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

#ifndef BURN_LOOP_H
#define BURN_LOOP_H

static void burn_loop_do_work(void *group_data, void *task_data,
			      unsigned long iterations)
{
	unsigned long i;
	for (i = 0; i < iterations; i++) {
	}
}

static const char burn_loop_name[] = "burn_loop";
static struct workload burn_loop_workload = {
	.name = burn_loop_name,
	.capabilities = WORKLOAD_CAP_EMPTY,

	.init_global = 0,
	.init_group = 0,
	.init_task = 0,
	.cleanup_global = 0,
	.cleanup_group = 0,
	.cleanup_task = 0,
	.do_work = burn_loop_do_work,

	.min_task_wss = 0,
	.max_task_wss = 0,
	.min_group_wss = 0,
	.max_group_wss = 0
};

#endif				/* BURN_LOOP_H */
