/*
 * Copyright © 2010 Intel Corporation
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
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <mtdev.h>

#include "compositor.h"
#include "evdev.h"

#define DEFAULT_AXIS_STEP_DISTANCE wl_fixed_from_int(10)

void
evdev_led_update(struct evdev_device *device, enum weston_led leds)
{
	static const struct {
		enum weston_led weston;
		int evdev;
	} map[] = {
		{ LED_NUM_LOCK, LED_NUML },
		{ LED_CAPS_LOCK, LED_CAPSL },
		{ LED_SCROLL_LOCK, LED_SCROLLL },
	};
	struct input_event ev[ARRAY_LENGTH(map)];
	unsigned int i;

	if (!device->caps & EVDEV_KEYBOARD)
		return;

	memset(ev, 0, sizeof(ev));
	for (i = 0; i < ARRAY_LENGTH(map); i++) {
		ev[i].type = EV_LED;
		ev[i].code = map[i].evdev;
		ev[i].value = !!(leds & map[i].weston);
	}

	i = write(device->fd, ev, sizeof ev);
	(void)i; /* no, we really don't care about the return value */
}

static inline void
evdev_process_key(struct evdev_device *device, struct input_event *e, int time)
{
	if (e->value == 2)
		return;

	switch (e->code) {
	case BTN_LEFT:
	case BTN_RIGHT:
	case BTN_MIDDLE:
	case BTN_SIDE:
	case BTN_EXTRA:
	case BTN_FORWARD:
	case BTN_BACK:
	case BTN_TASK:
		notify_button(device->seat,
			      time, e->code,
			      e->value ? WL_POINTER_BUTTON_STATE_PRESSED :
					 WL_POINTER_BUTTON_STATE_RELEASED);
		break;

	default:
		notify_key(device->seat,
			   time, e->code,
			   e->value ? WL_KEYBOARD_KEY_STATE_PRESSED :
				      WL_KEYBOARD_KEY_STATE_RELEASED,
			   STATE_UPDATE_AUTOMATIC);
		break;
	}
}

static void
evdev_process_touch(struct evdev_device *device, struct input_event *e)
{
	const int screen_width = device->output->current->width;
	const int screen_height = device->output->current->height;

	switch (e->code) {
	case ABS_MT_SLOT:
		device->mt.slot = e->value;
		break;
	case ABS_MT_TRACKING_ID:
		if (e->value >= 0)
			device->pending_events |= EVDEV_ABSOLUTE_MT_DOWN;
		else
			device->pending_events |= EVDEV_ABSOLUTE_MT_UP;
		break;
	case ABS_MT_POSITION_X:
		device->mt.x[device->mt.slot] =
			(e->value - device->abs.min_x) * screen_width /
			(device->abs.max_x - device->abs.min_x) +
			device->output->x;
		device->pending_events |= EVDEV_ABSOLUTE_MT_MOTION;
		break;
	case ABS_MT_POSITION_Y:
		device->mt.y[device->mt.slot] =
			(e->value - device->abs.min_y) * screen_height /
			(device->abs.max_y - device->abs.min_y) +
			device->output->y;
		device->pending_events |= EVDEV_ABSOLUTE_MT_MOTION;
		break;
	}
}

static inline void
evdev_process_absolute_motion(struct evdev_device *device,
			      struct input_event *e)
{
	const int screen_width = device->output->current->width;
	const int screen_height = device->output->current->height;

	switch (e->code) {
	case ABS_X:
		device->abs.x =
			(e->value - device->abs.min_x) * screen_width /
			(device->abs.max_x - device->abs.min_x) +
			device->output->x;
		device->pending_events |= EVDEV_ABSOLUTE_MOTION;
		break;
	case ABS_Y:
		device->abs.y =
			(e->value - device->abs.min_y) * screen_height /
			(device->abs.max_y - device->abs.min_y) +
			device->output->y;
		device->pending_events |= EVDEV_ABSOLUTE_MOTION;
		break;
	}
}

