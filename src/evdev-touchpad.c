/*
 * Copyright © 2012 Jonas Ådahl
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <linux/input.h>

#include "filter.h"
#include "evdev-private.h"

/* Default values */
#define DEFAULT_CONSTANT_ACCEL_NUMERATOR 50
#define DEFAULT_MIN_ACCEL_FACTOR 0.16
#define DEFAULT_MAX_ACCEL_FACTOR 1.0
#define DEFAULT_HYSTERESIS_MARGIN_DENOMINATOR 700.0

enum touchpad_model {
	TOUCHPAD_MODEL_UNKNOWN = 0,
	TOUCHPAD_MODEL_SYNAPTICS,
	TOUCHPAD_MODEL_ALPS,
	TOUCHPAD_MODEL_APPLETOUCH,
	TOUCHPAD_MODEL_ELANTECH
};

#define TOUCHPAD_EVENT_NONE		0
#define TOUCHPAD_EVENT_ABSOLUTE_ANY	(1 << 0)
#define TOUCHPAD_EVENT_ABSOLUTE_X	(1 << 1)
#define TOUCHPAD_EVENT_ABSOLUTE_Y	(1 << 2)
#define TOUCHPAD_EVENT_REPORT		(1 << 3)

struct touchpad_model_spec {
	short vendor;
	short product;
	enum touchpad_model model;
};

static struct touchpad_model_spec touchpad_spec_table[] = {
	{0x0002, 0x0007, TOUCHPAD_MODEL_SYNAPTICS},
	{0x0002, 0x0008, TOUCHPAD_MODEL_ALPS},
	{0x05ac, 0x0000, TOUCHPAD_MODEL_APPLETOUCH},
	{0x0002, 0x000e, TOUCHPAD_MODEL_ELANTECH},
	{0x0000, 0x0000, TOUCHPAD_MODEL_UNKNOWN}
};

enum touchpad_state {
	TOUCHPAD_STATE_NONE = 0,
	TOUCHPAD_STATE_TOUCH,
	TOUCHPAD_STATE_PRESS
};

#define TOUCHPAD_HISTORY_LENGTH 4

struct touchpad_motion {
	int32_t x;
	int32_t y;
};

enum touchpad_fingers_state {
	TOUCHPAD_FINGERS_ONE   = (1 << 0),
	TOUCHPAD_FINGERS_TWO   = (1 << 1),
	TOUCHPAD_FINGERS_THREE = (1 << 2)
};

struct touchpad_dispatch {
	struct evdev_dispatch base;
	struct evdev_input_device *device;

	enum touchpad_model model;
	enum touchpad_state state;
	int finger_state;
	int last_finger_state;

	double constant_accel_factor;
	double min_accel_factor;
	double max_accel_factor;

	unsigned int event_mask;
	unsigned int event_mask_filter;

	int reset;

	struct {
		int32_t x;
		int32_t y;
	} hw_abs;

	int has_pressure;
	struct {
		int32_t touch_low;
		int32_t touch_high;
		int32_t press;
	} pressure;

	struct {
		int32_t margin_x;
		int32_t margin_y;
		int32_t center_x;
		int32_t center_y;
	} hysteresis;

	struct touchpad_motion motion_history[TOUCHPAD_HISTORY_LENGTH];
	int motion_index;
	unsigned int motion_count;

	struct wl_list motion_filters;
};

static enum touchpad_model
get_touchpad_model(struct evdev_input_device *device)
{
	struct input_id id;
	unsigned int i;

	if (ioctl(device->fd, EVIOCGID, &id) < 0)
		return TOUCHPAD_MODEL_UNKNOWN;

	for (i = 0; i < sizeof touchpad_spec_table; i++)
		if (touchpad_spec_table[i].vendor == id.vendor &&
		    (!touchpad_spec_table[i].product ||
		     touchpad_spec_table[i].product == id.product))
			return touchpad_spec_table[i].model;

	return TOUCHPAD_MODEL_UNKNOWN;
}

