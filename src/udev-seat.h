/*
 * Copyright © 2013 Intel Corporation
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

#ifndef _UDEV_SEAT_H_
#define _UDEV_SEAT_H_

#include "config.h"

#include <libudev.h>

#include "compositor.h"

struct udev_seat {
	struct weston_seat base;
	struct wl_list devices_list;
	struct wl_listener output_create_listener;
};

struct udev_input {
	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_monitor_source;
	char *seat_id;
	struct weston_compositor *compositor;
	int enabled;
};

int udev_input_enable(struct udev_input *input, struct udev *udev);
void udev_input_disable(struct udev_input *input);
int udev_input_init(struct udev_input *input,
		    struct weston_compositor *c,
		    struct udev *udev,
		    const char *seat_id);
void udev_input_destroy(struct udev_input *input);

struct udev_seat *udev_seat_get_named(struct weston_compositor *c,
				      const char *seat_name);
#endif
