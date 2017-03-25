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

/*
 * This file provides functions to write the workload slope out to disk
 * and fetch the workload slope while running tests. For the utility
 * that generates the slope, see src/slope/slope.c
 */

#include "workload.h"

/*
 * Build the filename for a slope file based on the workload and timing method
 * and store the result in the character array passed in.
 */
static void get_slope_filename(char *filename, const int workload_num,
			       const int timing_method)
{
	char timing_average[] = "average";
	char timing_wcet[] = "wcet";
	const char *timing, *workload = get_workload_name(workload_num);
	assert(workload);

	if (timing_method == TIMING_AVERAGE)
		timing = timing_average;
	else
		timing = timing_wcet;

	sprintf(filename, "/usr/local/chronos/slope/%s_%s.conf", workload,
		timing);
}

/*
 * Check and see if the slope file for the specified workload/timing_method
 * combination is accessible.
 */
int workload_slope_file_accessible(const int workload, const int timing_method)
{
	int exists, accessible, creatable;
	char filename[100];
	FILE *f;

	get_slope_filename(filename, workload, timing_method);	//get the filename from the workload and timing_method

	exists = access(filename, F_OK) != -1;	//see if it exists
	accessible = access(filename, W_OK | R_OK) != -1;	//see if we can access it for reading and writing

	//see if we can create the file, if it doesn't already exist
	if (!exists) {
		if ((f = fopen(filename, "w+"))) {
			fclose(f);
			creatable = 1;
		} else
			creatable = 0;
	}
	//return true if the file exists and we can access it, or if it doesn't exist
	//and we are capable of creating it
	return (exists && accessible) || (!exists && creatable);
}

/*
 * Clear the workload slope file. This is used when re-generating the slope,
 * since we add slope values incrementally via put_workload_slope()
 */
int clear_workload_slope(const int workload, const int timing_method)
{
	char filename[100];
	FILE *f;

	get_slope_filename(filename, workload, timing_method);	//get the filename from the workload and timing_method

	f = fopen(filename, "w");
	if (!f)
		return -1;

	fclose(f);
	return 0;
}

/*
 * Read the slope from the file for the specified workload/timing_method
 * combination. Returns -1.0 on failure, or a positive value on success.
 */
double get_workload_slope(const int workload, const int timing_method,
			  const unsigned int task_wss,
			  const unsigned int group_wss)
{
	char filename[100];
	FILE *f;
	double exec_slope, percentage_between;
	struct workload *w = get_workload_struct(workload);

	long target_wss;
	long this_wss, lower_wss, higher_wss;
	double this_slope, lower_slope = -1.0, higher_slope = -1.0;

	//only search for the workload size that's actually being used
	target_wss = 0;
	if (w->capabilities & WORKLOAD_CAP_TASK_WSS)
		target_wss += task_wss;
	if (w->capabilities & WORKLOAD_CAP_GROUP_WSS)
		target_wss += group_wss;

	get_slope_filename(filename, workload, timing_method);	//get the filename from the workload and timing_method

	f = fopen(filename, "r");
	if (!f)
		return -1.0;

	//search through the file for the two closest slope values for WSSes just above and below ours
	while (fscanf(f, "%ld %lf\n", &this_wss, &this_slope) == 2) {
		if (this_wss == target_wss) {
			exec_slope = this_slope;
			goto out;
		} else if (this_wss < target_wss &&
			   (lower_slope < 0 || this_wss > lower_wss)) {
			lower_wss = this_wss;
			lower_slope = this_slope;
		} else if (this_wss > target_wss &&
			   (higher_slope < 0 || this_wss < higher_wss)) {
			higher_wss = this_wss;
			higher_slope = this_slope;
		}
	}

	if (higher_slope < 0 || lower_slope < 0) {
		exec_slope = -1.0;
		goto out;
	}
	//calculate our slope using linear interpolation between the lower and higher slope values
	percentage_between =
	    (double)(target_wss - lower_wss) / (higher_wss - lower_wss);
	exec_slope =
	    lower_slope + percentage_between * (higher_slope - lower_slope);

 out:
	fclose(f);
	return exec_slope;
}

/*
 * Put a new slope into the slope file for the specified workload/timing_method combination.
 */
int put_workload_slope(const int workload, const int timing_method,
		       const double slope, const unsigned int task_wss,
		       const unsigned int group_wss)
{
	char filename[100];
	FILE *f;
	struct workload *w = get_workload_struct(workload);

	long real_wss;

	//only search for the workload size that's actually being used
	real_wss = 0;
	if (w->capabilities & WORKLOAD_CAP_TASK_WSS)
		real_wss += task_wss;
	if (w->capabilities & WORKLOAD_CAP_GROUP_WSS)
		real_wss += group_wss;

	get_slope_filename(filename, workload, timing_method);	//get the filename from the workload and timing_method

	f = fopen(filename, "a");
	if (!f)
		return -1;

	fprintf(f, "%ld %lf\n", real_wss, slope);
	fclose(f);

	return 0;
}
