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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>

#ifndef UTILS_H
#define UTILS_H

/* The version number for this release of sched_test_app */
#define VERSION "3.0"

/* Get the tid for a thread */
#define gettid() syscall(SYS_gettid)

//function to make fatal error-handling simpler
static inline void fatal_error(char msg[])
{
	printf("Error: %s\n", msg);
	exit(1);
}

//function to do a generic warning
static inline void warning(char msg[])
{
	printf("Warning: %s\n", msg);
}

//macros to set or zero cpus in a cpu mask
#define MASK_ZERO(mask) (mask) = 0
#define MASK_SET(mask, cpu) (mask) |= (unsigned long) pow(2, cpu)

/* Priorities */
#define MAIN_PRIO                       98
#define TASK_CREATE_PRIO                96
#define TASK_START_PRIO                 94
#define TASK_CLEANUP_PRIO               92
#define TASK_RUN_PRIO                   90

/* Numerical definitions */
#define THOUSAND   1000
#define MILLION    1000000
#define BILLION    1000000000

/* Helper functions */
/*
 * Return the difference between the two timespecs in nanoseconds. This function
 * assumes that 'first' is less than 'last' and will return an invalid answer if
 * this is not true.
 */
static inline unsigned long timespec_subtract_ns(struct timespec *first,
						 struct timespec *last)
{
	signed long nsec;
	unsigned long time;

	nsec = last->tv_nsec - first->tv_nsec;
	if (nsec < 0) {
		time = BILLION + nsec;
		time += (last->tv_sec - first->tv_sec - 1) * BILLION;
	} else
		time = nsec + (last->tv_sec - first->tv_sec) * BILLION;

	return time;
}

/*
 * Return the difference between the two timespecs in microseconds. Does not
 * make any assumptions about the ordering of the two timespecs.
 */
static inline long long timespec_subtract_us(struct timespec *x,
					     struct timespec *y)
{
	long long sec, nsec;
	sec = x->tv_sec - y->tv_sec;
	nsec = x->tv_nsec - y->tv_nsec;
	if (nsec < 0) {
		nsec += BILLION;
		sec--;
	}

	return sec * MILLION + nsec / THOUSAND;
}

#endif				/* UTILS_H */