static void
configure_touchpad_pressure(struct touchpad_dispatch *touchpad,
			    int32_t pressure_min, int32_t pressure_max)
{
	int32_t range = pressure_max - pressure_min + 1;

	touchpad->has_pressure = 1;

	/* Magic numbers from xf86-input-synaptics */
	switch (touchpad->model) {
	case TOUCHPAD_MODEL_ELANTECH:
		touchpad->pressure.touch_low = pressure_min + 1;
		touchpad->pressure.touch_high = pressure_min + 1;
		break;
	default:
		touchpad->pressure.touch_low =
			pressure_min + range * (25.0/256.0);
		touchpad->pressure.touch_high =
			pressure_min + range * (30.0/256.0);
	}

	touchpad->pressure.press = pressure_min + range;
}

static double
touchpad_profile(struct weston_motion_filter *filter,
		 void *data,
		 double velocity,
		 uint32_t time)
{
	struct touchpad_dispatch *touchpad =
		(struct touchpad_dispatch *) data;

	double accel_factor;

	accel_factor = velocity * touchpad->constant_accel_factor;

	if (accel_factor > touchpad->max_accel_factor)
		accel_factor = touchpad->max_accel_factor;
	else if (accel_factor < touchpad->min_accel_factor)
		accel_factor = touchpad->min_accel_factor;

	return accel_factor;
}

static void
configure_touchpad(struct touchpad_dispatch *touchpad,
		   struct evdev_input_device *device)
{
	struct weston_motion_filter *accel;

	struct input_absinfo absinfo;
	unsigned long abs_bits[NBITS(ABS_MAX)];

	double width;
	double height;
	double diagonal;

	/* Detect model */
	touchpad->model = get_touchpad_model(device);

	/* Configure pressure */
	ioctl(device->fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);
	if (TEST_BIT(abs_bits, ABS_PRESSURE)) {
		ioctl(device->fd, EVIOCGABS(ABS_PRESSURE), &absinfo);
		configure_touchpad_pressure(touchpad,
					    absinfo.minimum,
					    absinfo.maximum);
	}

	/* Configure acceleration factor */
	width = abs(device->abs.max_x - device->abs.min_x);
	height = abs(device->abs.max_y - device->abs.min_y);
	diagonal = sqrt(width*width + height*height);

	touchpad->constant_accel_factor =
		DEFAULT_CONSTANT_ACCEL_NUMERATOR / diagonal;

	touchpad->min_accel_factor = DEFAULT_MIN_ACCEL_FACTOR;
	touchpad->max_accel_factor = DEFAULT_MAX_ACCEL_FACTOR;

	touchpad->hysteresis.margin_x =
	       	diagonal / DEFAULT_HYSTERESIS_MARGIN_DENOMINATOR;
	touchpad->hysteresis.margin_y =
	       	diagonal / DEFAULT_HYSTERESIS_MARGIN_DENOMINATOR;
	touchpad->hysteresis.center_x = 0;
	touchpad->hysteresis.center_y = 0;

	/* Configure acceleration profile */
	accel = create_pointer_accelator_filter(touchpad_profile);
	wl_list_insert(&touchpad->motion_filters, &accel->link);

	/* Setup initial state */
	touchpad->reset = 1;

	memset(touchpad->motion_history, 0, sizeof touchpad->motion_history);
	touchpad->motion_index = 0;
	touchpad->motion_count = 0;

	touchpad->state = TOUCHPAD_STATE_NONE;
	touchpad->last_finger_state = 0;
	touchpad->finger_state = 0;
}

static inline struct touchpad_motion *
motion_history_offset(struct touchpad_dispatch *touchpad, int offset)
{
	int offset_index =
		(touchpad->motion_index - offset + TOUCHPAD_HISTORY_LENGTH) %
		TOUCHPAD_HISTORY_LENGTH;

	return &touchpad->motion_history[offset_index];
}

static double
estimate_delta(int x0, int x1, int x2, int x3)
{
	return (x0 + x1 - x2 - x3) / 4;
}

static int
hysteresis(int in, int center, int margin)
{
	int diff = in - center;
	if (abs(diff) <= margin)
		return center;

	if (diff > margin)
		return center + diff - margin;
	else if (diff < -margin)
		return center + diff + margin;
	return center + diff;
}

