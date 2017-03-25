/***************************************************************************
 *   Copyright (C) 2009-2012 Virginia Tech Real-Time Systems Lab	   *
 *   Written by Matthew Dellinger					   *
 *   mdelling@vt.edu							   *
 *									   *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or	   *
 *   (at your option) any later version.				   *
 *									   *
 *   This program is distributed in the hope that it will be useful,	   *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of	   *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the	   *
 *   GNU General Public License for more details.			   *
 *									   *
 *   You should have received a copy of the GNU General Public License	   *
 *   along with this program; if not, write to the			   *
 *   Free Software Foundation, Inc.,					   *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.		   *
 ***************************************************************************/

#ifndef CHRONOS_UTILS_H
#define CHRONOS_UTILS_H

#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define BILLION		1000000000
#define MILLION		1000000
#define THOUSAND	1000

/* Read the timestamp counter. Throw mfence to prevent reordering */
#ifdef __i386__
#define DECLARE_ARGS(val, low, high)    unsigned long long val
#define EAX_EDX_VAL(val, low, high)     (val)
#define EAX_EDX_RET(val, low, high)     "=A" (val)
#else
#define DECLARE_ARGS(val, low, high)    unsigned low, high
#define EAX_EDX_VAL(val, low, high)     ((low) | ((uint64_t)(high) << 32))
#define EAX_EDX_RET(val, low, high)     "=a" (low), "=d" (high)
#endif

static inline unsigned long long RDTSC()
{
	DECLARE_ARGS(val, low, high);

	asm volatile(	"mfence\n"
			"rdtsc" : EAX_EDX_RET(val, low, high));

	return EAX_EDX_VAL(val, low, high);
}

#ifdef __cplusplus
extern "C" {
#endif

int deadline_met(struct timespec *end_time, struct timespec *deadline);

unsigned long subtract_ts(struct timespec *first, struct timespec *last);

/* Make periodic threads */
timer_t * make_periodic_threads(timer_t *timer,
				     void (*func)(union sigval),
				     void *arg,
				     struct timespec start_time,
				     struct timespec period,
				     pthread_attr_t *tattr);

int delete_periodic_thread(timer_t timer);

#ifdef __cplusplus
}
#endif

#endif

