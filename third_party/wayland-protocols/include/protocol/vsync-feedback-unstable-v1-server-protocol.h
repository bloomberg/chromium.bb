/*
 * Copyright 2016 The Chromium Authors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef VSYNC_FEEDBACK_UNSTABLE_V1_SERVER_PROTOCOL_H
#define VSYNC_FEEDBACK_UNSTABLE_V1_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct wl_output;
struct zwp_vsync_feedback_v1;
struct zwp_vsync_timing_v1;

extern const struct wl_interface zwp_vsync_feedback_v1_interface;
extern const struct wl_interface zwp_vsync_timing_v1_interface;

/**
 * zwp_vsync_feedback_v1 - Protocol for providing vertical
 *	synchronization timing
 * @destroy: destroy vsync feedback object
 * @get_vsync_timing: get vsync timing object for given wl_output
 *
 * The global interface that allows clients to subscribe for vertical
 * synchronization timing data for given wl_output.
 */
struct zwp_vsync_feedback_v1_interface {
	/**
	 * destroy - destroy vsync feedback object
	 *
	 * Destroy this vsync feedback object. Existing vsync timing
	 * objects shall not be affected by this request.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * get_vsync_timing - get vsync timing object for given wl_output
	 * @id: the new vsync timing interface id
	 * @output: the wl_output object to subscribe for timings of
	 *
	 * Create a new vsync timing object that represents a
	 * subscription to vertical synchronization timing updates of given
	 * wl_output object.
	 *
	 * The newly created object will immediately signal an update to
	 * notify the subscriber of initial timing parameters.
	 */
	void (*get_vsync_timing)(struct wl_client *client,
				 struct wl_resource *resource,
				 uint32_t id,
				 struct wl_resource *output);
};


struct zwp_vsync_timing_v1_interface {
	/**
	 * destroy - destroy vsync timing object
	 *
	 * Destroy this vsync timing object.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define ZWP_VSYNC_TIMING_V1_UPDATE	0

#define ZWP_VSYNC_TIMING_V1_UPDATE_SINCE_VERSION	1

static inline void
zwp_vsync_timing_v1_send_update(struct wl_resource *resource_, struct wl_resource *output, uint32_t timebase_l, uint32_t timebase_h, uint32_t interval_l, uint32_t interval_h)
{
	wl_resource_post_event(resource_, ZWP_VSYNC_TIMING_V1_UPDATE, output, timebase_l, timebase_h, interval_l, interval_h);
}

#ifdef  __cplusplus
}
#endif

#endif
