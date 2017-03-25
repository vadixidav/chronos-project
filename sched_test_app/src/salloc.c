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

#define DEBUG 0

#include <sys/mman.h>
#if DEBUG
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#endif

#include "salloc.h"

/*
 * Simple shared memory allocator for sched_test_app.
 * Note that this is not extremely efficient OR thread-safe, so it
 * should only be used from the parent process for those things which
 * are absolutely necessary to be shared.
 */

/* Doubly-linked list which serves as both the freelist and headers on
 * allocated sections */
typedef struct salloc_region {
	/* The total size of this memory block, including this header, as well
	 * as whether it is free. */
	size_t __size_and_free;
	/* Pointer to previous memory block. */
	struct salloc_region *prev;
	/* Pointers to surrounding free memory blocks. This makes it easy to
	 * combine blocks on free(). Note: these pointers are only valid on
	 * free blocks, and must not be read or written on allocated blocks. */
	struct salloc_region *next_free, *prev_free;
} salloc_t;

/* Macros for accessing salloc_region implicit elements */
#define free(r) ((r)->__size_and_free & 1)	//1 if free block, 0 otherwise
#define size(r) ((r)->__size_and_free & ~1)	//size of a block, in bytes
#define set_free(r, free) ((r)->__size_and_free = size(r) | ((free) & 1))
#define set_size(r, size) ((r)->__size_and_free = free(r) | ((size) & ~1))
#define region_start(r) (&(r)->next_free)	//gets the begininning of an allocated block from its header
#define past_end(b) (b >= (salloc_t *)(((char *)head) + total_size))
#define valid_block(b) (b >= head && !past_end(b))
#define _next(r) ((salloc_t *)((char *)(r) + size(r)))
#define next(r) (past_end(_next(r)) ? head : _next(r))	//gets the implicit ->next pointer from the size

/* Get the usable size of a memory region (i.e. minus the size of that
 * part of the header which is used for allocated memory) */
#define _usable_size(size) ((size) - (size_t)region_start((salloc_t *)0))
#define usable_size(s) (_usable_size(size(s)))

/* Total size of the area from which we're allocating */
#define SALLOC_ALLOC_SIZE (1*1024*1024*1024)	//1GB
/* The smallest we'll allow a free region to become */
#define SALLOC_SMALLEST_REGION (_usable_size(sizeof(salloc_t)))

static salloc_t *head = 0;
static size_t total_size = 0;
static salloc_t *free_list = 0;

#if DEBUG
static void print_block(salloc_t * block)
{
	if (free(block))
		printf("FREE   : %p, size: %ld, fprev: %p, fnext: %p\n", block,
		       size(block), block->prev_free, block->next_free);
	else
		printf("TAKEN  : %p, size: %ld\n", block, size(block));
}

static void print_state()
{
	salloc_t *curr = head;
	printf("total_size: %ld\n", total_size);
	do {
		print_block(curr);
		curr = next(curr);
	} while (curr != head);
}
#endif

/* Insert node in list of all regions */
static void insert(salloc_t * list, salloc_t * node)
{
	node->prev = list;
	next(node)->prev = node;
}

/* Remove node from list of all regions */
static void list_remove(salloc_t * node)
{
	next(node)->prev = node->prev;
	node->prev = 0;
}

/* Insert node in list of free regions */
static void insert_free(salloc_t * list, salloc_t * node)
{
	node->prev_free = list;
	node->next_free = list->next_free;
	list->next_free = node;
	node->next_free->prev_free = node;
	set_free(node, 1);
}

/*
 * Remove node from list of free regions.
 * If @node is being removed because it is being combined with another free
 * region, @node must be the second of those two regions, but if @node is being
 * removed because it is becoming used, the free pointer should point to
 * @node->next_free instead. @combined==1 indicates that this region was
 * combined with the region before it, so free should be moved backwards if
 * necessary; the opposite is true if @combined==0.
 */
static void list_remove_free(salloc_t * node, int combined)
{
	node->prev_free->next_free = node->next_free;
	node->next_free->prev_free = node->prev_free;
	if (node == free_list) {
		if (node == node->prev_free)
			free_list = NULL;
		else if (combined)
			free_list = node->prev_free;
		else
			free_list = node->next_free;
	}
	node->prev_free = 0;
	node->next_free = 0;
	set_free(node, 0);
}

/* Check and see if the nodes on either side of us are in the free
 * list, and combine with them if they are and are contiguous */
static void coalesce(salloc_t * block)
{
	if (next(block) != head && free(next(block))) {
		/* Combine our entry with the one following us */
		salloc_t *next = next(block);
		set_size(block, size(block) + size(next));
		list_remove(next);
		list_remove_free(next, 1);
	}

	if (block != head && free(block->prev)) {
		/* Combine our entry with the one preceeding us */
		salloc_t *prev = block->prev;
		set_size(prev, size(prev) + size(block));
		list_remove(block);
		list_remove_free(block, 1);
	}
}

