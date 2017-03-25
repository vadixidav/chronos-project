/***************************************************************************
 *   Copyright (C) 2009-2012 Virginia Tech Real-Time Systems Lab           *
 *                                                                         *
 *   Written by Matthew Dellinger                                          *
 *   mdelling@vt.edu                                                       *
 *                                                                         *
 *   Modified by Aaron Lindsay                                             *
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>

#include "utils.h"
#include "tester.h"
#include "tester_types.h"
#include "workload.h"

void print_usage()
{
	printf("Console testing application for ChronOS Linux (version ");
	printf(VERSION);
	printf(")\n");
	printf("------------------------------------------------------\n");
	printf("Required options:\n");
	printf("  -s scheduler  The name of scheduler - RMA, EDF, DASA etc\n");
	printf("  -f filename   The name of the file that has the task list\n");
	printf("  -c cpu-usage  The cpu-usage\n");
	printf("  -r run-time   The total time the test needs to run\n");
	printf("\n");
	printf("Optional Flags:\n");
	printf("  -t timing     "
	       "Specify an execution timing method. Defaults to \"worst-case\"\n");
	printf("  -w workload   "
	       "Specify a workload. Defaults to \"burn_loop\"\n");
	printf("  -p            Enable priority inheritance\n");
	printf("  -h            Enable HUA abort handlers\n");
	printf("  -d            Enable deadlock prevention\n");
	printf("  -z            "
	       "Don't run the test, just print the hyper-period\n");
	printf("  -l cs-length  Enable locking, and provide critical\n");
	printf("                section length\n");
	printf("  -n            "
	       "Enable nested locking (as opposed to sequential)\n");
	printf("\n");
	printf("Batch Mode Options:\n");
	printf("  -b            Enable batch mode\n");
	printf("  -e end-usage  The end cpu-usage (used in batch mode)\n");
	printf("  -i interval   The interval for each iteration in batch\n");
	printf("\n");
	printf("Output Formatting (mutually exclusive of each other):\n");
	printf("  -v            Enable verbose mode\n");
	printf("  -o            Enable output to a log file\n");
	printf("  -x            Enable log in Excel format\n");
	printf("  -g            Enable log in Gnuplot format\n");
	printf("\n");
	printf("Examples:\n");
	printf("    $ sched_test_app -s RMA -f tasksets/5t_nl -c 100 -r 20\n");
	printf(" OR $ sched_test_app -s EDF -f "
	       "tasksets/10t_nl -c 50 -l 10 -r 20 -p -h -d\n");
	printf("\n");
	printf(" NOTE: -p -h -d can also be written as -phd\n");
}

/*
 * Validate the options, returning error messages if applicable
 */
int validate_options(struct test_app_opts *options)
{
	int ret = 0;

	/* If we are lacking a taskset or scheduler, die */
	if (!options->taskset_filename ||
	    options->scheduler == -1 ||
	    options->cpu_usage == 0 || options->run_time == 0) {
		printf("Error: Provide the scheduler, cpu usage,"
		       " runtime and taskset filename.\n");
		ret = 1;
	}

	if (options->workload < 0) {
		printf("Error: Invalid workload identifier.\n");
		ret = 1;
	}

	/* If batch mode is enabled, has the end-usage and usage increment interval been given? */
	if (options->batch_mode && (!options->end_usage || !options->interval)) {
		printf("Error: Batch mode must be accompanied"
		       " by nonzero end_usage and interval.\n");
		ret = 1;
	}

	if (options->batch_mode && options->end_usage < options->cpu_usage) {
		printf("Batchmode selected, "
		       "but end usage is less than start usage.\n");
		ret = 1;
	}

	if (options->cpu_usage <= 0 || options->cpu_usage > 10000) {
		printf("Invalid CPU usage. Must be between 0 and 10000.\n");
		ret = 1;
	}

	if (options->run_time <= 0 || options->run_time > 3600) {
		printf("Invalid execution time. "
		       "Must be between 0 and 3600 seconds.\n");
		ret = 1;
	}

	if ((options->cs_length <= 0 && options->locking & LOCKING)
	    || options->cs_length > 100) {
		printf("Invalid critical section length.\n");
		ret = 1;
	}
	//fixup variables if we're not in batch mode
	if (!options->batch_mode) {
		options->end_usage = options->cpu_usage;
		options->interval = 1;
	}

	return ret;
}

