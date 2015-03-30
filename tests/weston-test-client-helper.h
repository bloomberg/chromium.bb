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
#include <stdbool.h>
#include "weston-test-runner.h"
#include "weston-test-client-protocol.h"

struct client {
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_shm *wl_shm;
	struct test *test;
	/* the seat that is actually used for input events */
	struct input *input;
	/* server can have more wl_seats. We need keep them all until we
	 * find the one that we need. After that, the others
	 * will be destroyed, so this list will have the length of 1.
	 * If some day in the future we will need the other seats,
	 * we can just keep them here. */
	struct wl_list inputs;
	struct output *output;
	struct surface *surface;
	int has_argb;
	struct wl_list global_list;
	bool has_wl_drm;
};

struct global {
	uint32_t name;
	char *interface;
	uint32_t version;
	struct wl_list link;
};

struct test {
	struct weston_test *weston_test;
	int pointer_x;
	int pointer_y;
	uint32_t n_egl_buffers;
};

struct input {
	struct wl_seat *wl_seat;
	struct pointer *pointer;
	struct keyboard *keyboard;
	struct touch *touch;
	char *seat_name;
	enum wl_seat_capability caps;
	struct wl_list link;
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
	struct {
		int rate;
		int delay;
	} repeat_info;
};

struct touch {
	struct wl_touch *wl_touch;
	int down_x;
	int down_y;
	int x;
	int y;
	int id;
	int up_id; /* id of last wl_touch.up event */
	int frame_no;
	int cancel_no;
};

struct output {
	struct wl_output *wl_output;
	int x;
	int y;
	int width;
	int height;
	int scale;
	int initialized;
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

static inline void *
xzalloc(size_t size)
{
        void *p;

        p = calloc(1, size);
        assert(p);

        return p;
}

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

int
frame_callback_wait_nofail(struct client *client, int *done);

#define frame_callback_wait(c, d) assert(frame_callback_wait_nofail((c), (d)))

int
get_n_egl_buffers(struct client *client);

void
skip(const char *fmt, ...);

void
expect_protocol_error(struct client *client,
		      const struct wl_interface *intf, uint32_t code);

#endif