static inline void
evdev_process_relative(struct evdev_device *device,
		       struct input_event *e, uint32_t time)
{
	switch (e->code) {
	case REL_X:
		device->rel.dx += wl_fixed_from_int(e->value);
		device->pending_events |= EVDEV_RELATIVE_MOTION;
		break;
	case REL_Y:
		device->rel.dy += wl_fixed_from_int(e->value);
		device->pending_events |= EVDEV_RELATIVE_MOTION;
		break;
	case REL_WHEEL:
		switch (e->value) {
		case -1:
			/* Scroll down */
		case 1:
			/* Scroll up */
			notify_axis(device->seat,
				    time,
				    WL_POINTER_AXIS_VERTICAL_SCROLL,
				    -1 * e->value * DEFAULT_AXIS_STEP_DISTANCE);
			break;
		default:
			break;
		}
		break;
	case REL_HWHEEL:
		switch (e->value) {
		case -1:
			/* Scroll left */
		case 1:
			/* Scroll right */
			notify_axis(device->seat,
				    time,
				    WL_POINTER_AXIS_HORIZONTAL_SCROLL,
				    e->value * DEFAULT_AXIS_STEP_DISTANCE);
			break;
		default:
			break;

		}
	}
}

static inline void
evdev_process_absolute(struct evdev_device *device, struct input_event *e)
{
	if (device->is_mt) {
		evdev_process_touch(device, e);
	} else {
		evdev_process_absolute_motion(device, e);
	}
}

static int
is_motion_event(struct input_event *e)
{
	switch (e->type) {
	case EV_REL:
		switch (e->code) {
		case REL_X:
		case REL_Y:
			return 1;
		}
		break;
	case EV_ABS:
		switch (e->code) {
		case ABS_X:
		case ABS_Y:
		case ABS_MT_POSITION_X:
		case ABS_MT_POSITION_Y:
			return 1;
		}
	}

	return 0;
}

static void
transform_absolute(struct evdev_device *device)
{
	if (!device->abs.apply_calibration)
		return;

	device->abs.x = device->abs.x * device->abs.calibration[0] +
			device->abs.y * device->abs.calibration[1] +
			device->abs.calibration[2];

	device->abs.y = device->abs.x * device->abs.calibration[3] +
			device->abs.y * device->abs.calibration[4] +
			device->abs.calibration[5];
}

static void
evdev_flush_motion(struct evdev_device *device, uint32_t time)
{
	struct weston_seat *master = device->seat;

	if (!(device->pending_events & EVDEV_SYN))
		return;

	device->pending_events &= ~EVDEV_SYN;
	if (device->pending_events & EVDEV_RELATIVE_MOTION) {
		notify_motion(master, time, device->rel.dx, device->rel.dy);
		device->pending_events &= ~EVDEV_RELATIVE_MOTION;
		device->rel.dx = 0;
		device->rel.dy = 0;
	}
	if (device->pending_events & EVDEV_ABSOLUTE_MT_DOWN) {
		notify_touch(master, time,
			     device->mt.slot,
			     wl_fixed_from_int(device->mt.x[device->mt.slot]),
			     wl_fixed_from_int(device->mt.y[device->mt.slot]),
			     WL_TOUCH_DOWN);
		device->pending_events &= ~EVDEV_ABSOLUTE_MT_DOWN;
		device->pending_events &= ~EVDEV_ABSOLUTE_MT_MOTION;
	}
	if (device->pending_events & EVDEV_ABSOLUTE_MT_MOTION) {
		notify_touch(master, time,
			     device->mt.slot,
			     wl_fixed_from_int(device->mt.x[device->mt.slot]),
			     wl_fixed_from_int(device->mt.y[device->mt.slot]),
			     WL_TOUCH_MOTION);
		device->pending_events &= ~EVDEV_ABSOLUTE_MT_DOWN;
		device->pending_events &= ~EVDEV_ABSOLUTE_MT_MOTION;
	}
	if (device->pending_events & EVDEV_ABSOLUTE_MT_UP) {
		notify_touch(master, time, device->mt.slot, 0, 0,
			     WL_TOUCH_UP);
		device->pending_events &= ~EVDEV_ABSOLUTE_MT_UP;
	}
	if (device->pending_events & EVDEV_ABSOLUTE_MOTION) {
		transform_absolute(device);
		notify_motion(master, time,
			      wl_fixed_from_int(device->abs.x),
			      wl_fixed_from_int(device->abs.y));
		device->pending_events &= ~EVDEV_ABSOLUTE_MOTION;
	}
}

