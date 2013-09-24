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

#include "config.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <linux/input.h>

#include "filter.h"
#include "evdev.h"
#include "../shared/config-parser.h"

/* Default values */
#define DEFAULT_CONSTANT_ACCEL_NUMERATOR 50
#define DEFAULT_MIN_ACCEL_FACTOR 0.16
#define DEFAULT_MAX_ACCEL_FACTOR 1.0
#define DEFAULT_HYSTERESIS_MARGIN_DENOMINATOR 700.0

#define DEFAULT_TOUCHPAD_SINGLE_TAP_BUTTON BTN_LEFT
#define DEFAULT_TOUCHPAD_SINGLE_TAP_TIMEOUT 100

enum touchpad_model {
	TOUCHPAD_MODEL_UNKNOWN = 0,
	TOUCHPAD_MODEL_SYNAPTICS,
	TOUCHPAD_MODEL_ALPS,
	TOUCHPAD_MODEL_APPLETOUCH,
	TOUCHPAD_MODEL_ELANTECH
};

enum touchpad_event {
	TOUCHPAD_EVENT_NONE	    = 0,
	TOUCHPAD_EVENT_ABSOLUTE_ANY = (1 << 0),
	TOUCHPAD_EVENT_ABSOLUTE_X   = (1 << 1),
	TOUCHPAD_EVENT_ABSOLUTE_Y   = (1 << 2),
	TOUCHPAD_EVENT_REPORT	    = (1 << 3)
};

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
	TOUCHPAD_STATE_NONE  = 0,
	TOUCHPAD_STATE_TOUCH = (1 << 0),
	TOUCHPAD_STATE_MOVE  = (1 << 1)
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

enum fsm_event {
	FSM_EVENT_TOUCH,
	FSM_EVENT_RELEASE,
	FSM_EVENT_MOTION,
	FSM_EVENT_TIMEOUT
};

enum fsm_state {
	FSM_IDLE,
	FSM_TOUCH,
	FSM_TAP,
	FSM_TAP_2,
	FSM_DRAG
};

struct touchpad_dispatch {
	struct evdev_dispatch base;
	struct evdev_device *device;

	enum touchpad_model model;
	unsigned int state;
	int finger_state;
	int last_finger_state;

	double constant_accel_factor;
	double min_accel_factor;
	double max_accel_factor;

	unsigned int event_mask;
	unsigned int event_mask_filter;

	int reset;

	struct {
		bool enable;

		struct wl_array events;
		enum fsm_state state;
		struct wl_event_source *timer_source;
	} fsm;

	struct {
		int32_t x;
		int32_t y;
	} hw_abs;

	int has_pressure;
	struct {
		int32_t touch_low;
		int32_t touch_high;
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

