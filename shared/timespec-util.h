/*
 * Copyright © 2014 - 2015 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef TIMESPEC_UTIL_H
#define TIMESPEC_UTIL_H

#include <stdint.h>
#include <assert.h>

#define NSEC_PER_SEC 1000000000

/* Subtract timespecs
 *
 * \param r[out] result: a - b
 * \param a[in] operand
 * \param b[in] operand
 */
static inline void
timespec_sub(struct timespec *r,
	     const struct timespec *a, const struct timespec *b)
{
	r->tv_sec = a->tv_sec - b->tv_sec;
	r->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (r->tv_nsec < 0) {
		r->tv_sec--;
		r->tv_nsec += NSEC_PER_SEC;
	}
}

/* Convert timespec to nanoseconds
 *
 * \param a timespec
 * \return nanoseconds
 */
static inline int64_t
timespec_to_nsec(const struct timespec *a)
{
	return (int64_t)a->tv_sec * NSEC_PER_SEC + a->tv_nsec;
}

/* Convert milli-Hertz to nanoseconds
 *
 * \param mhz frequency in mHz, not zero
 * \return period in nanoseconds
 */
static inline int64_t
millihz_to_nsec(uint32_t mhz)
{
	assert(mhz > 0);
	return 1000000000000LL / mhz;
}

#endif /* TIMESPEC_UTIL_H */
