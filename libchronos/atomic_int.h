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

#ifndef AINT_H
#define AINT_H

#define AINT_ADD 0
#define AINT_SUB 1
#define AINT_MUL 2
#define AINT_DIV 3
#define AINT_MOD 4

/* Simple userspace atomic non-blocking integer. For basic operations, such as
 * inc, this is _much_ faster than any locking protocol, since it uses only a
 * single CAS in the best case. This works excellently for multi-threaded
 * performance statistics
 */
typedef int atomic_int_t;

static int atomic_int_op(int op, atomic_int_t *resource, int val)
{
	int value, new_value;

	do {
		value = *resource;

		switch (op) {
		case AINT_ADD:
			new_value = value + val;
			break;
		case AINT_SUB:
			new_value = value - val;
			break;
		case AINT_MUL:
			new_value = value * val;
			break;
		case AINT_DIV:
			new_value = value / val;
			break;
		case AINT_MOD:
			new_value = value % val;
			break;
		default:
			new_value = value;
			break;
		}
	} while (__sync_val_compare_and_swap(resource, value, new_value) != value);

	return new_value;
}

static void atomic_int_init(atomic_int_t *resource) {
	*resource = 0;
}

static int atomic_int_get(atomic_int_t *resource) {
	return *resource;
}

static void atomic_int_set(atomic_int_t *resource, int val) {
	*resource = val;
}

static int atomic_int_add(atomic_int_t *resource, int val) {
	return atomic_int_op(AINT_ADD, resource, val);
}

static int atomic_int_sub(atomic_int_t *resource, int val) {
	return atomic_int_op(AINT_ADD, resource, val);
}

static int atomic_int_mul(atomic_int_t *resource, int val) {
	return atomic_int_op(AINT_ADD, resource, val);
}

static int atomic_int_div(atomic_int_t *resource, int val) {
	return atomic_int_op(AINT_ADD, resource, val);
}

static int atomic_int_inc(atomic_int_t *resource) {
	return atomic_int_op(AINT_ADD, resource, 1);
}

static int atomic_int_dec(atomic_int_t *resource) {
	return atomic_int_op(AINT_SUB, resource, 1);
}

static int atomic_int_mod(atomic_int_t *resource, int val) {
	return atomic_int_op(AINT_MOD, resource, val);
}
#endif

