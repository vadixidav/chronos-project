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

#include "utils.h"
#include "workload.h"
#include "tester_types.h"

#ifndef TASK_H
#define TASK_H

static inline void ill_formed_taskset_file()
{
	printf("Error: Ill-formed taskset file\n");
	exit(1);
}

#define group_leader(task) ((task)->group_leader == (task))
#define same_group(t1, t2) ((t1)->thread_group == (t2)->thread_group)

void init_task(struct test *tester, FILE * f);
void init_group(struct test *tester, FILE * f);
void set_lock_usage(struct task *t, struct test *tester);
void calculate_releases(struct task *t, struct test *tester);
void *start_task(void *arg);	//function passed to pthread_create
int start_task_group(void *arg);	//function passed to thread_create

#endif				/* TASK_H */
