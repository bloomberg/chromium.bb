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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "compositor.h"
#include "launcher-util.h"
#include "evdev.h"
#include "udev-seat.h"

static const char default_seat[] = "seat0";
static const char default_seat_name[] = "default";

static void
udev_seat_destroy(struct udev_seat *seat);

static int
device_added(struct udev_device *udev_device, struct udev_input *input)
{
	struct weston_compositor *c;
	struct evdev_device *device;
	struct weston_output *output;
	const char *devnode;
	const char *device_seat, *seat_name, *output_name;
	const char *calibration_values;
	int fd;
	struct udev_seat *seat;

	device_seat = udev_device_get_property_value(udev_device, "ID_SEAT");
	if (!device_seat)
		device_seat = default_seat;

	if (strcmp(device_seat, input->seat_id))
		return 0;

	c = input->compositor;
	devnode = udev_device_get_devnode(udev_device);

	/* Search for matching logical seat */
	seat_name = udev_device_get_property_value(udev_device, "WL_SEAT");
	if (!seat_name)
		seat_name = default_seat_name;

	seat = udev_seat_get_named(input, seat_name);

	if (seat == NULL)
		return -1;

	/* Use non-blocking mode so that we can loop on read on
	 * evdev_device_data() until all events on the fd are
	 * read.  mtdev_get() also expects this. */
	fd = weston_launcher_open(c->launcher, devnode, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		weston_log("opening input device '%s' failed.\n", devnode);
		return 0;
	}

	device = evdev_device_create(&seat->base, devnode, fd);
	if (device == EVDEV_UNHANDLED_DEVICE) {
		weston_launcher_close(c->launcher, fd);
		weston_log("not using input device '%s'.\n", devnode);
		return 0;
	} else if (device == NULL) {
		weston_launcher_close(c->launcher, fd);
		weston_log("failed to create input device '%s'.\n", devnode);
		return 0;
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

	wl_list_insert(seat->devices_list.prev, &device->link);

	if (seat->base.output && seat->base.pointer)
		weston_pointer_clamp(seat->base.pointer,
				     &seat->base.pointer->x,
				     &seat->base.pointer->y);

	output_name = udev_device_get_property_value(udev_device, "WL_OUTPUT");
	if (output_name) {
		device->output_name = strdup(output_name);
		wl_list_for_each(output, &c->output_list, link)
			if (strcmp(output->name, device->output_name) == 0)
				evdev_device_set_output(device, output);
	}

	if (device->output == NULL) {
		output = container_of(c->output_list.next,
				      struct weston_output, link);
		evdev_device_set_output(device, output);
	}

	if (input->enabled == 1)
		weston_seat_repick(&seat->base);

	return 0;
}

static int
udev_input_add_devices(struct udev_input *input, struct udev *udev)
{
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path, *sysname;
	struct udev_seat *seat;
	int devices_found = 0;

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

		if (device_added(device, input) < 0) {
			udev_device_unref(device);
			udev_enumerate_unref(e);
			return -1;
		}

		udev_device_unref(device);
	}
	udev_enumerate_unref(e);

	wl_list_for_each(seat, &input->compositor->seat_list, base.link) {
		evdev_notify_keyboard_focus(&seat->base, &seat->devices_list);

		if (!wl_list_empty(&seat->devices_list))
			devices_found = 1;
	}

	if (devices_found == 0) {
		weston_log(
			"warning: no input devices on entering Weston. "
			"Possible causes:\n"
			"\t- no permissions to read /dev/input/event*\n"
			"\t- seats misconfigured "
			"(Weston backend option 'seat', "
			"udev device property ID_SEAT)\n");
		return -1;
	}

	return 0;
}

static int
evdev_udev_handler(int fd, uint32_t mask, void *data)
{
	struct udev_input *input = data;
	struct udev_device *udev_device;
	struct evdev_device *device, *next;
	const char *action;
	const char *devnode;
	struct udev_seat *seat;

	udev_device = udev_monitor_receive_device(input->udev_monitor);
	if (!udev_device)
		return 1;

	action = udev_device_get_action(udev_device);
	if (!action)
		goto out;

	if (strncmp("event", udev_device_get_sysname(udev_device), 5) != 0)
		goto out;

	if (!strcmp(action, "add")) {
		device_added(udev_device, input);
	}
	else if (!strcmp(action, "remove")) {
		devnode = udev_device_get_devnode(udev_device);
		wl_list_for_each(seat, &input->compositor->seat_list, base.link) {
			wl_list_for_each_safe(device, next, &seat->devices_list, link)
				if (!strcmp(device->devnode, devnode)) {
					weston_log("input device %s, %s removed\n",
							device->devname, device->devnode);
					weston_launcher_close(input->compositor->launcher,
							      device->fd);
					evdev_device_destroy(device);
					break;
				}
		}
	}

out:
	udev_device_unref(udev_device);

	return 0;
}

