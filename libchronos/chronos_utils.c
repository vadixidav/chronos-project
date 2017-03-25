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

#include <string.h>
#include <signal.h>
#include <sys/syscall.h>
#include "chronos_utils.h"

int deadline_met(struct timespec *end_time, struct timespec *deadline) {
	return (end_time->tv_sec < deadline->tv_sec || 
		(end_time->tv_sec == deadline->tv_sec && 
		end_time->tv_nsec <= deadline->tv_nsec));
}

unsigned long subtract_ts(struct timespec *first, struct timespec *last) {
	signed long nsec;
	unsigned long time;

	nsec = last->tv_nsec - first->tv_nsec;
	if(nsec < 0) {
		time = BILLION + nsec;
		time += (last->tv_sec - first->tv_sec - 1)*BILLION;
	} else
		time = nsec + (last->tv_sec - first->tv_sec)*BILLION;

	return time;
}

/* Make a periodic thread */
timer_t * make_periodic_threads(timer_t *timer,
				     void (*func)(union sigval),
				     void *arg,
				     struct timespec start_time,
				     struct timespec period,
				     pthread_attr_t *tattr) {
	struct sigevent event;
	struct itimerspec new_value, old_value;

	/* create a timer */
	event.sigev_notify_function = func;
	event.sigev_value.sival_ptr = arg;
	event.sigev_notify = SIGEV_THREAD;
	event.sigev_notify_attributes = tattr;

	if(timer_create(CLOCK_REALTIME, &event, timer) < 0) {
		perror("creating timer fails");
		return NULL;
	}

	/* start the timer */
	memcpy(&new_value.it_value, &start_time, sizeof(struct timespec));
	memcpy(&new_value.it_interval, &period, sizeof(struct timespec));

	if(timer_settime(*timer, TIMER_ABSTIME, &new_value, &old_value) < 0) {
		perror("timer_settime fails");
		return NULL;
	}

	return timer;
}

int delete_periodic_thread(timer_t timer) {
	return timer_delete(timer);
}