static void
fallback_process(struct evdev_dispatch *dispatch,
		 struct evdev_device *device,
		 struct input_event *event,
		 uint32_t time)
{
	switch (event->type) {
	case EV_REL:
		evdev_process_relative(device, event, time);
		break;
	case EV_ABS:
		evdev_process_absolute(device, event);
		break;
	case EV_KEY:
		evdev_process_key(device, event, time);
		break;
	case EV_SYN:
		device->pending_events |= EVDEV_SYN;
		break;
	}
}

static void
fallback_destroy(struct evdev_dispatch *dispatch)
{
	free(dispatch);
}

struct evdev_dispatch_interface fallback_interface = {
	fallback_process,
	fallback_destroy
};

static struct evdev_dispatch *
fallback_dispatch_create(void)
{
	struct evdev_dispatch *dispatch = malloc(sizeof *dispatch);
	if (dispatch == NULL)
		return NULL;

	dispatch->interface = &fallback_interface;

	return dispatch;
}

static void
evdev_process_events(struct evdev_device *device,
		     struct input_event *ev, int count)
{
	struct evdev_dispatch *dispatch = device->dispatch;
	struct input_event *e, *end;
	uint32_t time = 0;

	device->pending_events = 0;

	e = ev;
	end = e + count;
	for (e = ev; e < end; e++) {
		time = e->time.tv_sec * 1000 + e->time.tv_usec / 1000;

		/* we try to minimize the amount of notifications to be
		 * forwarded to the compositor, so we accumulate motion
		 * events and send as a bunch */
		if (!is_motion_event(e))
			evdev_flush_motion(device, time);

		dispatch->interface->process(dispatch, device, e, time);
	}

	evdev_flush_motion(device, time);
}

static int
evdev_device_data(int fd, uint32_t mask, void *data)
{
	struct weston_compositor *ec;
	struct evdev_device *device = data;
	struct input_event ev[32];
	int len;

	ec = device->seat->compositor;
	if (!ec->focus)
		return 1;

	/* If the compositor is repainting, this function is called only once
	 * per frame and we have to process all the events available on the
	 * fd, otherwise there will be input lag. */
	do {
		if (device->mtdev)
			len = mtdev_get(device->mtdev, fd, ev,
					ARRAY_LENGTH(ev)) *
				sizeof (struct input_event);
		else
			len = read(fd, &ev, sizeof ev);

		if (len < 0 || len % sizeof ev[0] != 0) {
			/* FIXME: call evdev_device_destroy when errno is ENODEV. */
			return 1;
		}

		evdev_process_events(device, ev, len / sizeof ev[0]);

	} while (len > 0);

	return 1;
}

