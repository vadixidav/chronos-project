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

#include "chronos_aborts.h"
#include <stdio.h>

#define MIN_ABORTABLE_PID 1
#define DEFAULT_PID_MAX 32768
static int pid_max = 0; /* Cached copy of /proc/sys/kernel/pid_max */

/* Attempt to get and return the value in /proc/sys/kernel/pid_max, but
 * return the default if we fail */
static int read_pid_max() {
	int ret, proc_pid_max = DEFAULT_PID_MAX;
	FILE *f = fopen("/proc/sys/kernel/pid_max", "r");
	if (!f) {
		fprintf(stderr, "Warning: Unable to read /proc/sys/kernel/pid_max, defaulting to");
		fprintf(stderr, " %db for the size of the abort mmap() region\n", DEFAULT_PID_MAX);
		return proc_pid_max;
	}

	ret = fscanf(f, "%d", &proc_pid_max);
	if (!ret || ret == EOF) {
		fprintf(stderr, "Warning: Unable to read /proc/sys/kernel/pid_max, defaulting to");
		fprintf(stderr, " %db for the size of the abort mmap() region\n", DEFAULT_PID_MAX);
		proc_pid_max = DEFAULT_PID_MAX;
	}

	fclose(f);
	return proc_pid_max;
}

/* Initialize the aborts for a process by mapping the shared memory into the
 * current process, and return a struct which can be passed to get_abort_ptr on
 * a particular thread/process to get a char pointer for that process to check
 * to see if it has been aborted. Returns < 0 on failure, 0 on success */
int init_aborts(chronos_aborts_t * adata){
	unsigned int pageorder=0, pagesize = getpagesize(), mapsize=0;
	int fd;
	char * mmapptr;

	if(!adata)
		return -1;

	pid_max = read_pid_max();

	/*Attempt to open the character device*/
	fd = open("/dev/aborts", O_RDWR | O_SYNC);
	if(fd<0){ /*If we failed to open the file, make sure the module is loaded, and try to mknod*/
		system("modprobe abort_shmem");
		system("mknod /dev/aborts c 222 0");

		/*Try again to open the file, and if we fail, just fail*/
		fd = open("/dev/aborts", O_RDWR | O_SYNC);
		if(fd<0)
			return -1;
	}

	/*Figure out how many pages to allocate based on page size and number of pids */
	while((1 << pageorder) * pagesize < (pid_max - MIN_ABORTABLE_PID + 1) * sizeof(char))
		pageorder++;
	mapsize = (1 << pageorder) * pagesize;

	/*Actually map the character device into memory*/
	mmapptr = (char *) mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
	if(mmapptr == MAP_FAILED)
		goto out_mmap;

	/*Set the struct elements*/
	adata->mmap_pointer = mmapptr;
	adata->fd = fd;
	adata->mapsize = mapsize;
	adata->initialized = 1;

	return 0;

out_mmap:
	close(fd);
	return -1;
}

/* Unmap the mapped memory, and close the file descriptor.
 * Note: After this, the values inside the struct become invalid,
 * and should NOT be used for anything, including a call to get_abort_ptr() */
int cleanup_aborts(chronos_aborts_t * adata){
	if(!adata || !adata->initialized)
		return -1;

	pid_max = 0;

	/*unmap*/
	munmap(adata->mmap_pointer, adata->mapsize);
	/*close fd*/
	close(adata->fd);

	adata->initialized=0;

	return 0;
}

/* Given a pointer to the beginning of the memory block, return a pointer to a
 * character which will be non-zero only when the calling thread is being aborted */
char *get_abort_ptr(chronos_aborts_t * adata){
	int tid = syscall(SYS_gettid);
	if(tid > pid_max || tid < MIN_ABORTABLE_PID)
		return NULL;

	return get_abort_ptr_tid(adata, tid);
}

/* Given a pointer to the beginning of the memory block, return a pointer to a
 * character which will be non-zero only when the calling thread is being aborted */
char *get_abort_ptr_tid(chronos_aborts_t * adata, int tid){
	if(!adata || !adata->initialized)
		return NULL;

	return adata->mmap_pointer+tid-MIN_ABORTABLE_PID;
}
