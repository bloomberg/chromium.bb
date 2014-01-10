/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <assert.h>
#include "wayland-private.h"
#include "test-runner.h"

TEST(array_init)
{
	const int iterations = 4122; /* this is arbitrary */
	int i;

	/* Init array an arbitray amount of times and verify the
	 * defaults are sensible. */

	for (i = 0; i < iterations; i++) {
		struct wl_array array;
		wl_array_init(&array);
		assert(array.size == 0);
		assert(array.alloc == 0);
		assert(array.data == 0);
	}
}

TEST(array_add)
{
	struct mydata {
		unsigned int a;
		unsigned int b;
		double c;
		double d;
	};

	const unsigned int iterations = 1321; /* this is arbitrary */
	const int datasize = sizeof(struct mydata);
	struct wl_array array;
	size_t i;

	wl_array_init(&array);

	/* add some data */
	for (i = 0; i < iterations; i++) {
		struct mydata* ptr = wl_array_add(&array, datasize);
		assert(ptr);
		assert((i + 1) * datasize == array.size);

		ptr->a = i * 3;
		ptr->b = 20000 - i;
		ptr->c = (double)(i);
		ptr->d = (double)(i / 2.);
	}

	/* verify the data */
	for (i = 0; i < iterations; ++i) {
		const int index = datasize * i;
		struct mydata* check = (struct mydata*)(array.data + index);

		assert(check->a == i * 3);
		assert(check->b == 20000 - i);
		assert(check->c == (double)(i));
		assert(check->d == (double)(i/2.));
	}

	wl_array_release(&array);
}

TEST(array_copy)
{
	const int iterations = 1529; /* this is arbitrary */
	struct wl_array source;
	struct wl_array copy;
	int i;

	wl_array_init(&source);

	/* add some data */
	for (i = 0; i < iterations; i++) {
		int *p = wl_array_add(&source, sizeof(int));
		assert(p);
		*p = i * 2 + i;
	}

	/* copy the array */
	wl_array_init(&copy);
	wl_array_copy(&copy, &source);

	/* check the copy */
	for (i = 0; i < iterations; i++) {
		const int index = sizeof(int) * i;
		int *s = (int *)(source.data + index);
		int *c = (int *)(copy.data + index);

		assert(*s == *c); /* verify the values are the same */
		assert(s != c); /* ensure the addresses aren't the same */
		assert(*s == i * 2 + i); /* sanity check */
	}

	wl_array_release(&source);
	wl_array_release(&copy);
}

TEST(array_for_each)
{
	static const int elements[] = { 77, 12, 45192, 53280, 334455 };
	struct wl_array array;
	int *p, i;

	wl_array_init(&array);
	for (i = 0; i < 5; i++) {
		p = wl_array_add(&array, sizeof *p);
		assert(p);
		*p = elements[i];
	}

	i = 0;
	wl_array_for_each(p, &array)
		assert(*p == elements[i++]);
	assert(i == 5);

	wl_array_release(&array);
}
