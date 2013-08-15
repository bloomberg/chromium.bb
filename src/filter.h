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

#ifndef _FILTER_H_
#define _FILTER_H_

#include "config.h"

#include <wayland-util.h>

#include "compositor.h"

struct weston_motion_params {
	double dx, dy;
};

struct weston_motion_filter;

WL_EXPORT void
weston_filter_dispatch(struct weston_motion_filter *filter,
		       struct weston_motion_params *motion,
		       void *data, uint32_t time);


struct weston_motion_filter_interface {
	void (*filter)(struct weston_motion_filter *filter,
		       struct weston_motion_params *motion,
		       void *data, uint32_t time);
	void (*destroy)(struct weston_motion_filter *filter);
};

struct weston_motion_filter {
	struct weston_motion_filter_interface *interface;
	struct wl_list link;
};

WL_EXPORT struct weston_motion_filter *
create_linear_acceleration_filter(double speed);

typedef double (*accel_profile_func_t)(struct weston_motion_filter *filter,
				       void *data,
				       double velocity,
				       uint32_t time);

WL_EXPORT struct weston_motion_filter *
create_pointer_accelator_filter(accel_profile_func_t filter);

#endif // _FILTER_H_
