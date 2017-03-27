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

#ifndef CHRONOS_H
#define CHRONOS_H

#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

/* System call numbers for 3.0-chronos */
#ifdef __i386__
#define __NR_do_rt_seg			347
#define __NR_do_chronos_mutex		348
#define __NR_set_scheduler		349
#else
#ifdef __x86_64__
#define __NR_do_rt_seg			309
#define __NR_do_chronos_mutex		310
#define __NR_set_scheduler		311
#else
#ifdef __arm__
#define __NR_do_rt_seg			376
#define __NR_do_chronos_mutex		377
#define __NR_set_scheduler		378
#else
#error "Your architecture is not supported by ChronOS."
#endif
#endif
#endif

/* Masks for getting the scheduler from userspace
 * 1 bit - global
 * 7 bits - scheduler number
 * 8 bits - flags
 */
#define SCHED_GLOBAL_MASK		0x8000
#define SCHED_NUM_MASK			0x7F00
#define SCHED_FLAGS_MASK		0x00FF
/* Scheduler - behaviors for each flag may or may not be defined */
#define SCHED_RT_FIFO 			0x0000
#define SCHED_RT_RMA 			0x0100
#define SCHED_RT_EDF 			0x0200
#define SCHED_RT_HVDF 			0x0300
#define SCHED_RT_GFIFO			0x8000
#define SCHED_RT_GRMA			0x8100

/* Scheduling Flags */
/* PI == Priority Inheritance
 * HUA == HUA style abort handlers
 * ABORT_FIRST == tasks will be aborted as soon as they fail. 
 * 	Otherwise, the behavior is left unspecified.
 * NO_DEADLOCKS == with deadlock prevention
 *
 * Also, 0x*0 is left open for future flags.
 */
#define SCHED_FLAG_NO_FLAGS		0x00
#define SCHED_FLAG_HUA			0x01
#define SCHED_FLAG_PI			0x02
#define SCHED_FLAG_NO_DEADLOCKS		0x04

/* Operations for multiplexed system calls */
#define RT_SEG_BEGIN			0
#define RT_SEG_END			1
#define RT_SEG_ADD_ABORT		2

#define CHRONOS_MUTEX_REQUEST		0
#define CHRONOS_MUTEX_RELEASE		1
#define CHRONOS_MUTEX_INIT		2
#define CHRONOS_MUTEX_DESTROY		3

struct rt_data {
	int tid;
	int prio;
	unsigned long exec_time;
	unsigned int max_util;
	struct timespec *deadline;
	struct timespec *period;
};

/* An actual mutex */
struct mutex_data {
	u_int32_t value;
	int owner;
	unsigned long id;
};

typedef struct mutex_data chronos_mutex_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Set the basic information of the calling task */
long begin_rtseg_selfbasic(int prio, struct timespec* deadline,
			 struct timespec* period);

/* Set the basic information of another task */
long begin_rtseg_basic(int tid, int prio, struct timespec* deadline,
		     struct timespec* period);

/* Set the full rt information of the calling task */
long begin_rtseg_self(int prio, int max_util, struct timespec* deadline,
		    struct timespec* period, unsigned long exec_time);

/* Set the full rt information of another task */
long begin_rtseg(int tid, int prio, int max_util, struct timespec* deadline,
		struct timespec* period, unsigned long exec_time);

/* End the real-time portion of the calling task */
long end_rtseg_self(int prio);

/* End the real-time portion of another task */
long end_rtseg(int tid, int prio);

/* Add an abort handler for the task */
long add_abort_handler_self(int max_util, struct timespec *deadline,
			 unsigned long exec_time);

/* Add an abort handler for another task */
long add_abort_handler(int tid, int max_util, struct timespec *deadline,
			 unsigned long exec_time);

/* Add an abort handler for the task with no deadline */
long add_abort_handler_selfnodeadline(int max_util, unsigned long exec_time);

/* Add an abort handler for another task with no deadline */
long add_abort_handler_nodeadline(int tid, int max_util, unsigned long exec_time);

long set_scheduler(int scheduler, int prio, unsigned long cpus);

/* Changed from class for ease in replacing pthread_mutext */
long chronos_mutex_init(chronos_mutex_t *m);
long chronos_mutex_destroy(chronos_mutex_t *m);
long chronos_mutex_lock(chronos_mutex_t *m);
long chronos_mutex_unlock(chronos_mutex_t *m);
int chronos_mutex_owner(chronos_mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif

