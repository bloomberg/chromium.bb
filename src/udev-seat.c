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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "compositor.h"
#include "launcher-util.h"
#include "evdev.h"
#include "udev-seat.h"

static const char default_seat[] = "seat0";

static int
device_added(struct udev_device *udev_device, struct udev_seat *master)
{
	struct weston_compositor *c;
	struct evdev_device *device;
	const char *devnode;
	const char *device_seat;
	const char *calibration_values;
	int fd;

	device_seat = udev_device_get_property_value(udev_device, "ID_SEAT");
	if (!device_seat)
		device_seat = default_seat;

	if (strcmp(device_seat, master->seat_id))
		return 0;

	c = master->base.compositor;
	devnode = udev_device_get_devnode(udev_device);

	/* Use non-blocking mode so that we can loop on read on
	 * evdev_device_data() until all events on the fd are
	 * read.  mtdev_get() also expects this. */
	fd = weston_launcher_open(c, devnode, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		weston_log("opening input device '%s' failed.\n", devnode);
		return -1;
	}

	device = evdev_device_create(&master->base, devnode, fd);
	if (device == EVDEV_UNHANDLED_DEVICE) {
		close(fd);
		weston_log("not using input device '%s'.\n", devnode);
		return 0;
	} else if (device == NULL) {
		close(fd);
		weston_log("failed to create input device '%s'.\n", devnode);
		return -1;
	}

	calibration_values =
		udev_device_get_property_value(udev_device,
					       "WL_CALIBRATION");

	if (calibration_values && sscanf(calibration_values,
					 "%f %f %f %f %f %f",
					 &device->abs.calibration[0],
					 &device->abs.calibration[1],
					 &device->abs.calibration[2],
					 &device->abs.calibration[3],
					 &device->abs.calibration[4],
					 &device->abs.calibration[5]) == 6) {
		device->abs.apply_calibration = 1;
		weston_log ("Applying calibration: %f %f %f %f %f %f\n",
			    device->abs.calibration[0],
			    device->abs.calibration[1],
			    device->abs.calibration[2],
			    device->abs.calibration[3],
			    device->abs.calibration[4],
			    device->abs.calibration[5]);
	}

	wl_list_insert(master->devices_list.prev, &device->link);

	return 0;
}

static int
udev_seat_add_devices(struct udev_seat *seat, struct udev *udev)
{
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path, *sysname;

	e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "input");
	udev_enumerate_scan_devices(e);
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(udev, path);

		sysname = udev_device_get_sysname(device);
		if (strncmp("event", sysname, 5) != 0) {
			udev_device_unref(device);
			continue;
		}

		if (device_added(device, seat) < 0) {
			udev_device_unref(device);
			udev_enumerate_unref(e);
			return -1;
		}

		udev_device_unref(device);
	}
	udev_enumerate_unref(e);

	evdev_notify_keyboard_focus(&seat->base, &seat->devices_list);

	if (wl_list_empty(&seat->devices_list)) {
		weston_log(
			"warning: no input devices on entering Weston. "
			"Possible causes:\n"
			"\t- no permissions to read /dev/input/event*\n"
			"\t- seats misconfigured "
			"(Weston backend option 'seat', "
			"udev device property ID_SEAT)\n");
	}

	return 0;
}

static int
evdev_udev_handler(int fd, uint32_t mask, void *data)
{
	struct udev_seat *seat = data;
	struct udev_device *udev_device;
	struct evdev_device *device, *next;
	const char *action;
	const char *devnode;

	udev_device = udev_monitor_receive_device(seat->udev_monitor);
	if (!udev_device)
		return 1;

	action = udev_device_get_action(udev_device);
	if (!action)
		goto out;

	if (strncmp("event", udev_device_get_sysname(udev_device), 5) != 0)
		goto out;

	if (!strcmp(action, "add")) {
		device_added(udev_device, seat);
	}
	else if (!strcmp(action, "remove")) {
		devnode = udev_device_get_devnode(udev_device);
		wl_list_for_each_safe(device, next, &seat->devices_list, link)
			if (!strcmp(device->devnode, devnode)) {
				weston_log("input device %s, %s removed\n",
					   device->devname, device->devnode);
				evdev_device_destroy(device);
				break;
			}
	}

out:
	udev_device_unref(udev_device);

	return 0;
}

int
udev_seat_enable(struct udev_seat *seat, struct udev *udev)
{
	struct wl_event_loop *loop;
	struct weston_compositor *c = seat->base.compositor;
	int fd;

	seat->udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
	if (!seat->udev_monitor) {
		weston_log("udev: failed to create the udev monitor\n");
		return -1;
	}

	udev_monitor_filter_add_match_subsystem_devtype(seat->udev_monitor,
			"input", NULL);

	if (udev_monitor_enable_receiving(seat->udev_monitor)) {
		weston_log("udev: failed to bind the udev monitor\n");
		udev_monitor_unref(seat->udev_monitor);
		return -1;
	}

	loop = wl_display_get_event_loop(c->wl_display);
	fd = udev_monitor_get_fd(seat->udev_monitor);
	seat->udev_monitor_source =
		wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
				     evdev_udev_handler, seat);
	if (!seat->udev_monitor_source) {
		udev_monitor_unref(seat->udev_monitor);
		return -1;
	}

	if (udev_seat_add_devices(seat, udev) < 0)
		return -1;

	return 0;
}

static void
udev_seat_remove_devices(struct udev_seat *seat)
{
	struct evdev_device *device, *next;

	wl_list_for_each_safe(device, next, &seat->devices_list, link)
		evdev_device_destroy(device);

	if (seat->base.seat.keyboard)
		notify_keyboard_focus_out(&seat->base);
}

void
udev_seat_disable(struct udev_seat *seat)
{
	if (!seat->udev_monitor)
		return;

	udev_monitor_unref(seat->udev_monitor);
	seat->udev_monitor = NULL;
	wl_event_source_remove(seat->udev_monitor_source);
	seat->udev_monitor_source = NULL;

	udev_seat_remove_devices(seat);
}

static void
drm_led_update(struct weston_seat *seat_base, enum weston_led leds)
{
	struct udev_seat *seat = (struct udev_seat *) seat_base;
	struct evdev_device *device;

	wl_list_for_each(device, &seat->devices_list, link)
		evdev_led_update(device, leds);
}

struct udev_seat *
udev_seat_create(struct weston_compositor *c, struct udev *udev,
		const char *seat_id)
{
	struct udev_seat *seat;

	seat = malloc(sizeof *seat);
	if (seat == NULL)
		return NULL;

	memset(seat, 0, sizeof *seat);
	weston_seat_init(&seat->base, c);
	seat->base.led_update = drm_led_update;

	wl_list_init(&seat->devices_list);
	seat->seat_id = strdup(seat_id);
	if (udev_seat_enable(seat, udev) < 0)
		goto err;

	return seat;

 err:
	free(seat->seat_id);
	free(seat);
	return NULL;
}

void
udev_seat_destroy(struct udev_seat *seat)
{
	udev_seat_disable(seat);

	weston_seat_release(&seat->base);
	free(seat->seat_id);
	free(seat);
}
