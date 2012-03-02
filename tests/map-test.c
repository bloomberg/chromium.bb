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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../src/wayland-private.h"
#include "test-runner.h"

TEST(map_insert_new)
{
	struct wl_map map;
	uint32_t i, j, k, a, b, c;

	wl_map_init(&map);
	i = wl_map_insert_new(&map, WL_MAP_SERVER_SIDE, &a);
	j = wl_map_insert_new(&map, WL_MAP_SERVER_SIDE, &b);
	k = wl_map_insert_new(&map, WL_MAP_SERVER_SIDE, &c);
	assert(i == WL_SERVER_ID_START);
	assert(j == WL_SERVER_ID_START + 1);
	assert(k == WL_SERVER_ID_START + 2);

	assert(wl_map_lookup(&map, i) == &a);
	assert(wl_map_lookup(&map, j) == &b);
	assert(wl_map_lookup(&map, k) == &c);

	wl_map_release(&map);
}