	struct weston_motion_filter *filter;
};

static enum touchpad_model
get_touchpad_model(struct evdev_device *device)
{
	struct input_id id;
	unsigned int i;

	if (ioctl(device->fd, EVIOCGID, &id) < 0)
		return TOUCHPAD_MODEL_UNKNOWN;

	for (i = 0; i < ARRAY_LENGTH(touchpad_spec_table); i++)
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
	struct weston_motion_params motion;

	motion.dx = *dx;
	motion.dy = *dy;

	weston_filter_dispatch(touchpad->filter, &motion, touchpad, time);

	*dx = motion.dx;
	*dy = motion.dy;
}

static void
notify_button_pressed(struct touchpad_dispatch *touchpad, uint32_t time)
{
	notify_button(touchpad->device->seat, time,
		      DEFAULT_TOUCHPAD_SINGLE_TAP_BUTTON,
		      WL_POINTER_BUTTON_STATE_PRESSED);
}

static void
notify_button_released(struct touchpad_dispatch *touchpad, uint32_t time)
{
	notify_button(touchpad->device->seat, time,
		      DEFAULT_TOUCHPAD_SINGLE_TAP_BUTTON,
		      WL_POINTER_BUTTON_STATE_RELEASED);
}

static void
notify_tap(struct touchpad_dispatch *touchpad, uint32_t time)
{
	notify_button_pressed(touchpad, time);
	notify_button_released(touchpad, time);
}

static void
process_fsm_events(struct touchpad_dispatch *touchpad, uint32_t time)
{
	uint32_t timeout = UINT32_MAX;
	enum fsm_event *pevent;
	enum fsm_event event;

	if (!touchpad->fsm.enable)
		return;

	if (touchpad->fsm.events.size == 0)
		return;

	wl_array_for_each(pevent, &touchpad->fsm.events) {
		event = *pevent;
		timeout = 0;

		switch (touchpad->fsm.state) {
		case FSM_IDLE:
			switch (event) {
			case FSM_EVENT_TOUCH:
				touchpad->fsm.state = FSM_TOUCH;
				break;
			default:
				break;
			}
			break;
		case FSM_TOUCH:
			switch (event) {
			case FSM_EVENT_RELEASE:
				timeout = DEFAULT_TOUCHPAD_SINGLE_TAP_TIMEOUT;
				touchpad->fsm.state = FSM_TAP;
				break;
			default:
				touchpad->fsm.state = FSM_IDLE;
				break;
			}
			break;
		case FSM_TAP:
			switch (event) {
			case FSM_EVENT_TIMEOUT:
				notify_tap(touchpad, time);
				touchpad->fsm.state = FSM_IDLE;
				break;
			case FSM_EVENT_TOUCH:
				notify_button_pressed(touchpad, time);
				touchpad->fsm.state = FSM_TAP_2;
				break;
			default:
				touchpad->fsm.state = FSM_IDLE;
				break;
			}
			break;
		case FSM_TAP_2:
			switch (event) {
			case FSM_EVENT_MOTION:
				touchpad->fsm.state = FSM_DRAG;
				break;
			case FSM_EVENT_RELEASE:
				notify_button_released(touchpad, time);
				notify_tap(touchpad, time);
				touchpad->fsm.state = FSM_IDLE;
				break;
			default:
				touchpad->fsm.state = FSM_IDLE;
				break;
			}
			break;
		case FSM_DRAG:
			switch (event) {
			case FSM_EVENT_RELEASE:
				notify_button_released(touchpad, time);
				touchpad->fsm.state = FSM_IDLE;
				break;
			default:
				touchpad->fsm.state = FSM_IDLE;
				break;
			}
			break;
		default:
			weston_log("evdev-touchpad: Unknown state %d",
				   touchpad->fsm.state);
			touchpad->fsm.state = FSM_IDLE;
			break;
		}
	}

	if (timeout != UINT32_MAX)
		wl_event_source_timer_update(touchpad->fsm.timer_source,
					     timeout);

	wl_array_release(&touchpad->fsm.events);
	wl_array_init(&touchpad->fsm.events);
}

static void
push_fsm_event(struct touchpad_dispatch *touchpad,
	       enum fsm_event event)
{
	enum fsm_event *pevent;

	if (!touchpad->fsm.enable)
		return;

