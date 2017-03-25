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

#include "tgroup.h"

#include <sys/wait.h>
#include <errno.h>

/*
 * Create a new thread in its own thread group. Otherwise, should be
 * more-or-less functionally the same as pthread_create.
 */
int tgroup_create(tgroup_t * tgroup, int (*fn) (void *), void *arg)
{
	pid_t pid = fork();

	if (pid == -1) {
		return -1;
	} else if (pid == 0) {
		/* re-lock all the memory, because these locks were lost on fork() */
		if (mlockall(MCL_CURRENT | MCL_FUTURE))
			fatal_error("Failed to re-lock all memory on fork()\n");
		exit(fn(arg));
	} else {
		tgroup->pid = pid;
		return 0;
	}
}

/*
 * Join a thread group created with tgroup_create()
 */
int tgroup_join(tgroup_t * tgroup, int *ret)
{
	int status, r;

	do {
		r = waitpid(tgroup->pid, &status, 0);
	} while (r < 0 && errno == EINTR);

	if (r != tgroup->pid)
		return -1;

	if (WIFEXITED(status) && ret)
		*ret = (int)WEXITSTATUS(status);

	return 0;
}
