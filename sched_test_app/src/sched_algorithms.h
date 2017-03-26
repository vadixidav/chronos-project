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

#include <string.h>
#include <chronos/chronos.h>

#ifndef SCHED_ALGORITHMS_H
#define SCHED_ALGORITHMS_H

struct algorithm {
	int mask;
	char name[50];
};

/*
 * The list of all the possibile scheduling algorithms, complete with their
 * names and constants.
 */
#define NUM_ALGORITHMS 17
static struct algorithm algorithms[] = {
	{SCHED_RT_FIFO, "FIFO"},
	{SCHED_RT_RMA, "RMA"},
	{SCHED_RT_EDF, "EDF"},
	{SCHED_RT_GFIFO, "GFIFO"},
	{SCHED_RT_GRMA, "GRMA"},
};

/*
 * Return the scheduler's constant given its name as a string. Return -1 if it
 * cannot be found.
 */
static inline int find_scheduler(char sched_name[])
{
	int i;
	for (i = 0; i < NUM_ALGORITHMS; i++) {
		if (!strcmp(sched_name, algorithms[i].name))
			return algorithms[i].mask;
	}
	return -1;
}

/*
 * Given a scheduler's integer constant, return its name as a string. Zero is
 * returned if it cannot be found.
 */
static inline char *get_sched_name(int sched_mask)
{
	int i;
	for (i = 0; i < NUM_ALGORITHMS; i++) {
		if ((sched_mask & ~SCHED_FLAGS_MASK) == algorithms[i].mask)
			return algorithms[i].name;
	}
	return 0;
}

#endif				/* SCHED_ALGORITHMS_H */
