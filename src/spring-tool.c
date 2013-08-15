/*
 * Copyright © 2011 Intel Corporation
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

#include "compositor.h"

WL_EXPORT void
weston_surface_geometry_dirty(struct weston_surface *surface)
{
}

WL_EXPORT int
weston_log(const char *fmt, ...)
{
	return 0;
}

WL_EXPORT void
weston_compositor_schedule_repaint(struct weston_compositor *compositor)
{
}

int
main(int argc, char *argv[])
{
	const double k = 300.0;
	const double current = 0.5;
	const double target = 1.0;
	const double friction = 1400;

	struct weston_spring spring;
	uint32_t time = 0;

	weston_spring_init(&spring, k, current, target);
	spring.friction = friction;
	spring.previous = 0.48;
	spring.timestamp = 0;

	while (!weston_spring_done(&spring)) {
		printf("\t%d\t%f\n", time, spring.current);
		weston_spring_update(&spring, time);
		time += 16;
	}

	return 0;
}