#define get_integer(a,i) do { \
       			i = atoi(a); \
			if (i == INT_MAX || i == INT_MIN) \
				i = 0;\
			} while(0)

/*
 * Parse all the arguments into the options struct and call run_test()
 */
int main(int argc, char *argv[])
{
	char optstring[] = "c:e:f:i:l:r:s:t:w:bdghnopvxz";
	int c;
	char *sched_name = 0, *workload_name = 0, *timing_name = 0;
	int index;
	struct test_app_opts options = {
		.output_format = OUTPUT_LOG,
		.cpu_usage = 100,
		.run_time = 1,
		.scheduler = -1,
		.taskset_filename = 0,
		.workload = 0,
		.priority_inheritance = 0,
		.enable_hua = 0,
		.deadlock_prevention = 0,
		.no_run = 0,
		.locking = NO_LOCKING,
		.cs_length = 0,
		.batch_mode = 0,
		.end_usage = 0,
		.interval = 0
	};

	//Minimum 8 arguments (i.e. 4 actual flags and their corresponding textual arguments)
	if (argc < 8) {
		printf("Error: Provide the scheduler, cpu usage,"
		       " runtime and taskset filename.\n\n");
		print_usage();
		return 0;
	}

	/* Handle command line arguments */
	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch (c) {
		case 'b':
			options.batch_mode = 1;
			break;

		case 'c':
			get_integer(optarg, options.cpu_usage);
			break;

		case 'd':
			options.deadlock_prevention = 1;
			break;

		case 'e':
			get_integer(optarg, options.end_usage);
			break;

		case 'f':
			options.taskset_filename = optarg;
			break;

		case 'g':
			options.output_format = OUTPUT_GNUPLOT;
			break;

		case 'h':
			options.enable_hua = 1;
			break;

		case 'i':
			get_integer(optarg, options.interval);
			break;

		case 'l':
			get_integer(optarg, options.cs_length);
			options.locking |= LOCKING;
			break;

		case 'n':
			options.locking |= NESTED_LOCKING;
			break;

		case 'o':
			options.output_format = OUTPUT_LOG;
			break;

		case 'p':
			options.priority_inheritance = 1;
			break;

		case 'r':
			get_integer(optarg, options.run_time);
			break;

		case 's':
			sched_name = optarg;
			break;

		case 't':
			timing_name = optarg;
			break;

		case 'v':
			options.output_format = OUTPUT_VERBOSE;
			break;

		case 'w':
			workload_name = optarg;
			break;

		case 'x':
			options.output_format = OUTPUT_EXCEL;
			break;

		case 'z':
			options.no_run = 1;
			break;

		case '?':
			if (optopt == 'f' || optopt == 'l' || optopt == 's' ||
			    optopt == 'c' || optopt == 'r' || optopt == 'w')
				printf("Option -%c requires an argument.\n",
				       (char)optopt);
		default:
			printf("Invalid argument: '%c'\n", c);
			print_usage();
			return 0;
		}
	}

	/* If any character was used without the '-' handle them here */
	for (index = optind; index < argc; index++) {
		printf("An option was used without the preceeding '-'\n");
		print_usage();
		return 0;
	}

	//convert scheduling algorithm string into integer constant for that algo
	if (sched_name)
		options.scheduler = find_scheduler(sched_name);

	//set the workload integer constant from the workload name
	if (workload_name)
		options.workload = find_workload(workload_name);
	else
		options.workload = WORKLOAD_BURN_LOOP;

	//set the timing method from the timing string, or fallback on the default
	if (timing_name) {
		if (!strcmp(timing_name, "average"))
			options.timing_method = TIMING_AVERAGE;
		else if (!strcmp(timing_name, "wcet"))
			options.timing_method = TIMING_WCET;
		else if (!strcmp(timing_name, "timer"))
			options.timing_method = TIMING_TIMER;
		else
			fatal_error("Timing method must be one of \"average\","
				    "\"wcet\", or \"timer\"");
	} else
		options.timing_method = TIMING_TIMER;

	//Make sure the options we've collected can peacefully coexist
	if (validate_options(&options)) {
		print_usage();
		return 1;
	}
	// Lock the memory space
	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
		printf("Error: Unable to lock the memory space.\n");
		printf("Make sure you are running with sudo or root.\n");
		return 1;
	}
	//run the test using the options we've collected from the user
	run_test(&options);

	return 0;
}
