/*
 * Copyright © 2012 Intel Corporation
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

#ifdef NDEBUG
#error "Tests must not be built with NDEBUG defined, they rely on assert()."
#endif

struct weston_test {
	const char *name;
	void (*run)(void);
	int must_fail;
} __attribute__ ((aligned (16)));

#define TEST(name)						\
	static void name(void);					\
								\
	const struct weston_test test##name			\
		 __attribute__ ((section ("test_section"))) = {	\
		#name, name, 0					\
	};							\
								\
	static void name(void)

#define FAIL_TEST(name)						\
	static void name(void);					\
								\
	const struct weston_test test##name			\
		 __attribute__ ((section ("test_section"))) = {	\
		#name, name, 1					\
	};							\
								\
	static void name(void)

#endif
