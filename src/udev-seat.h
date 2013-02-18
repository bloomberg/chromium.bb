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

#include <libudev.h>

#include "compositor.h"

struct udev_seat {
	struct weston_seat base;
	struct wl_list devices_list;
	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_monitor_source;
	char *seat_id;
};

int udev_seat_add_devices(struct udev_seat *seat, struct udev *udev);
int udev_seat_enable_udev_monitor(struct udev_seat *seat, struct udev *udev);
void udev_seat_disable_udev_monitor(struct udev_seat *seat);
struct udev_seat *udev_seat_create(struct weston_compositor *c,
				   struct udev *udev,
				   const char *seat_id);
void udev_seat_remove_devices(struct udev_seat *seat);
void udev_seat_destroy(struct udev_seat *seat);

#endif
