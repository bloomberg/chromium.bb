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
#include "wayland-private.h"
#include "test-runner.h"

TEST(map_insert_new)
{
	struct wl_map map;
	uint32_t i, j, k, a, b, c;

	wl_map_init(&map, WL_MAP_SERVER_SIDE);
	i = wl_map_insert_new(&map, 0, &a);
	j = wl_map_insert_new(&map, 0, &b);
	k = wl_map_insert_new(&map, 0, &c);
	assert(i == WL_SERVER_ID_START);
	assert(j == WL_SERVER_ID_START + 1);
	assert(k == WL_SERVER_ID_START + 2);

	assert(wl_map_lookup(&map, i) == &a);
	assert(wl_map_lookup(&map, j) == &b);
	assert(wl_map_lookup(&map, k) == &c);
    wl_map_release(&map);

	wl_map_init(&map, WL_MAP_CLIENT_SIDE);
	i = wl_map_insert_new(&map, 0, &a);
	assert(i == 0);
	assert(wl_map_lookup(&map, i) == &a);

	wl_map_release(&map);
}

TEST(map_insert_at)
{
	struct wl_map map;
	uint32_t a, b, c;

	wl_map_init(&map, WL_MAP_CLIENT_SIDE);
	assert(wl_map_insert_at(&map, 0, WL_SERVER_ID_START, &a) == 0);
	assert(wl_map_insert_at(&map, 0, WL_SERVER_ID_START + 3, &b) == -1);
	assert(wl_map_insert_at(&map, 0, WL_SERVER_ID_START + 1, &c) == 0);

	assert(wl_map_lookup(&map, WL_SERVER_ID_START) == &a);
	assert(wl_map_lookup(&map, WL_SERVER_ID_START + 1) == &c);

	wl_map_release(&map);
}

TEST(map_remove)
{
	struct wl_map map;
	uint32_t i, j, k, l, a, b, c, d;

	wl_map_init(&map, WL_MAP_SERVER_SIDE);
	i = wl_map_insert_new(&map, 0, &a);
	j = wl_map_insert_new(&map, 0, &b);
	k = wl_map_insert_new(&map, 0, &c);
	assert(i == WL_SERVER_ID_START);
	assert(j == WL_SERVER_ID_START + 1);
	assert(k == WL_SERVER_ID_START + 2);

	assert(wl_map_lookup(&map, i) == &a);
	assert(wl_map_lookup(&map, j) == &b);
	assert(wl_map_lookup(&map, k) == &c);

	wl_map_remove(&map, j);
	assert(wl_map_lookup(&map, j) == NULL);

	/* Verify that we insert d at the hole left by removing b */
	l = wl_map_insert_new(&map, 0, &d);
	assert(l == WL_SERVER_ID_START + 1);
	assert(wl_map_lookup(&map, l) == &d);

	wl_map_release(&map);
}

TEST(map_flags)
{
	struct wl_map map;
	uint32_t i, j, a, b;

	wl_map_init(&map, WL_MAP_SERVER_SIDE);
	i = wl_map_insert_new(&map, 0, &a);
	j = wl_map_insert_new(&map, 1, &b);
	assert(i == WL_SERVER_ID_START);
	assert(j == WL_SERVER_ID_START + 1);

	assert(wl_map_lookup(&map, i) == &a);
	assert(wl_map_lookup(&map, j) == &b);

    assert(wl_map_lookup_flags(&map, i) == 0);
    assert(wl_map_lookup_flags(&map, j) == 1);

	wl_map_release(&map);
}