static void
touchpad_get_delta(struct touchpad_dispatch *touchpad, double *dx, double *dy)
{
	*dx = estimate_delta(motion_history_offset(touchpad, 0)->x,
			     motion_history_offset(touchpad, 1)->x,
			     motion_history_offset(touchpad, 2)->x,
			     motion_history_offset(touchpad, 3)->x);
	*dy = estimate_delta(motion_history_offset(touchpad, 0)->y,
			     motion_history_offset(touchpad, 1)->y,
			     motion_history_offset(touchpad, 2)->y,
			     motion_history_offset(touchpad, 3)->y);
}

static void
filter_motion(struct touchpad_dispatch *touchpad,
	      double *dx, double *dy, uint32_t time)
{
	struct weston_motion_filter *filter;
	struct weston_motion_params motion;

	motion.dx = *dx;
	motion.dy = *dy;

	wl_list_for_each(filter, &touchpad->motion_filters, link)
		weston_filter_dispatch(filter, &motion, touchpad, time);

	*dx = motion.dx;
	*dy = motion.dy;
}

static void
touchpad_update_state(struct touchpad_dispatch *touchpad, uint32_t time)
{
	int motion_index;
	int center_x, center_y;
	double dx, dy;

	if (touchpad->reset ||
	    touchpad->last_finger_state != touchpad->finger_state) {
		touchpad->reset = 0;
		touchpad->motion_count = 0;
		touchpad->event_mask = TOUCHPAD_EVENT_NONE;
		touchpad->event_mask_filter =
			TOUCHPAD_EVENT_ABSOLUTE_X | TOUCHPAD_EVENT_ABSOLUTE_Y;

		touchpad->last_finger_state = touchpad->finger_state;

		return;
	}
	touchpad->last_finger_state = touchpad->finger_state;

	if (!(touchpad->event_mask & TOUCHPAD_EVENT_REPORT))
		return;
	else
		touchpad->event_mask &= ~TOUCHPAD_EVENT_REPORT;

	if ((touchpad->event_mask & touchpad->event_mask_filter) !=
	    touchpad->event_mask_filter)
		return;

	touchpad->event_mask_filter = TOUCHPAD_EVENT_ABSOLUTE_ANY;
	touchpad->event_mask = 0;

	/* Avoid noice by moving center only when delta reaches a threshold
	 * distance from the old center. */
	if (touchpad->motion_count > 0) {
		center_x = hysteresis(touchpad->hw_abs.x,
				      touchpad->hysteresis.center_x,
				      touchpad->hysteresis.margin_x);
		center_y = hysteresis(touchpad->hw_abs.y,
				      touchpad->hysteresis.center_y,
				      touchpad->hysteresis.margin_y);
	}
	else {
		center_x = touchpad->hw_abs.x;
		center_y = touchpad->hw_abs.y;
	}
	touchpad->hysteresis.center_x = center_x;
	touchpad->hysteresis.center_y = center_y;
	touchpad->hw_abs.x = center_x;
	touchpad->hw_abs.y = center_y;

	/* Update motion history tracker */
	motion_index = (touchpad->motion_index + 1) % TOUCHPAD_HISTORY_LENGTH;
	touchpad->motion_index = motion_index;
	touchpad->motion_history[motion_index].x = touchpad->hw_abs.x;
	touchpad->motion_history[motion_index].y = touchpad->hw_abs.y;
	if (touchpad->motion_count < 4)
		touchpad->motion_count++;

	if (touchpad->motion_count >= 4) {
		touchpad_get_delta(touchpad, &dx, &dy);

		filter_motion(touchpad, &dx, &dy, time);

		touchpad->device->rel.dx = wl_fixed_from_double(dx);
		touchpad->device->rel.dy = wl_fixed_from_double(dy);
		touchpad->device->pending_events |= EVDEV_RELATIVE_MOTION;
	}
}

static inline void
process_absolute(struct touchpad_dispatch *touchpad,
		 struct evdev_input_device *device,
		 struct input_event *e)
{
	switch (e->code) {
	case ABS_PRESSURE:
		if (e->value > touchpad->pressure.press)
			touchpad->state = TOUCHPAD_STATE_PRESS;
		else if (e->value > touchpad->pressure.touch_high)
			touchpad->state = TOUCHPAD_STATE_TOUCH;
		else if (e->value < touchpad->pressure.touch_low) {
			if (touchpad->state > TOUCHPAD_STATE_NONE)
				touchpad->reset = 1;

			touchpad->state = TOUCHPAD_STATE_NONE;
		}

		break;
	case ABS_X:
		if (touchpad->state >= TOUCHPAD_STATE_TOUCH) {
			touchpad->hw_abs.x = e->value;
			touchpad->event_mask |= TOUCHPAD_EVENT_ABSOLUTE_ANY;
			touchpad->event_mask |= TOUCHPAD_EVENT_ABSOLUTE_X;
		}
		break;
	case ABS_Y:
		if (touchpad->state >= TOUCHPAD_STATE_TOUCH) {
			touchpad->hw_abs.y = e->value;
			touchpad->event_mask |= TOUCHPAD_EVENT_ABSOLUTE_ANY;
			touchpad->event_mask |= TOUCHPAD_EVENT_ABSOLUTE_Y;
		}
		break;
	}
}

