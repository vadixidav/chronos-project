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

#include "chronos.h"

/* Set the basic information of the calling task */
long begin_rtseg_selfbasic(int prio, struct timespec* deadline,
			 struct timespec* period) {
	struct rt_data data;
	data.tid = 0;
	data.prio = prio;
	data.max_util = 0;
	data.exec_time = 0;
	data.period = period;
	data.deadline = deadline;
	return syscall(__NR_do_rt_seg, RT_SEG_BEGIN, &data);
}

/* Set the basic information of another task */
long begin_rtseg_basic(int tid, int prio, struct timespec* deadline,
		     struct timespec* period) {
	struct rt_data data;
	data.tid = tid;
	data.prio = prio;
	data.max_util = 0;
	data.exec_time = 0;
	data.period = period;
	data.deadline = deadline;
	return syscall(__NR_do_rt_seg, RT_SEG_BEGIN, &data);
}

/* Set the full rt information of the calling task */
long begin_rtseg_self(int prio, int max_util, struct timespec* deadline,
		    struct timespec* period, unsigned long exec_time) {
	struct rt_data data;
	data.tid = 0;
	data.prio = prio;
	data.max_util = max_util;
	data.exec_time = exec_time;
	data.period = period;
	data.deadline = deadline;
	return syscall(__NR_do_rt_seg, RT_SEG_BEGIN, &data);
}

/* Set the full rt information of another task */
long begin_rtseg(int tid, int prio, int max_util, struct timespec* deadline,
		struct timespec* period, unsigned long exec_time) {
	struct rt_data data;
	data.tid = tid;
	data.prio = prio;
	data.max_util = max_util;
	data.exec_time = exec_time;
	data.period = period;
	data.deadline = deadline;
	return syscall(__NR_do_rt_seg, RT_SEG_BEGIN, &data);
}

/* End the real-time portion of the calling task */
long end_rtseg_self(int prio) {
	struct rt_data data;
	data.tid = 0;
	data.prio = prio;
	data.period = NULL;
	data.deadline = NULL;
	return syscall(__NR_do_rt_seg, RT_SEG_END, &data);
}

/* End the real-time portion of another task */
long end_rtseg(int tid, int prio) {
	struct rt_data data;
	data.tid = tid;
	data.prio = prio;
	data.period = NULL;
	data.deadline = NULL;
	return syscall(__NR_do_rt_seg, RT_SEG_END, &data);
}

/* Add an abort handler for the task */
long add_abort_handler_self(int max_util, struct timespec *deadline,
			 unsigned long exec_time) {
	struct rt_data data;
	data.tid = 0;
	data.max_util = max_util;
	data.exec_time = exec_time;
	data.period = NULL;
	data.deadline = deadline;
	return syscall(__NR_do_rt_seg, RT_SEG_ADD_ABORT, &data);
}

/* Add an abort handler for another task */
long add_abort_handler(int tid, int max_util, struct timespec *deadline,
		       unsigned long exec_time) {
	struct rt_data data;
	data.tid = tid;
	data.max_util = max_util;
	data.exec_time = exec_time;
	data.period = NULL;
	data.deadline = deadline;
	return syscall(__NR_do_rt_seg, RT_SEG_ADD_ABORT, &data);
}

/* Add an abort handler for the task with no deadline */
long add_abort_handler_selfnodeadline(int max_util, unsigned long exec_time) {
	struct rt_data data;
	data.tid = 0;
	data.max_util = max_util;
	data.exec_time = exec_time;
	data.period = NULL;
	data.deadline = NULL;
	return syscall(__NR_do_rt_seg, RT_SEG_ADD_ABORT, &data);
}

/* Add an abort handler for another task with no deadline */
long add_abort_handler_nodeadline(int tid, int max_util,
				  unsigned long exec_time) {
	struct rt_data data;
	data.tid = tid;
	data.max_util = max_util;
	data.exec_time = exec_time;
	data.period = NULL;
	data.deadline = NULL;
	return syscall(__NR_do_rt_seg, RT_SEG_ADD_ABORT, &data);
}

long set_scheduler(int scheduler, int prio, unsigned long cpus) {
	unsigned long mask = cpus;
	unsigned int len = sizeof(mask);

	return syscall(__NR_set_scheduler, scheduler, prio, len, (cpu_set_t*) &mask);
}

/* Changed from class for ease in replacing pthread_mutext */
/* Init a resource */
long chronos_mutex_init(chronos_mutex_t *m) {
	m->value = 0;
	m->owner = 0;
	return syscall(__NR_do_chronos_mutex, m, CHRONOS_MUTEX_INIT);
}

/* Destroy a resource */
long chronos_mutex_destroy(chronos_mutex_t *m) {
	return syscall(__NR_do_chronos_mutex, m, CHRONOS_MUTEX_DESTROY);
}

/* Request a resource from the host */
long chronos_mutex_lock(chronos_mutex_t *m) {
	return syscall(__NR_do_chronos_mutex, m, CHRONOS_MUTEX_REQUEST);
}

/* Release a resource */
long chronos_mutex_unlock(chronos_mutex_t *m) {
	return syscall(__NR_do_chronos_mutex, m, CHRONOS_MUTEX_RELEASE);
}

int chronos_mutex_owner(chronos_mutex_t *m) {
	return m->owner;
}

