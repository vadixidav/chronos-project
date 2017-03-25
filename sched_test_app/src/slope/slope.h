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

#ifndef SLOPE_H
#define SLOPE_H

/*
 * The default number of iterations to initially pass to a workload to determine
 * an initial rough-estimate slope value. This is chosen to hopefully be big
 * enough to have a not-insignificant amount of time, but not take more than one
 * second to run.
 */
#define DEFAULT_AVG_SLOPE_ITERS 10000
#define DEFAULT_WCET_SLOPE_ITERS 1000

/*
 * The next three #defines control the running time and numbers of the workload
 * timing samples used to find the slope values.
 *
 * SLOPE_GEN_{MIN|MAX}_TIME control the range of the timing values used to
 * generate the slope. Note that if very small values are used here (less than
 * around 100 microseconds), the overhead of the timing mechanism comes into
 * play.
 *
 * SLOPE_GEN_SAMPLES determines how many times run the workload to find the
 * slope. The step size of each iteration is determined by the difference in the
 * two ranges divided by this value.
 */
#define SLOPE_GEN_MIN_TIME (1 * THOUSAND)	//one millisecond, in microseconds
#define SLOPE_GEN_MAX_TIME (250 * THOUSAND)	//a quarter of a second, in microseconds
#define SLOPE_GEN_SAMPLES 10

/*
 * This #defines determines the number of divisions to make between the
 * minimum WSS and maximum WSS while generating the slope for a
 * particular workload.
 */
#define SLOPE_GEN_WSS_SAMPLES 10

/*
 * Controls how long the slope generation thread gets to run without being
 * preempted during WCET slope generation.
 */
#define SLOPE_GEN_INTERRUPT_EVERY 10	//10 microseconds

/*
 * Check the slopes for this workload. If force is zero (false), only
 * regenerate the slope value if it is not already defined. If force is true,
 * attempt to regenerate the slopes for this workload.
 */
void check_slopes(int workload, const int force, const int verbose);

#endif				/* SLOPE_H */
