/***************************************************************************
 *   Copyright (C) 2009-2012 Virginia Tech Real-Time Systems Lab	   *
 *   Written by Aaron Lindsay						   *
 *   aaron.lindsay@vt.edu						   *
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

#ifndef CHRONOS_ABORTS_H
#define CHRONOS_ABORTS_H

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>

/*Structure to hold the data surrounding the shared memory for a particular process*/
struct chronos_aborts_data{
	char * mmap_pointer;
	int fd;
	unsigned int mapsize;
	char initialized;
};

typedef struct chronos_aborts_data chronos_aborts_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the aborts for a process by mapping the shared memory into the current process, and */
int init_aborts(chronos_aborts_t * adata);

/* Unmap the mapped memory, and close the file descriptor */
int cleanup_aborts(chronos_aborts_t * adata);

/* Given a pointer to the beginning of the memory block and the tid of the calling thread/process, return a pointer to the character variable to check for the current thread to see if it has been aborted */
char *get_abort_ptr(chronos_aborts_t * adata);
char *get_abort_ptr_tid(chronos_aborts_t * adata, int tid);

#ifdef __cplusplus
}
#endif

#endif /*CHRONOS_ABORTS_H*/
