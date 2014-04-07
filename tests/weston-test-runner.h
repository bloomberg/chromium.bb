/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Sam Spilsbury <smspillaz@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WESTON_TEST_RUNNER_H_
#define _WESTON_TEST_RUNNER_H_

#include "config.h"

#include <stdlib.h>

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
		    sizeof(test_data) / sizeof (test_data[0]))	\
	TEST_BEGIN(name, void *data)				\

#define TEST(name) NO_ARG_TEST(name, 0)
#define FAIL_TEST(name) NO_ARG_TEST(name, 1)
#define TEST_P(name, data) ARG_TEST(name, 0, data)
#define FAIL_TEST_P(name, data) ARG_TEST(name, 1, data)

#endif
