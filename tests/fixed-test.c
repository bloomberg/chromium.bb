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
#include <stdio.h>
#include <assert.h>
#include "../src/wayland-private.h"
#include "test-runner.h"

TEST(fixed_double_conversions)
{
	wl_fixed_t f;
	double d;

	d = 62.125;
	f = wl_fixed_from_double(d);
	fprintf(stderr, "double %lf to fixed %x\n", d, f);
	assert(f == 0x3e20);

	d = -1200.625;
	f = wl_fixed_from_double(d);
	fprintf(stderr, "double %lf to fixed %x\n", d, f);
	assert(f == -0x4b0a0);

	f = random();
	d = wl_fixed_to_double(f);
	fprintf(stderr, "fixed %x to double %lf\n", f, d);
	assert(d == f / 256.0);

	f = 0x012030;
	d = wl_fixed_to_double(f);
	fprintf(stderr, "fixed %x to double %lf\n", f, d);
	assert(d == 288.1875);

	f = 0x70000000;
	d = wl_fixed_to_double(f);
	fprintf(stderr, "fixed %x to double %lf\n", f, d);
	assert(d == f / 256);

	f = -0x012030;
	d = wl_fixed_to_double(f);
	fprintf(stderr, "fixed %x to double %lf\n", f, d);
	assert(d == -288.1875);

	f = 0x80000000;
	d = wl_fixed_to_double(f);
	fprintf(stderr, "fixed %x to double %lf\n", f, d);
	assert(d == f / 256);
}

TEST(fixed_int_conversions)
{
	wl_fixed_t f;
	int i;

	i = 62;
	f = wl_fixed_from_int(i);
	assert(f == 62 * 256);

	i = -2080;
	f = wl_fixed_from_int(i);
	assert(f == -2080 * 256);

	f = 0x277013;
	i = wl_fixed_to_int(f);
	assert(i == 0x2770);

	f = -0x5044;
	i = wl_fixed_to_int(f);
	assert(i == -0x50);
}
