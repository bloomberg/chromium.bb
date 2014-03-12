/*
 * Copyright © 2011, 2012 Intel Corporation
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

#ifndef _LIBINPUT_DEVICE_H_
#define _LIBINPUT_DEVICE_H_

#include "config.h"

#include <linux/input.h>
#include <wayland-util.h>
#include <libinput.h>

#include "compositor.h"

enum evdev_device_seat_capability {
	EVDEV_SEAT_POINTER = (1 << 0),
	EVDEV_SEAT_KEYBOARD = (1 << 1),
	EVDEV_SEAT_TOUCH = (1 << 2)
};

struct evdev_device {
	struct weston_seat *seat;
	enum evdev_device_seat_capability seat_caps;
	struct libinput_device *device;
	struct wl_list link;
	struct weston_output *output;
	struct wl_listener output_destroy_listener;
	char *devnode;
	char *output_name;
	int fd;
};

void
evdev_led_update(struct evdev_device *device, enum weston_led leds);

struct evdev_device *
evdev_device_create(struct libinput_device *libinput_device,
		    struct weston_seat *seat);

int
evdev_device_process_event(struct libinput_event *event);

void
evdev_device_set_output(struct evdev_device *device,
			struct weston_output *output);
void
evdev_device_destroy(struct evdev_device *device);

void
evdev_notify_keyboard_focus(struct weston_seat *seat,
			    struct wl_list *evdev_devices);

int
dispatch_libinput(struct libinput *libinput);

#endif /* _LIBINPUT_DEVICE_H_ */