static int increase_heap_size()
{
	salloc_t *new;
	new = (salloc_t *) mmap(((char *)head) + total_size,
				SALLOC_ALLOC_SIZE,
				PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (new == MAP_FAILED)
		return -1;

	total_size += SALLOC_ALLOC_SIZE;
	set_size(new, SALLOC_ALLOC_SIZE);
	set_free(new, 1);

	if (!free_list) {
		free_list = new;
		new->next_free = new;
		new->prev_free = new;
	} else {
		insert_free(free_list->prev_free, new);
		coalesce(new);
	}

#if DEBUG
	printf("increase_heap_size()\n");
	print_state();
#endif

	return 0;
}

/* Initialize salloc
 * This function should only be called once, as it clears the head and free_list
 * pointers. Returns 0 on success, -1 on failure. */
static int init_salloc()
{
	//Allocate 1GB, there's no way we'll use more than that
	head = (salloc_t *) mmap(NULL, SALLOC_ALLOC_SIZE,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (head == MAP_FAILED)
		return -1;

	total_size = SALLOC_ALLOC_SIZE;
	free_list = head;
	set_size(head, SALLOC_ALLOC_SIZE);
	set_free(head, 1);
	head->prev = head;
	head->next_free = head;
	head->prev_free = head;

#if DEBUG
	print_state();
#endif

	return 0;
}

static void *_salloc(size_t size, int retry)
{
	int found = 0;
	salloc_t *curr = 0;
	size_t split_size;
	if (!head && init_salloc())
		return NULL;

	/* If there are no free segments, try to allocate more */
	if (!free_list) {
		if (retry) {
			if (increase_heap_size())
				return NULL;
			else
				return _salloc(size, retry - 1);
		} else {
			return NULL;
		}
	}

	/* Search through the free list for a block that's big enough */
	curr = free_list;
	do {
		if (usable_size(curr) >= size) {
			found = 1;
			break;
		}
		curr = curr->next_free;
	} while (curr != free_list);

	/* If we didn't find one, try to allocate more */
	if (!found) {
		if (retry) {
			if (increase_heap_size())
				return NULL;
			else
				return _salloc(size, retry - 1);
		} else {
			return NULL;
		}
	}

	/* If the free region small enough that splitting it would put it under
	 * SALLOC_SMALLEST_REGION, don't split it */
	split_size = (unsigned long)usable_size(curr) - size;
	if (_usable_size(split_size) < SALLOC_SMALLEST_REGION) {
		list_remove_free(curr, 0);
	} else {
		/* Otherwise, split the sucker */
		salloc_t *bottom_half =
		    (salloc_t *) ((char *)(region_start(curr)) + size);
		set_size(bottom_half, split_size);
		set_size(curr, size(curr) - split_size);
		set_free(bottom_half, 1);
		set_free(curr, 0);
		insert(curr, bottom_half);
		insert_free(curr, bottom_half);
		list_remove_free(curr, 0);
	}

#if DEBUG
	printf("_salloc(): %p with size %ld (ends at %p)\n", curr, size(curr),
	       (char *)curr + size(curr));
	print_state();
#endif

	return region_start(curr);
}

/*
 * Allocate memory from the shared memory region. Returns a pointer to the
 * allocated region of memory on success and NULL on failure.
 */
void *salloc(size_t size)
{
	/* Align size to multiple of SALLOC_SMALLEST_REGION */
	if (size < SALLOC_SMALLEST_REGION)
		size = SALLOC_SMALLEST_REGION;
	else if (size % SALLOC_SMALLEST_REGION)
		size += size % SALLOC_SMALLEST_REGION;

	return _salloc(size, 1);
}

/*
 * Frees a block of memory allocated via a call to salloc(). sfree() may only be
 * called once per call to salloc(), and may only be called with the pointer
 * passed back from salloc().
 */
void sfree(void *ptr)
{
	salloc_t *free, *curr;

#if DEBUG
	assert(ptr);
#endif
	free = (salloc_t *) ((char *)ptr
			     - (char *)region_start((salloc_t *) 0));

#if DEBUG
	printf("sfree(): %p with size %ld (ends at %p)\n", free, size(free),
	       (char *)free + size(free));
#endif

	/* Add us back to the free list. First, find the first free block which
	 * comes before us element in the list. We start at free and search
	 * through the list of all blocks rather than search starting at
	 * free_list because this way we (hopefully) start closer to where we
	 * want to be. */
	curr = free->prev;
	do {
		if (free(curr))
			break;
		curr = curr->prev;
	} while (curr != free);

	/* There are no other free segments, re-initialize the free list */
	if (curr == free) {
		free_list = free;
		free->next_free = free;
		free->prev_free = free;
		set_free(curr, 1);
	} else {
		/* If we found the previous item in the free list, add us after it. */
		insert_free(curr, free);
		coalesce(free);
	}

#if DEBUG
	print_state();
#endif
}