int
udev_input_enable(struct udev_input *input)
{
	struct wl_event_loop *loop;
	struct weston_compositor *c = input->compositor;
	int fd;

	input->udev_monitor = udev_monitor_new_from_netlink(input->udev, "udev");
	if (!input->udev_monitor) {
		weston_log("udev: failed to create the udev monitor\n");
		return -1;
	}

	udev_monitor_filter_add_match_subsystem_devtype(input->udev_monitor,
			"input", NULL);

	if (udev_monitor_enable_receiving(input->udev_monitor)) {
		weston_log("udev: failed to bind the udev monitor\n");
		udev_monitor_unref(input->udev_monitor);
		return -1;
	}

	loop = wl_display_get_event_loop(c->wl_display);
	fd = udev_monitor_get_fd(input->udev_monitor);
	input->udev_monitor_source =
		wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
				     evdev_udev_handler, input);
	if (!input->udev_monitor_source) {
		udev_monitor_unref(input->udev_monitor);
		return -1;
	}

	if (udev_input_add_devices(input, input->udev) < 0)
		return -1;

	input->enabled = 1;

	return 0;
}

static void
udev_input_remove_devices(struct udev_input *input)
{
	struct evdev_device *device, *next;
	struct udev_seat *seat;

	wl_list_for_each(seat, &input->compositor->seat_list, base.link) {
		wl_list_for_each_safe(device, next, &seat->devices_list, link) {
			weston_launcher_close(input->compositor->launcher,
					      device->fd);
			evdev_device_destroy(device);
		}

		if (seat->base.keyboard)
			notify_keyboard_focus_out(&seat->base);
	}
}

void
udev_input_disable(struct udev_input *input)
{
	if (!input->udev_monitor)
		return;

	udev_monitor_unref(input->udev_monitor);
	input->udev_monitor = NULL;
	wl_event_source_remove(input->udev_monitor_source);
	input->udev_monitor_source = NULL;

	udev_input_remove_devices(input);
}


int
udev_input_init(struct udev_input *input, struct weston_compositor *c, struct udev *udev,
		const char *seat_id)
{
	memset(input, 0, sizeof *input);
	input->seat_id = strdup(seat_id);
	input->compositor = c;
	input->udev = udev;
	input->udev = udev_ref(udev);
	if (udev_input_enable(input) < 0)
		goto err;

	return 0;

 err:
	free(input->seat_id);
	return -1;
}

void
udev_input_destroy(struct udev_input *input)
{
	struct udev_seat *seat, *next;
	udev_input_disable(input);
	wl_list_for_each_safe(seat, next, &input->compositor->seat_list, base.link)
		udev_seat_destroy(seat);
	udev_unref(input->udev);
	free(input->seat_id);
}

static void
drm_led_update(struct weston_seat *seat_base, enum weston_led leds)
{
	struct udev_seat *seat = (struct udev_seat *) seat_base;
	struct evdev_device *device;

	wl_list_for_each(device, &seat->devices_list, link)
		evdev_led_update(device, leds);
}

static void
notify_output_create(struct wl_listener *listener, void *data)
{
	struct udev_seat *seat = container_of(listener, struct udev_seat,
					      output_create_listener);
	struct evdev_device *device;
	struct weston_output *output = data;

	wl_list_for_each(device, &seat->devices_list, link)
		if (device->output_name &&
		    strcmp(output->name, device->output_name) == 0) {
			evdev_device_set_output(device, output);
			break;
		}
}

static struct udev_seat *
udev_seat_create(struct udev_input *input, const char *seat_name)
{
	struct weston_compositor *c = input->compositor;
	struct udev_seat *seat;

	seat = zalloc(sizeof *seat);

	if (!seat)
		return NULL;
	weston_seat_init(&seat->base, c, seat_name);
	seat->base.led_update = drm_led_update;

	seat->output_create_listener.notify = notify_output_create;
	wl_signal_add(&c->output_created_signal,
		      &seat->output_create_listener);

	wl_list_init(&seat->devices_list);
	return seat;
}

static void
udev_seat_destroy(struct udev_seat *seat)
{
	weston_seat_release(&seat->base);
	wl_list_remove(&seat->output_create_listener.link);
	free(seat);
}

struct udev_seat *
udev_seat_get_named(struct udev_input *input, const char *seat_name)
{
	struct weston_compositor *c = input->compositor;
	struct udev_seat *seat;

	wl_list_for_each(seat, &c->seat_list, base.link) {
		if (strcmp(seat->base.seat_name, seat_name) == 0)
			return seat;
	}

	seat = udev_seat_create(input, seat_name);

	if (!seat)
		return NULL;

	return seat;
}
