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

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "utils.h"
#include "sched_algorithms.h"
#include "tester_types.h"
#include "tgroup.h"
#include "task.h"

#ifndef TESTER_H
#define TESTER_H

/*
 * Calculate greatest common divisor
 */
static inline unsigned long gcd(unsigned long m, unsigned long n)
{
	int t, r;
	if (m < n) {
		t = m;
		m = n;
		n = t;
	}
	r = m % n;

	if (r == 0)
		return n;
	else
		return gcd(n, r);
}

/*
 * Calculate the hyper period for the task set. This is also known as the lowest
 * common multiple of all the tasks' periods.
 */
static inline long taskset_lcm(struct test *tester)
{
	int i;
	long lcm = tester->tasks[0]->period;

	for (i = 1; i < tester->num_tasks; i++) {
		int GCD = gcd(lcm, tester->tasks[i]->period);
		lcm = lcm / GCD;
		lcm = lcm * tester->tasks[i]->period;
	}

	return lcm;
}

void run_test(struct test_app_opts *options);

#endif				/* TESTER_H */
