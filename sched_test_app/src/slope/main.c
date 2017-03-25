/***************************************************************************
 *   Copyright (C) 2009-2012 Virginia Tech Real-Time Systems Lab           *
 *                                                                         *
 *   Written by Aaron Lindsay                                              *
 *   aaron@aclindsay.com                                                   *
 *                                                                         *
 *   Based on code written by Matthew Dellinger                            *
 *   mdelling@vt.edu                                                       *
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "../utils.h"
#include "../workload.h"
#include "slope.h"

void print_usage()
{
	printf("Slope generation application for ChronOS Linux (version ");
	printf(VERSION);
	printf(")\n");
	printf("------------------------------------------------------\n");
	printf("Optional flags:\n");
	printf("  -f            "
	       "Force generation of slopes that already exist.\n");
	printf("  -v            "
	       "Verbose output (show each individual slope sample as it is calculated, among other things).\n");
	printf("  -w workload   "
	       "Only re-generate the slope for \"workload\".\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	char optstring[] = "w:fv";
	char *workload_name = 0;
	int force = 0;		//force regeneration of pre-existing slopes
	int verbose = 0;
	int c, i, workload;

	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch (c) {
		case 'f':
			force = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			workload_name = optarg;
			break;
		default:
			print_usage();
			return 0;
		}
	}

	//set the workload integer constant from the workload name
	if (workload_name) {
		workload = find_workload(workload_name);
		if (workload < 0)
			fatal_error("Invalid workload identifier.\n");
	} else {
		workload = -1;
	}

	//attempt to lock the memory of this process
	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
		printf("Error: Unable to lock the memory space.\n");
		printf("Make sure you are running with sudo or root.\n");
		return 1;
	}

	if (force)
		printf("Forcing slope regeneration. ");
	else
		printf("Updating slope(s) only if "
		       "not previously calculated. ");

	printf("This process could take up to 15 minutes "
	       "for each workload. \n\n");

	if (workload < 0) {
		//loop through all workloads, finding the slope for each as we go
		for (i = 0; i < NUM_WORKLOADS; i++)
			check_slopes(i, force, verbose);
	} else {
		check_slopes(workload, force, verbose);
	}

	return 0;
}