static int
evdev_handle_device(struct evdev_device *device)
{
	struct input_absinfo absinfo;
	unsigned long ev_bits[NBITS(EV_MAX)];
	unsigned long abs_bits[NBITS(ABS_MAX)];
	unsigned long rel_bits[NBITS(REL_MAX)];
	unsigned long key_bits[NBITS(KEY_MAX)];
	int has_key, has_abs;
	unsigned int i;

	has_key = 0;
	has_abs = 0;
	device->caps = 0;

	ioctl(device->fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits);
	if (TEST_BIT(ev_bits, EV_ABS)) {
		has_abs = 1;

		ioctl(device->fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)),
		      abs_bits);
		if (TEST_BIT(abs_bits, ABS_X)) {
			ioctl(device->fd, EVIOCGABS(ABS_X), &absinfo);
			device->abs.min_x = absinfo.minimum;
			device->abs.max_x = absinfo.maximum;
			device->caps |= EVDEV_MOTION_ABS;
		}
		if (TEST_BIT(abs_bits, ABS_Y)) {
			ioctl(device->fd, EVIOCGABS(ABS_Y), &absinfo);
			device->abs.min_y = absinfo.minimum;
			device->abs.max_y = absinfo.maximum;
			device->caps |= EVDEV_MOTION_ABS;
		}
		if (TEST_BIT(abs_bits, ABS_MT_SLOT)) {
			ioctl(device->fd, EVIOCGABS(ABS_MT_POSITION_X),
			      &absinfo);
			device->abs.min_x = absinfo.minimum;
			device->abs.max_x = absinfo.maximum;
			ioctl(device->fd, EVIOCGABS(ABS_MT_POSITION_Y),
			      &absinfo);
			device->abs.min_y = absinfo.minimum;
			device->abs.max_y = absinfo.maximum;
			device->is_mt = 1;
			device->mt.slot = 0;
			device->caps |= EVDEV_TOUCH;
		}
	}
	if (TEST_BIT(ev_bits, EV_REL)) {
		ioctl(device->fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)),
		      rel_bits);
		if (TEST_BIT(rel_bits, REL_X) || TEST_BIT(rel_bits, REL_Y))
			device->caps |= EVDEV_MOTION_REL;
	}
	if (TEST_BIT(ev_bits, EV_KEY)) {
		has_key = 1;
		ioctl(device->fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)),
		      key_bits);
		if (TEST_BIT(key_bits, BTN_TOOL_FINGER) &&
		    !TEST_BIT(key_bits, BTN_TOOL_PEN) &&
		    has_abs)
			device->dispatch = evdev_touchpad_create(device);
		for (i = KEY_ESC; i < KEY_MAX; i++) {
			if (i >= BTN_MISC && i < KEY_OK)
				continue;
			if (TEST_BIT(key_bits, i)) {
				device->caps |= EVDEV_KEYBOARD;
				break;
			}
		}
		for (i = BTN_MISC; i < KEY_OK; i++) {
			if (TEST_BIT(key_bits, i)) {
				device->caps |= EVDEV_BUTTON;
				break;
			}
		}
	}
	if (TEST_BIT(ev_bits, EV_LED)) {
		device->caps |= EVDEV_KEYBOARD;
	}

	/* This rule tries to catch accelerometer devices and opt out. We may
	 * want to adjust the protocol later adding a proper event for dealing
	 * with accelerometers and implement here accordingly */
	if (has_abs && !has_key && !device->is_mt) {
		weston_log("input device %s, %s "
			   "ignored: unsupported device type\n",
			   device->devname, device->devnode);
		return 0;
	}

	return 1;
}

static int
evdev_configure_device(struct evdev_device *device)
{
	if ((device->caps &
	     (EVDEV_MOTION_ABS | EVDEV_MOTION_REL | EVDEV_BUTTON))) {
		weston_seat_init_pointer(device->seat);
		weston_log("input device %s, %s is a pointer caps =%s%s%s\n",
			   device->devname, device->devnode,
			   device->caps & EVDEV_MOTION_ABS ? " absolute-motion" : "",
			   device->caps & EVDEV_MOTION_REL ? " relative-motion": "",
			   device->caps & EVDEV_BUTTON ? " button" : "");
	}
	if ((device->caps & EVDEV_KEYBOARD)) {
		if (weston_seat_init_keyboard(device->seat, NULL) < 0)
			return -1;
		weston_log("input device %s, %s is a keyboard\n",
			   device->devname, device->devnode);
	}
	if ((device->caps & EVDEV_TOUCH)) {
		weston_seat_init_touch(device->seat);
		weston_log("input device %s, %s is a touch device\n",
			   device->devname, device->devnode);
	}

	return 0;
}