	pevent = wl_array_add(&touchpad->fsm.events, sizeof event);
	if (pevent)
		*pevent = event;
	else
		touchpad->fsm.state = FSM_IDLE;
}

static int
fsm_timout_handler(void *data)
{
	struct touchpad_dispatch *touchpad = data;

	if (touchpad->fsm.events.size == 0) {
		push_fsm_event(touchpad, FSM_EVENT_TIMEOUT);
		process_fsm_events(touchpad, weston_compositor_get_time());
	}

	return 1;
}

static void
touchpad_update_state(struct touchpad_dispatch *touchpad, uint32_t time)
{
	int motion_index;
	int center_x, center_y;
	double dx = 0.0, dy = 0.0;

	if (touchpad->reset ||
	    touchpad->last_finger_state != touchpad->finger_state) {
		touchpad->reset = 0;
		touchpad->motion_count = 0;
		touchpad->event_mask = TOUCHPAD_EVENT_NONE;
		touchpad->event_mask_filter =
			TOUCHPAD_EVENT_ABSOLUTE_X | TOUCHPAD_EVENT_ABSOLUTE_Y;

		touchpad->last_finger_state = touchpad->finger_state;

		process_fsm_events(touchpad, time);

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
	} else {
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

		if (touchpad->finger_state == TOUCHPAD_FINGERS_ONE) {
			notify_motion(touchpad->device->seat, time,
				      wl_fixed_from_double(dx),
				      wl_fixed_from_double(dy));
		} else if (touchpad->finger_state == TOUCHPAD_FINGERS_TWO) {
			if (dx != 0.0)
				notify_axis(touchpad->device->seat,
					    time,
					    WL_POINTER_AXIS_HORIZONTAL_SCROLL,
					    wl_fixed_from_double(dx));
			if (dy != 0.0)
				notify_axis(touchpad->device->seat,
					    time,
					    WL_POINTER_AXIS_VERTICAL_SCROLL,
					    wl_fixed_from_double(dy));
		}
	}

	if (!(touchpad->state & TOUCHPAD_STATE_MOVE) &&
	    ((int)dx || (int)dy)) {
		touchpad->state |= TOUCHPAD_STATE_MOVE;
		push_fsm_event(touchpad, FSM_EVENT_MOTION);
	}

	process_fsm_events(touchpad, time);
}

static void
on_touch(struct touchpad_dispatch *touchpad)
{
	touchpad->state |= TOUCHPAD_STATE_TOUCH;

	push_fsm_event(touchpad, FSM_EVENT_TOUCH);
}

static void
on_release(struct touchpad_dispatch *touchpad)
{

	touchpad->reset = 1;
	touchpad->state &= ~(TOUCHPAD_STATE_MOVE | TOUCHPAD_STATE_TOUCH);

	push_fsm_event(touchpad, FSM_EVENT_RELEASE);
}

static inline void
process_absolute(struct touchpad_dispatch *touchpad,
		 struct evdev_device *device,
		 struct input_event *e)
{
	switch (e->code) {
	case ABS_PRESSURE:
		if (e->value > touchpad->pressure.touch_high &&
		    !(touchpad->state & TOUCHPAD_STATE_TOUCH))
			on_touch(touchpad);
		else if (e->value < touchpad->pressure.touch_low &&
			 touchpad->state & TOUCHPAD_STATE_TOUCH)
			on_release(touchpad);

		break;
	case ABS_X:
		if (touchpad->state & TOUCHPAD_STATE_TOUCH) {
			touchpad->hw_abs.x = e->value;
			touchpad->event_mask |= TOUCHPAD_EVENT_ABSOLUTE_ANY;
			touchpad->event_mask |= TOUCHPAD_EVENT_ABSOLUTE_X;
		}
		break;
	case ABS_Y:
		if (touchpad->state & TOUCHPAD_STATE_TOUCH) {
			touchpad->hw_abs.y = e->value;
			touchpad->event_mask |= TOUCHPAD_EVENT_ABSOLUTE_ANY;
			touchpad->event_mask |= TOUCHPAD_EVENT_ABSOLUTE_Y;
		}
		break;
	}
}

static inline void
process_key(struct touchpad_dispatch *touchpad,
	    struct evdev_device *device,
	    struct input_event *e,
	    uint32_t time)
{
	uint32_t code;

	switch (e->code) {
	case BTN_TOUCH:
		if (!touchpad->has_pressure) {
			if (e->value && !(touchpad->state & TOUCHPAD_STATE_TOUCH))
				on_touch(touchpad);
			else if (!e->value)
				on_release(touchpad);
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
		if (!touchpad->fsm.enable && e->code == BTN_LEFT &&
		    touchpad->finger_state == TOUCHPAD_FINGERS_TWO)
			code = BTN_RIGHT;
		else
			code = e->code;
		notify_button(device->seat, time, code,
			      e->value ? WL_POINTER_BUTTON_STATE_PRESSED :
			                 WL_POINTER_BUTTON_STATE_RELEASED);
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
		if (e->value)
			touchpad->finger_state |= TOUCHPAD_FINGERS_ONE;
		else
			touchpad->finger_state &= ~TOUCHPAD_FINGERS_ONE;
		break;
	case BTN_TOOL_DOUBLETAP:
		if (e->value)
			touchpad->finger_state |= TOUCHPAD_FINGERS_TWO;
		else
			touchpad->finger_state &= ~TOUCHPAD_FINGERS_TWO;
		break;
	case BTN_TOOL_TRIPLETAP:
		if (e->value)
			touchpad->finger_state |= TOUCHPAD_FINGERS_THREE;
		else
			touchpad->finger_state &= ~TOUCHPAD_FINGERS_THREE;
		break;
	}
}

static void
touchpad_process(struct evdev_dispatch *dispatch,
		 struct evdev_device *device,
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

	touchpad->filter->interface->destroy(touchpad->filter);
	wl_event_source_remove(touchpad->fsm.timer_source);
	free(dispatch);
}

struct evdev_dispatch_interface touchpad_interface = {
	touchpad_process,
	touchpad_destroy
};

static void
touchpad_parse_config(struct touchpad_dispatch *touchpad, double diagonal)
{
	struct weston_compositor *compositor =
		touchpad->device->seat->compositor;
	struct weston_config_section *s;
	double constant_accel_factor;
	double min_accel_factor;
	double max_accel_factor;

	s = weston_config_get_section(compositor->config,
				      "touchpad", NULL, NULL);
	weston_config_section_get_double(s, "constant_accel_factor",
					 &constant_accel_factor,
					 DEFAULT_CONSTANT_ACCEL_NUMERATOR);
	weston_config_section_get_double(s, "min_accel_factor",
					 &min_accel_factor,
					 DEFAULT_MIN_ACCEL_FACTOR);
	weston_config_section_get_double(s, "max_accel_factor",
					 &max_accel_factor,
					 DEFAULT_MAX_ACCEL_FACTOR);

	touchpad->constant_accel_factor =
		constant_accel_factor / diagonal;
	touchpad->min_accel_factor = min_accel_factor;
	touchpad->max_accel_factor = max_accel_factor;
}

static int
touchpad_init(struct touchpad_dispatch *touchpad,
	      struct evdev_device *device)
{
	struct weston_motion_filter *accel;
	struct wl_event_loop *loop;

	unsigned long prop_bits[INPUT_PROP_MAX];
	struct input_absinfo absinfo;
	unsigned long abs_bits[NBITS(ABS_MAX)];

	bool has_buttonpad;

	double width;
	double height;
	double diagonal;

	touchpad->base.interface = &touchpad_interface;
	touchpad->device = device;

	/* Detect model */
	touchpad->model = get_touchpad_model(device);

	ioctl(device->fd, EVIOCGPROP(sizeof(prop_bits)), prop_bits);
	has_buttonpad = TEST_BIT(prop_bits, INPUT_PROP_BUTTONPAD);

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

	touchpad_parse_config(touchpad, diagonal);

	touchpad->hysteresis.margin_x =
		diagonal / DEFAULT_HYSTERESIS_MARGIN_DENOMINATOR;
	touchpad->hysteresis.margin_y =
		diagonal / DEFAULT_HYSTERESIS_MARGIN_DENOMINATOR;
	touchpad->hysteresis.center_x = 0;
	touchpad->hysteresis.center_y = 0;

	/* Configure acceleration profile */
	accel = create_pointer_accelator_filter(touchpad_profile);
	if (accel == NULL)
		return -1;
	touchpad->filter = accel;

	/* Setup initial state */
	touchpad->reset = 1;

	memset(touchpad->motion_history, 0, sizeof touchpad->motion_history);
	touchpad->motion_index = 0;
	touchpad->motion_count = 0;

	touchpad->state = TOUCHPAD_STATE_NONE;
	touchpad->last_finger_state = 0;
	touchpad->finger_state = 0;

	wl_array_init(&touchpad->fsm.events);
	touchpad->fsm.state = FSM_IDLE;

	loop = wl_display_get_event_loop(device->seat->compositor->wl_display);
	touchpad->fsm.timer_source =
		wl_event_loop_add_timer(loop, fsm_timout_handler, touchpad);
	if (touchpad->fsm.timer_source == NULL) {
		accel->interface->destroy(accel);
		return -1;
	}

	/* Configure */
	touchpad->fsm.enable = !has_buttonpad;

	return 0;
}

struct evdev_dispatch *
evdev_touchpad_create(struct evdev_device *device)
{
	struct touchpad_dispatch *touchpad;

	touchpad = malloc(sizeof *touchpad);
	if (touchpad == NULL)
		return NULL;

	if (touchpad_init(touchpad, device) != 0) {
		free(touchpad);
		return NULL;
	}

	return &touchpad->base;
}
