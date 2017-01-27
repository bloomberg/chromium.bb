/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Sam Spilsbury <smspillaz@gmail.com>
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

#ifndef _WESTON_TEST_RUNNER_H_
#define _WESTON_TEST_RUNNER_H_

#include "config.h"

#include <stdlib.h>

#include "shared/helpers.h"

#ifdef NDEBUG
#error "Tests must not be built with NDEBUG defined, they rely on assert()."
#endif

struct weston_test {
	const char *name;
	void (*run)(void *);
	const void *table_data;
	size_t element_size;
	int n_elements;
	int must_fail;
} __attribute__ ((aligned (32)));

#define TEST_BEGIN(name, arg)					\
	static void name(arg)

#define TEST_COMMON(func, name, ret, data, size, n_elem)	\
	static void func(void *);				\
								\
	const struct weston_test test##name			\
		__attribute__ ((section ("test_section"))) =	\
	{							\
		#name, func, data, size, n_elem, ret		\
	};

#define NO_ARG_TEST(name, ret)					\
	TEST_COMMON(wrap##name, name, ret, NULL, 0, 1)		\
	static void name(void);					\
	static void wrap##name(void *data)			\
	{							\
		(void) data;					\
		name();						\
	}							\
								\
	TEST_BEGIN(name, void)

#define ARG_TEST(name, ret, test_data)				\
	TEST_COMMON(name, name, ret, test_data,			\
		    sizeof(test_data[0]),			\
		    ARRAY_LENGTH(test_data))			\
	TEST_BEGIN(name, void *data)				\

#define TEST(name) NO_ARG_TEST(name, 0)
#define FAIL_TEST(name) NO_ARG_TEST(name, 1)
#define TEST_P(name, data) ARG_TEST(name, 0, data)
#define FAIL_TEST_P(name, data) ARG_TEST(name, 1, data)

/**
 * Get the test name string with counter
 *
 * \return The test name. For an iterated test, e.g. defined with TEST_P(),
 * the name has a '[%d]' suffix to indicate the iteration.
 *
 * This is only usable from code paths inside TEST(), TEST_P(), etc.
 * defined functions.
 */
const char *
get_test_name(void);

#endif