static inline void
process_key(struct touchpad_dispatch *touchpad,
	    struct evdev_input_device *device,
	    struct input_event *e,
	    uint32_t time)
{
	switch (e->code) {
	case BTN_TOUCH:
		if (!touchpad->has_pressure) {
			if (!e->value) {
				touchpad->state = TOUCHPAD_STATE_NONE;
				touchpad->reset = 1;
			} else {
				touchpad->state =
					e->value ?
					TOUCHPAD_STATE_TOUCH :
					TOUCHPAD_STATE_NONE;
			}
		}
		break;
	case BTN_LEFT:
	case BTN_RIGHT:
	case BTN_MIDDLE:
	case BTN_SIDE:
	case BTN_EXTRA:
	case BTN_FORWARD:
	case BTN_BACK:
	case BTN_TASK:
		notify_button(&device->master->base.seat,
			      time, e->code, e->value);
		break;
	case BTN_TOOL_PEN:
	case BTN_TOOL_RUBBER:
	case BTN_TOOL_BRUSH:
	case BTN_TOOL_PENCIL:
	case BTN_TOOL_AIRBRUSH:
	case BTN_TOOL_MOUSE:
	case BTN_TOOL_LENS:
		touchpad->reset = 1;
		break;
	case BTN_TOOL_FINGER:
		touchpad->finger_state =
			~TOUCHPAD_FINGERS_ONE | e->value ?
			TOUCHPAD_FINGERS_ONE : 0;
		break;
	case BTN_TOOL_DOUBLETAP:
		touchpad->finger_state =
			~TOUCHPAD_FINGERS_TWO | e->value ?
			TOUCHPAD_FINGERS_TWO : 0;
		break;
	case BTN_TOOL_TRIPLETAP:
		touchpad->finger_state =
			~TOUCHPAD_FINGERS_THREE | e->value ?
			TOUCHPAD_FINGERS_THREE : 0;
		break;
	}
}

static void
touchpad_process(struct evdev_dispatch *dispatch,
		 struct evdev_input_device *device,
		 struct input_event *e,
		 uint32_t time)
{
	struct touchpad_dispatch *touchpad =
		(struct touchpad_dispatch *) dispatch;

	switch (e->type) {
	case EV_SYN:
		if (e->code == SYN_REPORT)
			touchpad->event_mask |= TOUCHPAD_EVENT_REPORT;
		break;
	case EV_ABS:
		process_absolute(touchpad, device, e);
		break;
	case EV_KEY:
		process_key(touchpad, device, e, time);
		break;
	}

	touchpad_update_state(touchpad, time);
}

static void
touchpad_destroy(struct evdev_dispatch *dispatch)
{
	struct touchpad_dispatch *touchpad =
		(struct touchpad_dispatch *) dispatch;
	struct weston_motion_filter *filter;
	struct weston_motion_filter *next;

	wl_list_for_each_safe(filter, next, &touchpad->motion_filters, link)
		filter->interface->destroy(filter);

	free(dispatch);
}

struct evdev_dispatch_interface touchpad_interface = {
	touchpad_process,
	touchpad_destroy
};

struct evdev_dispatch *
evdev_touchpad_create(struct evdev_input_device *device)
{
	struct touchpad_dispatch *touchpad;

	touchpad = malloc(sizeof *touchpad);
	if (touchpad == NULL)
		return NULL;

	touchpad->base.interface = &touchpad_interface;

	touchpad->device = device;
	wl_list_init(&touchpad->motion_filters);

	configure_touchpad(touchpad, device);

	return &touchpad->base;
}
