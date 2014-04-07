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

#ifndef _WESTON_TEST_CLIENT_HELPER_H_
#define _WESTON_TEST_CLIENT_HELPER_H_

#include "config.h"

#include <assert.h>
#include "weston-test-runner.h"
#include "wayland-test-client-protocol.h"

struct client {
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_shm *wl_shm;
	struct test *test;
	struct input *input;
	struct output *output;
	struct surface *surface;
	int has_argb;
	struct wl_list global_list;
};

struct global {
	uint32_t name;
	char *interface;
	uint32_t version;
	struct wl_list link;
};

struct test {
	struct wl_test *wl_test;
	int pointer_x;
	int pointer_y;
	uint32_t n_egl_buffers;
};

struct input {
	struct wl_seat *wl_seat;
	struct pointer *pointer;
	struct keyboard *keyboard;
};

struct pointer {
	struct wl_pointer *wl_pointer;
	struct surface *focus;
	int x;
	int y;
	uint32_t button;
	uint32_t state;
};

struct keyboard {
	struct wl_keyboard *wl_keyboard;
	struct surface *focus;
	uint32_t key;
	uint32_t state;
	uint32_t mods_depressed;
	uint32_t mods_latched;
	uint32_t mods_locked;
	uint32_t group;
};

struct output {
	struct wl_output *wl_output;
	int x;
	int y;
	int width;
	int height;
};

struct surface {
	struct wl_surface *wl_surface;
	struct wl_buffer *wl_buffer;
	struct output *output;
	int x;
	int y;
	int width;
	int height;
	void *data;
};

struct client *
client_create(int x, int y, int width, int height);

struct wl_buffer *
create_shm_buffer(struct client *client, int width, int height, void **pixels);

int
surface_contains(struct surface *surface, int x, int y);

void
move_client(struct client *client, int x, int y);

#define client_roundtrip(c) do { \
	assert(wl_display_roundtrip((c)->wl_display) >= 0); \
} while (0)

struct wl_callback *
frame_callback_set(struct wl_surface *surface, int *done);

void
frame_callback_wait(struct client *client, int *done);

int
get_n_egl_buffers(struct client *client);

void
skip(const char *fmt, ...);

#endif