struct evdev_device *
evdev_device_create(struct weston_seat *seat, const char *path, int device_fd)
{
	struct evdev_device *device;
	struct weston_compositor *ec;
	char devname[256] = "unknown";

	device = malloc(sizeof *device);
	if (device == NULL)
		return NULL;
	memset(device, 0, sizeof *device);

	ec = seat->compositor;
	device->output =
		container_of(ec->output_list.next, struct weston_output, link);

	device->seat = seat;
	device->is_mt = 0;
	device->mtdev = NULL;
	device->devnode = strdup(path);
	device->mt.slot = -1;
	device->rel.dx = 0;
	device->rel.dy = 0;
	device->dispatch = NULL;
	device->fd = device_fd;

	ioctl(device->fd, EVIOCGNAME(sizeof(devname)), devname);
	device->devname = strdup(devname);

	if (!evdev_handle_device(device)) {
		free(device->devnode);
		free(device->devname);
		free(device);
		return EVDEV_UNHANDLED_DEVICE;
	}

	if (evdev_configure_device(device) == -1)
		goto err1;

	/* If the dispatch was not set up use the fallback. */
	if (device->dispatch == NULL)
		device->dispatch = fallback_dispatch_create();
	if (device->dispatch == NULL)
		goto err1;


	if (device->is_mt) {
		device->mtdev = mtdev_new_open(device->fd);
		if (!device->mtdev)
			weston_log("mtdev failed to open for %s\n", path);
	}

	device->source = wl_event_loop_add_fd(ec->input_loop, device->fd,
					      WL_EVENT_READABLE,
					      evdev_device_data, device);
	if (device->source == NULL)
		goto err2;

	return device;

err2:
	device->dispatch->interface->destroy(device->dispatch);
err1:
	free(device->devname);
	free(device->devnode);
	free(device);
	return NULL;
}

void
evdev_device_destroy(struct evdev_device *device)
{
	struct evdev_dispatch *dispatch;

	dispatch = device->dispatch;
	if (dispatch)
		dispatch->interface->destroy(dispatch);

	wl_event_source_remove(device->source);
	wl_list_remove(&device->link);
	if (device->mtdev)
		mtdev_close_delete(device->mtdev);
	close(device->fd);
	free(device->devname);
	free(device->devnode);
	free(device);
}

void
evdev_notify_keyboard_focus(struct weston_seat *seat,
			    struct wl_list *evdev_devices)
{
	struct evdev_device *device;
	struct wl_array keys;
	unsigned int i, set;
	char evdev_keys[(KEY_CNT + 7) / 8];
	char all_keys[(KEY_CNT + 7) / 8];
	uint32_t *k;
	int ret;

	if (!seat->seat.keyboard)
		return;

	memset(all_keys, 0, sizeof all_keys);
	wl_list_for_each(device, evdev_devices, link) {
		memset(evdev_keys, 0, sizeof evdev_keys);
		ret = ioctl(device->fd,
			    EVIOCGKEY(sizeof evdev_keys), evdev_keys);
		if (ret < 0) {
			weston_log("failed to get keys for device %s\n",
				device->devnode);
			continue;
		}
		for (i = 0; i < ARRAY_LENGTH(evdev_keys); i++)
			all_keys[i] |= evdev_keys[i];
	}

	wl_array_init(&keys);
	for (i = 0; i < KEY_CNT; i++) {
		set = all_keys[i >> 3] & (1 << (i & 7));
		if (set) {
			k = wl_array_add(&keys, sizeof *k);
			*k = i;
		}
	}

	notify_keyboard_focus_in(seat, &keys, STATE_UPDATE_AUTOMATIC);

	wl_array_release(&keys);
}
