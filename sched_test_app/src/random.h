/*
 * This is an implementation of pseudo-random number generation using the Linear
 * feedback shift register approach. Code and appropriate 'taps' values were
 * obtained from http://en.wikipedia.org/wiki/Linear_feedback_shift_register and
 * http://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
 */

#include <stdint.h>
#include <time.h>

#ifndef RANDOM_H
#define RANDOM_H

/* These 'taps' values produce pseudo-random series with maximal periods for
 * their numbers of bits, according to the sources listed above. I have verified
 * this with a few runs for 32-bits, but didn't want to take the approximately
 * 3678 years it would've taken to verify it for 64 bits on this computer. */
#define LSFR_TAPS_64 0xd800000000000000u
#define LSFR_TAPS_32 0x80200003u
#define lfsr_next(lfsr, taps) ((lfsr) >> 1) ^ (-((lfsr) & 1u) & (taps))

/* Advance the random value and return it */
static inline uint64_t lfsrandom64(uint64_t * r)
{
	return *r = lfsr_next(*r, LSFR_TAPS_64);
}

/* Advance the random value and return it */
static inline uint32_t lfsrandom32(uint32_t * r)
{
	return *r = lfsr_next(*r, LSFR_TAPS_32);
}

/* Seed the random value with the current time in nanoseconds */
static inline void init_lfsrandom64(uint64_t * r)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	*r = (uint64_t) now.tv_nsec;
}

/* Seed the random value with the current time in nanoseconds */
static inline void init_lfsrandom32(uint32_t * r)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	*r = (uint32_t) now.tv_nsec;
}

#endif				/* RANDOM_H */
