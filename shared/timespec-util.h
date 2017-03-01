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

/* Add a nanosecond value to a timespec
 *
 * \param r[out] result: a + b
 * \param a[in] base operand as timespec
 * \param b[in] operand in nanoseconds
 */
static inline void
timespec_add_nsec(struct timespec *r, const struct timespec *a, int64_t b)
{
	r->tv_sec = a->tv_sec + (b / NSEC_PER_SEC);
	r->tv_nsec = a->tv_nsec + (b % NSEC_PER_SEC);

	if (r->tv_nsec >= NSEC_PER_SEC) {
		r->tv_sec++;
		r->tv_nsec -= NSEC_PER_SEC;
	} else if (r->tv_nsec < 0) {
		r->tv_sec--;
		r->tv_nsec += NSEC_PER_SEC;
	}
}

/* Add a millisecond value to a timespec
 *
 * \param r[out] result: a + b
 * \param a[in] base operand as timespec
 * \param b[in] operand in milliseconds
 */
static inline void
timespec_add_msec(struct timespec *r, const struct timespec *a, int64_t b)
{
	return timespec_add_nsec(r, a, b * 1000000);
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

/* Subtract timespecs and return result in nanoseconds
 *
 * \param a[in] operand
 * \param b[in] operand
 * \return to_nanoseconds(a - b)
 */
static inline int64_t
timespec_sub_to_nsec(const struct timespec *a, const struct timespec *b)
{
	struct timespec r;
	timespec_sub(&r, a, b);
	return timespec_to_nsec(&r);
}

/* Convert timespec to milliseconds
 *
 * \param a timespec
 * \return milliseconds
 *
 * Rounding to integer milliseconds happens always down (floor()).
 */
static inline int64_t
timespec_to_msec(const struct timespec *a)
{
	return (int64_t)a->tv_sec * 1000 + a->tv_nsec / 1000000;
}

/* Subtract timespecs and return result in milliseconds
 *
 * \param a[in] operand
 * \param b[in] operand
 * \return to_milliseconds(a - b)
 */
static inline int64_t
timespec_sub_to_msec(const struct timespec *a, const struct timespec *b)
{
	return timespec_sub_to_nsec(a, b) / 1000000;
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
