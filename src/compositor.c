/*
 * Copyright © 2010-2011 Intel Corporation
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2012-2015 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <linux/input.h>
#include <dlfcn.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include "timeline.h"

#include "compositor.h"
#include "scaler-server-protocol.h"
#include "presentation_timing-server-protocol.h"
#include "shared/helpers.h"
#include "shared/os-compatibility.h"
#include "shared/timespec-util.h"
#include "git-version.h"
#include "version.h"

#define DEFAULT_REPAINT_WINDOW 7 /* milliseconds */

static void
weston_output_transform_scale_init(struct weston_output *output,
				   uint32_t transform, uint32_t scale);

static void
weston_compositor_build_view_list(struct weston_compositor *compositor);

static void weston_mode_switch_finish(struct weston_output *output,
				      int mode_changed,
				      int scale_changed)
{
	struct weston_seat *seat;
	struct wl_resource *resource;
	pixman_region32_t old_output_region;
	int version;

	pixman_region32_init(&old_output_region);
	pixman_region32_copy(&old_output_region, &output->region);

	/* Update output region and transformation matrix */
	weston_output_transform_scale_init(output, output->transform, output->current_scale);

	pixman_region32_init(&output->previous_damage);
	pixman_region32_init_rect(&output->region, output->x, output->y,
				  output->width, output->height);

	weston_output_update_matrix(output);

	/* If a pointer falls outside the outputs new geometry, move it to its
	 * lower-right corner */
	wl_list_for_each(seat, &output->compositor->seat_list, link) {
		struct weston_pointer *pointer = weston_seat_get_pointer(seat);
		int32_t x, y;

		if (!pointer)
			continue;

		x = wl_fixed_to_int(pointer->x);
		y = wl_fixed_to_int(pointer->y);

		if (!pixman_region32_contains_point(&old_output_region,
						    x, y, NULL) ||
		    pixman_region32_contains_point(&output->region,
						   x, y, NULL))
			continue;

		if (x >= output->x + output->width)
			x = output->x + output->width - 1;
		if (y >= output->y + output->height)
			y = output->y + output->height - 1;

		pointer->x = wl_fixed_from_int(x);
		pointer->y = wl_fixed_from_int(y);
	}

	pixman_region32_fini(&old_output_region);

	if (!mode_changed && !scale_changed)
		return;

	/* notify clients of the changes */
	wl_resource_for_each(resource, &output->resource_list) {
		if (mode_changed) {
			wl_output_send_mode(resource,
					    output->current_mode->flags,
					    output->current_mode->width,
					    output->current_mode->height,
					    output->current_mode->refresh);
		}

		version = wl_resource_get_version(resource);
		if (version >= WL_OUTPUT_SCALE_SINCE_VERSION && scale_changed)
			wl_output_send_scale(resource, output->current_scale);

		if (version >= WL_OUTPUT_DONE_SINCE_VERSION)
			wl_output_send_done(resource);
	}
}

WL_EXPORT int
weston_output_mode_set_native(struct weston_output *output,
			      struct weston_mode *mode,
			      int32_t scale)
{
	int ret;
	int mode_changed = 0, scale_changed = 0;

	if (!output->switch_mode)
		return -1;

	if (!output->original_mode) {
		mode_changed = 1;
		ret = output->switch_mode(output, mode);
		if (ret < 0)
			return ret;
		if (output->current_scale != scale) {
			scale_changed = 1;
			output->current_scale = scale;
		}
	}

	output->native_mode = mode;
	output->native_scale = scale;

	weston_mode_switch_finish(output, mode_changed, scale_changed);

	return 0;
}

WL_EXPORT int
weston_output_mode_switch_to_native(struct weston_output *output)
{
	int ret;
	int mode_changed = 0, scale_changed = 0;

	if (!output->switch_mode)
		return -1;

	if (!output->original_mode) {
		weston_log("already in the native mode\n");
		return -1;
	}
	/* the non fullscreen clients haven't seen a mode set since we
	 * switched into a temporary, so we need to notify them if the
	 * mode at that time is different from the native mode now.
	 */
	mode_changed = (output->original_mode != output->native_mode);
	scale_changed = (output->original_scale != output->native_scale);

	ret = output->switch_mode(output, output->native_mode);
	if (ret < 0)
		return ret;

	output->current_scale = output->native_scale;

	output->original_mode = NULL;
	output->original_scale = 0;

	weston_mode_switch_finish(output, mode_changed, scale_changed);

	return 0;
}

WL_EXPORT int
weston_output_mode_switch_to_temporary(struct weston_output *output,
				       struct weston_mode *mode,
				       int32_t scale)
{
	int ret;

	if (!output->switch_mode)
		return -1;

	/* original_mode is the last mode non full screen clients have seen,
	 * so we shouldn't change it if we already have one set.
	 */
	if (!output->original_mode) {
		output->original_mode = output->native_mode;
		output->original_scale = output->native_scale;
	}
	ret = output->switch_mode(output, mode);
	if (ret < 0)
		return ret;

	output->current_scale = scale;

	weston_mode_switch_finish(output, 0, 0);

	return 0;
}

static void
child_client_exec(int sockfd, const char *path)
{
	int clientfd;
	char s[32];
	sigset_t allsigs;

	/* do not give our signal mask to the new process */
	sigfillset(&allsigs);
	sigprocmask(SIG_UNBLOCK, &allsigs, NULL);

	/* Launch clients as the user. Do not lauch clients with wrong euid.*/
	if (seteuid(getuid()) == -1) {
		weston_log("compositor: failed seteuid\n");
		return;
	}

	/* SOCK_CLOEXEC closes both ends, so we dup the fd to get a
	 * non-CLOEXEC fd to pass through exec. */
	clientfd = dup(sockfd);
	if (clientfd == -1) {
		weston_log("compositor: dup failed: %m\n");
		return;
	}

	snprintf(s, sizeof s, "%d", clientfd);
	setenv("WAYLAND_SOCKET", s, 1);

	if (execl(path, path, NULL) < 0)
		weston_log("compositor: executing '%s' failed: %m\n",
			path);
}

WL_EXPORT struct wl_client *
weston_client_launch(struct weston_compositor *compositor,
		     struct weston_process *proc,
		     const char *path,
		     weston_process_cleanup_func_t cleanup)
{
	int sv[2];
	pid_t pid;
	struct wl_client *client;

	weston_log("launching '%s'\n", path);

	if (os_socketpair_cloexec(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		weston_log("weston_client_launch: "
			"socketpair failed while launching '%s': %m\n",
			path);
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		close(sv[0]);
		close(sv[1]);
		weston_log("weston_client_launch: "
			"fork failed while launching '%s': %m\n", path);
		return NULL;
	}

	if (pid == 0) {
		child_client_exec(sv[1], path);
		_exit(-1);
	}

	close(sv[1]);

	client = wl_client_create(compositor->wl_display, sv[0]);
	if (!client) {
		close(sv[0]);
		weston_log("weston_client_launch: "
			"wl_client_create failed while launching '%s'.\n",
			path);
		return NULL;
	}

	proc->pid = pid;
	proc->cleanup = cleanup;
	weston_watch_process(proc);

	return client;
}

struct process_info {
	struct weston_process proc;
	char *path;
};

static void
process_handle_sigchld(struct weston_process *process, int status)
{
	struct process_info *pinfo =
		container_of(process, struct process_info, proc);

	/*
	 * There are no guarantees whether this runs before or after
	 * the wl_client destructor.
	 */

	if (WIFEXITED(status)) {
		weston_log("%s exited with status %d\n", pinfo->path,
			   WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		weston_log("%s died on signal %d\n", pinfo->path,
			   WTERMSIG(status));
	} else {
		weston_log("%s disappeared\n", pinfo->path);
	}

	free(pinfo->path);
	free(pinfo);
}

WL_EXPORT struct wl_client *
weston_client_start(struct weston_compositor *compositor, const char *path)
{
	struct process_info *pinfo;
	struct wl_client *client;

	pinfo = zalloc(sizeof *pinfo);
	if (!pinfo)
		return NULL;

	pinfo->path = strdup(path);
	if (!pinfo->path)
		goto out_free;

	client = weston_client_launch(compositor, &pinfo->proc, path,
				      process_handle_sigchld);
	if (!client)
		goto out_str;

	return client;

out_str:
	free(pinfo->path);

out_free:
	free(pinfo);

	return NULL;
}

static void
region_init_infinite(pixman_region32_t *region)
{
	pixman_region32_init_rect(region, INT32_MIN, INT32_MIN,
				  UINT32_MAX, UINT32_MAX);
}

static struct weston_subsurface *
weston_surface_to_subsurface(struct weston_surface *surface);

WL_EXPORT struct weston_view *
weston_view_create(struct weston_surface *surface)
{
	struct weston_view *view;

	view = zalloc(sizeof *view);
	if (view == NULL)
		return NULL;

	view->surface = surface;

	/* Assign to surface */
	wl_list_insert(&surface->views, &view->surface_link);

	wl_signal_init(&view->destroy_signal);
	wl_list_init(&view->link);
	wl_list_init(&view->layer_link.link);

	pixman_region32_init(&view->clip);

	view->alpha = 1.0;
	pixman_region32_init(&view->transform.opaque);

	wl_list_init(&view->geometry.transformation_list);
	wl_list_insert(&view->geometry.transformation_list,
		       &view->transform.position.link);
	weston_matrix_init(&view->transform.position.matrix);
	wl_list_init(&view->geometry.child_list);
	pixman_region32_init(&view->geometry.scissor);
	pixman_region32_init(&view->transform.boundingbox);
	view->transform.dirty = 1;

	return view;
}

struct weston_frame_callback {
	struct wl_resource *resource;
	struct wl_list link;
};

struct weston_presentation_feedback {
	struct wl_resource *resource;

	/* XXX: could use just wl_resource_get_link() instead */
	struct wl_list link;

	/* The per-surface feedback flags */
	uint32_t psf_flags;
};

static void
weston_presentation_feedback_discard(
		struct weston_presentation_feedback *feedback)
{
	presentation_feedback_send_discarded(feedback->resource);
	wl_resource_destroy(feedback->resource);
}

static void
weston_presentation_feedback_discard_list(struct wl_list *list)
{
	struct weston_presentation_feedback *feedback, *tmp;

	wl_list_for_each_safe(feedback, tmp, list, link)
		weston_presentation_feedback_discard(feedback);
}

static void
weston_presentation_feedback_present(
		struct weston_presentation_feedback *feedback,
		struct weston_output *output,
		uint32_t refresh_nsec,
		const struct timespec *ts,
		uint64_t seq,
		uint32_t flags)
{
	struct wl_client *client = wl_resource_get_client(feedback->resource);
	struct wl_resource *o;
	uint64_t secs;

	wl_resource_for_each(o, &output->resource_list) {
		if (wl_resource_get_client(o) != client)
			continue;

		presentation_feedback_send_sync_output(feedback->resource, o);
	}

	secs = ts->tv_sec;
	presentation_feedback_send_presented(feedback->resource,
					     secs >> 32, secs & 0xffffffff,
					     ts->tv_nsec,
					     refresh_nsec,
					     seq >> 32, seq & 0xffffffff,
					     flags | feedback->psf_flags);
	wl_resource_destroy(feedback->resource);
}

static void
weston_presentation_feedback_present_list(struct wl_list *list,
					  struct weston_output *output,
					  uint32_t refresh_nsec,
					  const struct timespec *ts,
					  uint64_t seq,
					  uint32_t flags)
{
	struct weston_presentation_feedback *feedback, *tmp;

	assert(!(flags & PRESENTATION_FEEDBACK_INVALID) ||
	       wl_list_empty(list));

	wl_list_for_each_safe(feedback, tmp, list, link)
		weston_presentation_feedback_present(feedback, output,
						     refresh_nsec, ts, seq,
						     flags);
}

static void
surface_state_handle_buffer_destroy(struct wl_listener *listener, void *data)
{
	struct weston_surface_state *state =
		container_of(listener, struct weston_surface_state,
			     buffer_destroy_listener);

	state->buffer = NULL;
}

static void
weston_surface_state_init(struct weston_surface_state *state)
{
	state->newly_attached = 0;
	state->buffer = NULL;
	state->buffer_destroy_listener.notify =
		surface_state_handle_buffer_destroy;
	state->sx = 0;
	state->sy = 0;

	pixman_region32_init(&state->damage);
	pixman_region32_init(&state->opaque);
	region_init_infinite(&state->input);

	wl_list_init(&state->frame_callback_list);
	wl_list_init(&state->feedback_list);

	state->buffer_viewport.buffer.transform = WL_OUTPUT_TRANSFORM_NORMAL;
	state->buffer_viewport.buffer.scale = 1;
	state->buffer_viewport.buffer.src_width = wl_fixed_from_int(-1);
	state->buffer_viewport.surface.width = -1;
	state->buffer_viewport.changed = 0;
}

static void
weston_surface_state_fini(struct weston_surface_state *state)
{
	struct weston_frame_callback *cb, *next;

	wl_list_for_each_safe(cb, next,
			      &state->frame_callback_list, link)
		wl_resource_destroy(cb->resource);

	weston_presentation_feedback_discard_list(&state->feedback_list);

	pixman_region32_fini(&state->input);
	pixman_region32_fini(&state->opaque);
	pixman_region32_fini(&state->damage);

	if (state->buffer)
		wl_list_remove(&state->buffer_destroy_listener.link);
	state->buffer = NULL;
}

static void
weston_surface_state_set_buffer(struct weston_surface_state *state,
				struct weston_buffer *buffer)
{
	if (state->buffer == buffer)
		return;

	if (state->buffer)
		wl_list_remove(&state->buffer_destroy_listener.link);
	state->buffer = buffer;
	if (state->buffer)
		wl_signal_add(&state->buffer->destroy_signal,
			      &state->buffer_destroy_listener);
}

WL_EXPORT struct weston_surface *
weston_surface_create(struct weston_compositor *compositor)
{
	struct weston_surface *surface;

	surface = zalloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	wl_signal_init(&surface->destroy_signal);

	surface->compositor = compositor;
	surface->ref_count = 1;

	surface->buffer_viewport.buffer.transform = WL_OUTPUT_TRANSFORM_NORMAL;
	surface->buffer_viewport.buffer.scale = 1;
	surface->buffer_viewport.buffer.src_width = wl_fixed_from_int(-1);
	surface->buffer_viewport.surface.width = -1;

	weston_surface_state_init(&surface->pending);

	pixman_region32_init(&surface->damage);
	pixman_region32_init(&surface->opaque);
	region_init_infinite(&surface->input);

	wl_list_init(&surface->views);

	wl_list_init(&surface->frame_callback_list);
	wl_list_init(&surface->feedback_list);

	wl_list_init(&surface->subsurface_list);
	wl_list_init(&surface->subsurface_list_pending);

	weston_matrix_init(&surface->buffer_to_surface_matrix);
	weston_matrix_init(&surface->surface_to_buffer_matrix);

	return surface;
}

WL_EXPORT void
weston_surface_set_color(struct weston_surface *surface,
		 float red, float green, float blue, float alpha)
{
	surface->compositor->renderer->surface_set_color(surface, red, green, blue, alpha);
}

WL_EXPORT void
weston_view_to_global_float(struct weston_view *view,
			    float sx, float sy, float *x, float *y)
{
	if (view->transform.enabled) {
		struct weston_vector v = { { sx, sy, 0.0f, 1.0f } };

		weston_matrix_transform(&view->transform.matrix, &v);

		if (fabsf(v.f[3]) < 1e-6) {
			weston_log("warning: numerical instability in "
				"%s(), divisor = %g\n", __func__,
				v.f[3]);
			*x = 0;
			*y = 0;
			return;
		}

		*x = v.f[0] / v.f[3];
		*y = v.f[1] / v.f[3];
	} else {
		*x = sx + view->geometry.x;
		*y = sy + view->geometry.y;
	}
}

WL_EXPORT void
weston_transformed_coord(int width, int height,
			 enum wl_output_transform transform,
			 int32_t scale,
			 float sx, float sy, float *bx, float *by)
{
	switch (transform) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
	default:
		*bx = sx;
		*by = sy;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		*bx = width - sx;
		*by = sy;
		break;
	case WL_OUTPUT_TRANSFORM_90:
		*bx = height - sy;
		*by = sx;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		*bx = height - sy;
		*by = width - sx;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		*bx = width - sx;
		*by = height - sy;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		*bx = sx;
		*by = height - sy;
		break;
	case WL_OUTPUT_TRANSFORM_270:
		*bx = sy;
		*by = width - sx;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		*bx = sy;
		*by = sx;
		break;
	}

	*bx *= scale;
	*by *= scale;
}

WL_EXPORT pixman_box32_t
weston_transformed_rect(int width, int height,
			enum wl_output_transform transform,
			int32_t scale,
			pixman_box32_t rect)
{
	float x1, x2, y1, y2;

	pixman_box32_t ret;

	weston_transformed_coord(width, height, transform, scale,
				 rect.x1, rect.y1, &x1, &y1);
	weston_transformed_coord(width, height, transform, scale,
				 rect.x2, rect.y2, &x2, &y2);

	if (x1 <= x2) {
		ret.x1 = x1;
		ret.x2 = x2;
	} else {
		ret.x1 = x2;
		ret.x2 = x1;
	}

	if (y1 <= y2) {
		ret.y1 = y1;
		ret.y2 = y2;
	} else {
		ret.y1 = y2;
		ret.y2 = y1;
	}

	return ret;
}

WL_EXPORT void
weston_transformed_region(int width, int height,
			  enum wl_output_transform transform,
			  int32_t scale,
			  pixman_region32_t *src, pixman_region32_t *dest)
{
	pixman_box32_t *src_rects, *dest_rects;
	int nrects, i;

	if (transform == WL_OUTPUT_TRANSFORM_NORMAL && scale == 1) {
		if (src != dest)
			pixman_region32_copy(dest, src);
		return;
	}

	src_rects = pixman_region32_rectangles(src, &nrects);
	dest_rects = malloc(nrects * sizeof(*dest_rects));
	if (!dest_rects)
		return;

	if (transform == WL_OUTPUT_TRANSFORM_NORMAL) {
		memcpy(dest_rects, src_rects, nrects * sizeof(*dest_rects));
	} else {
		for (i = 0; i < nrects; i++) {
			switch (transform) {
			default:
			case WL_OUTPUT_TRANSFORM_NORMAL:
				dest_rects[i].x1 = src_rects[i].x1;
				dest_rects[i].y1 = src_rects[i].y1;
				dest_rects[i].x2 = src_rects[i].x2;
				dest_rects[i].y2 = src_rects[i].y2;
				break;
			case WL_OUTPUT_TRANSFORM_90:
				dest_rects[i].x1 = height - src_rects[i].y2;
				dest_rects[i].y1 = src_rects[i].x1;
				dest_rects[i].x2 = height - src_rects[i].y1;
				dest_rects[i].y2 = src_rects[i].x2;
				break;
			case WL_OUTPUT_TRANSFORM_180:
				dest_rects[i].x1 = width - src_rects[i].x2;
				dest_rects[i].y1 = height - src_rects[i].y2;
				dest_rects[i].x2 = width - src_rects[i].x1;
				dest_rects[i].y2 = height - src_rects[i].y1;
				break;
			case WL_OUTPUT_TRANSFORM_270:
				dest_rects[i].x1 = src_rects[i].y1;
				dest_rects[i].y1 = width - src_rects[i].x2;
				dest_rects[i].x2 = src_rects[i].y2;
				dest_rects[i].y2 = width - src_rects[i].x1;
				break;
			case WL_OUTPUT_TRANSFORM_FLIPPED:
				dest_rects[i].x1 = width - src_rects[i].x2;
				dest_rects[i].y1 = src_rects[i].y1;
				dest_rects[i].x2 = width - src_rects[i].x1;
				dest_rects[i].y2 = src_rects[i].y2;
				break;
			case WL_OUTPUT_TRANSFORM_FLIPPED_90:
				dest_rects[i].x1 = height - src_rects[i].y2;
				dest_rects[i].y1 = width - src_rects[i].x2;
				dest_rects[i].x2 = height - src_rects[i].y1;
				dest_rects[i].y2 = width - src_rects[i].x1;
				break;
			case WL_OUTPUT_TRANSFORM_FLIPPED_180:
				dest_rects[i].x1 = src_rects[i].x1;
				dest_rects[i].y1 = height - src_rects[i].y2;
				dest_rects[i].x2 = src_rects[i].x2;
				dest_rects[i].y2 = height - src_rects[i].y1;
				break;
			case WL_OUTPUT_TRANSFORM_FLIPPED_270:
				dest_rects[i].x1 = src_rects[i].y1;
				dest_rects[i].y1 = src_rects[i].x1;
				dest_rects[i].x2 = src_rects[i].y2;
				dest_rects[i].y2 = src_rects[i].x2;
				break;
			}
		}
	}

	if (scale != 1) {
		for (i = 0; i < nrects; i++) {
			dest_rects[i].x1 *= scale;
			dest_rects[i].x2 *= scale;
			dest_rects[i].y1 *= scale;
			dest_rects[i].y2 *= scale;
		}
	}

	pixman_region32_clear(dest);
	pixman_region32_init_rects(dest, dest_rects, nrects);
	free(dest_rects);
}

static void
scaler_surface_to_buffer(struct weston_surface *surface,
			 float sx, float sy, float *bx, float *by)
{
	struct weston_buffer_viewport *vp = &surface->buffer_viewport;
	double src_width, src_height;
	double src_x, src_y;

	if (vp->buffer.src_width == wl_fixed_from_int(-1)) {
		if (vp->surface.width == -1) {
			*bx = sx;
			*by = sy;
			return;
		}

		src_x = 0.0;
		src_y = 0.0;
		src_width = surface->width_from_buffer;
		src_height = surface->height_from_buffer;
	} else {
		src_x = wl_fixed_to_double(vp->buffer.src_x);
		src_y = wl_fixed_to_double(vp->buffer.src_y);
		src_width = wl_fixed_to_double(vp->buffer.src_width);
		src_height = wl_fixed_to_double(vp->buffer.src_height);
	}

	*bx = sx * src_width / surface->width + src_x;
	*by = sy * src_height / surface->height + src_y;
}

WL_EXPORT void
weston_surface_to_buffer_float(struct weston_surface *surface,
			       float sx, float sy, float *bx, float *by)
{
	struct weston_buffer_viewport *vp = &surface->buffer_viewport;

	/* first transform coordinates if the scaler is set */
	scaler_surface_to_buffer(surface, sx, sy, bx, by);

	weston_transformed_coord(surface->width_from_buffer,
				 surface->height_from_buffer,
				 vp->buffer.transform, vp->buffer.scale,
				 *bx, *by, bx, by);
}

WL_EXPORT void
weston_surface_to_buffer(struct weston_surface *surface,
			 int sx, int sy, int *bx, int *by)
{
	float bxf, byf;

	weston_surface_to_buffer_float(surface,
				       sx, sy, &bxf, &byf);

	*bx = floorf(bxf);
	*by = floorf(byf);
}

WL_EXPORT pixman_box32_t
weston_surface_to_buffer_rect(struct weston_surface *surface,
			      pixman_box32_t rect)
{
	struct weston_buffer_viewport *vp = &surface->buffer_viewport;
	float xf, yf;

	/* first transform box coordinates if the scaler is set */
	scaler_surface_to_buffer(surface, rect.x1, rect.y1, &xf, &yf);
	rect.x1 = floorf(xf);
	rect.y1 = floorf(yf);

	scaler_surface_to_buffer(surface, rect.x2, rect.y2, &xf, &yf);
	rect.x2 = floorf(xf);
	rect.y2 = floorf(yf);

	return weston_transformed_rect(surface->width_from_buffer,
				       surface->height_from_buffer,
				       vp->buffer.transform, vp->buffer.scale,
				       rect);
}

/** Transform a region from surface coordinates to buffer coordinates
 *
 * \param surface The surface to fetch wl_viewport and buffer transformation
 * from.
 * \param surface_region[in] The region in surface coordinates.
 * \param buffer_region[out] The region converted to buffer coordinates.
 *
 * Buffer_region must be init'd, but will be completely overwritten.
 *
 * Viewport and buffer transformations can only do translation, scaling,
 * and rotations in 90-degree steps. Therefore the only loss in the
 * conversion is coordinate flooring (rounding).
 */
WL_EXPORT void
weston_surface_to_buffer_region(struct weston_surface *surface,
				pixman_region32_t *surface_region,
				pixman_region32_t *buffer_region)
{
	pixman_box32_t *src_rects, *dest_rects;
	int nrects, i;

	src_rects = pixman_region32_rectangles(surface_region, &nrects);
	dest_rects = malloc(nrects * sizeof(*dest_rects));
	if (!dest_rects)
		return;

	for (i = 0; i < nrects; i++) {
		dest_rects[i] = weston_surface_to_buffer_rect(surface,
							      src_rects[i]);
	}

	pixman_region32_fini(buffer_region);
	pixman_region32_init_rects(buffer_region, dest_rects, nrects);
	free(dest_rects);
}

WL_EXPORT void
weston_view_move_to_plane(struct weston_view *view,
			     struct weston_plane *plane)
{
	if (view->plane == plane)
		return;

	weston_view_damage_below(view);
	view->plane = plane;
	weston_surface_damage(view->surface);
}

/** Inflict damage on the plane where the view is visible.
 *
 * \param view The view that causes the damage.
 *
 * If the view is currently on a plane (including the primary plane),
 * take the view's boundingbox, subtract all the opaque views that cover it,
 * and add the remaining region as damage to the plane. This corresponds
 * to the damage inflicted to the plane if this view disappeared.
 *
 * A repaint is scheduled for this view.
 *
 * The region of all opaque views covering this view is stored in
 * weston_view::clip and updated by view_accumulate_damage() during
 * weston_output_repaint(). Specifically, that region matches the
 * scenegraph as it was last painted.
 */
WL_EXPORT void
weston_view_damage_below(struct weston_view *view)
{
	pixman_region32_t damage;

	pixman_region32_init(&damage);
	pixman_region32_subtract(&damage, &view->transform.boundingbox,
				 &view->clip);
	if (view->plane)
		pixman_region32_union(&view->plane->damage,
				      &view->plane->damage, &damage);
	pixman_region32_fini(&damage);
	weston_view_schedule_repaint(view);
}

static void
weston_surface_update_output_mask(struct weston_surface *es, uint32_t mask)
{
	uint32_t different = es->output_mask ^ mask;
	uint32_t entered = mask & different;
	uint32_t left = es->output_mask & different;
	struct weston_output *output;
	struct wl_resource *resource = NULL;
	struct wl_client *client;

	es->output_mask = mask;
	if (es->resource == NULL)
		return;
	if (different == 0)
		return;

	client = wl_resource_get_client(es->resource);

	wl_list_for_each(output, &es->compositor->output_list, link) {
		if (1 << output->id & different)
			resource =
				wl_resource_find_for_client(&output->resource_list,
							 client);
		if (resource == NULL)
			continue;
		if (1 << output->id & entered)
			wl_surface_send_enter(es->resource, resource);
		if (1 << output->id & left)
			wl_surface_send_leave(es->resource, resource);
	}
}


static void
weston_surface_assign_output(struct weston_surface *es)
{
	struct weston_output *new_output;
	struct weston_view *view;
	pixman_region32_t region;
	uint32_t max, area, mask;
	pixman_box32_t *e;

	new_output = NULL;
	max = 0;
	mask = 0;
	pixman_region32_init(&region);
	wl_list_for_each(view, &es->views, surface_link) {
		if (!view->output)
			continue;

		pixman_region32_intersect(&region, &view->transform.boundingbox,
					  &view->output->region);

		e = pixman_region32_extents(&region);
		area = (e->x2 - e->x1) * (e->y2 - e->y1);

		mask |= view->output_mask;

		if (area >= max) {
			new_output = view->output;
			max = area;
		}
	}
	pixman_region32_fini(&region);

	es->output = new_output;
	weston_surface_update_output_mask(es, mask);
}

static void
weston_view_assign_output(struct weston_view *ev)
{
	struct weston_compositor *ec = ev->surface->compositor;
	struct weston_output *output, *new_output;
	pixman_region32_t region;
	uint32_t max, area, mask;
	pixman_box32_t *e;

	new_output = NULL;
	max = 0;
	mask = 0;
	pixman_region32_init(&region);
	wl_list_for_each(output, &ec->output_list, link) {
		if (output->destroying)
			continue;

		pixman_region32_intersect(&region, &ev->transform.boundingbox,
					  &output->region);

		e = pixman_region32_extents(&region);
		area = (e->x2 - e->x1) * (e->y2 - e->y1);

		if (area > 0)
			mask |= 1 << output->id;

		if (area >= max) {
			new_output = output;
			max = area;
		}
	}
	pixman_region32_fini(&region);

	ev->output = new_output;
	ev->output_mask = mask;

	weston_surface_assign_output(ev->surface);
}

static void
weston_view_to_view_map(struct weston_view *from, struct weston_view *to,
			int from_x, int from_y, int *to_x, int *to_y)
{
	float x, y;

	weston_view_to_global_float(from, from_x, from_y, &x, &y);
	weston_view_from_global_float(to, x, y, &x, &y);

	*to_x = round(x);
	*to_y = round(y);
}

static void
weston_view_transfer_scissor(struct weston_view *from, struct weston_view *to)
{
	pixman_box32_t *a;
	pixman_box32_t b;

	a = pixman_region32_extents(&from->geometry.scissor);

	weston_view_to_view_map(from, to, a->x1, a->y1, &b.x1, &b.y1);
	weston_view_to_view_map(from, to, a->x2, a->y2, &b.x2, &b.y2);

	pixman_region32_fini(&to->geometry.scissor);
	pixman_region32_init_with_extents(&to->geometry.scissor, &b);
}

static void
view_compute_bbox(struct weston_view *view, const pixman_box32_t *inbox,
		  pixman_region32_t *bbox)
{
	float min_x = HUGE_VALF,  min_y = HUGE_VALF;
	float max_x = -HUGE_VALF, max_y = -HUGE_VALF;
	int32_t s[4][2] = {
		{ inbox->x1, inbox->y1 },
		{ inbox->x1, inbox->y2 },
		{ inbox->x2, inbox->y1 },
		{ inbox->x2, inbox->y2 },
	};
	float int_x, int_y;
	int i;

	if (inbox->x1 == inbox->x2 || inbox->y1 == inbox->y2) {
		/* avoid rounding empty bbox to 1x1 */
		pixman_region32_init(bbox);
		return;
	}

	for (i = 0; i < 4; ++i) {
		float x, y;
		weston_view_to_global_float(view, s[i][0], s[i][1], &x, &y);
		if (x < min_x)
			min_x = x;
		if (x > max_x)
			max_x = x;
		if (y < min_y)
			min_y = y;
		if (y > max_y)
			max_y = y;
	}

	int_x = floorf(min_x);
	int_y = floorf(min_y);
	pixman_region32_init_rect(bbox, int_x, int_y,
				  ceilf(max_x) - int_x, ceilf(max_y) - int_y);
}

static void
weston_view_update_transform_disable(struct weston_view *view)
{
	view->transform.enabled = 0;

	/* round off fractions when not transformed */
	view->geometry.x = roundf(view->geometry.x);
	view->geometry.y = roundf(view->geometry.y);

	/* Otherwise identity matrix, but with x and y translation. */
	view->transform.position.matrix.type = WESTON_MATRIX_TRANSFORM_TRANSLATE;
	view->transform.position.matrix.d[12] = view->geometry.x;
	view->transform.position.matrix.d[13] = view->geometry.y;

	view->transform.matrix = view->transform.position.matrix;

	view->transform.inverse = view->transform.position.matrix;
	view->transform.inverse.d[12] = -view->geometry.x;
	view->transform.inverse.d[13] = -view->geometry.y;

	pixman_region32_init_rect(&view->transform.boundingbox,
				  0, 0,
				  view->surface->width,
				  view->surface->height);
	if (view->geometry.scissor_enabled)
		pixman_region32_intersect(&view->transform.boundingbox,
					  &view->transform.boundingbox,
					  &view->geometry.scissor);

	pixman_region32_translate(&view->transform.boundingbox,
				  view->geometry.x, view->geometry.y);

	if (view->alpha == 1.0) {
		pixman_region32_copy(&view->transform.opaque,
				     &view->surface->opaque);
		pixman_region32_translate(&view->transform.opaque,
					  view->geometry.x,
					  view->geometry.y);
	}
}

static int
weston_view_update_transform_enable(struct weston_view *view)
{
	struct weston_view *parent = view->geometry.parent;
	struct weston_matrix *matrix = &view->transform.matrix;
	struct weston_matrix *inverse = &view->transform.inverse;
	struct weston_transform *tform;
	pixman_region32_t surfregion;
	const pixman_box32_t *surfbox;

	view->transform.enabled = 1;

	/* Otherwise identity matrix, but with x and y translation. */
	view->transform.position.matrix.type = WESTON_MATRIX_TRANSFORM_TRANSLATE;
	view->transform.position.matrix.d[12] = view->geometry.x;
	view->transform.position.matrix.d[13] = view->geometry.y;

	weston_matrix_init(matrix);
	wl_list_for_each(tform, &view->geometry.transformation_list, link)
		weston_matrix_multiply(matrix, &tform->matrix);

	if (parent)
		weston_matrix_multiply(matrix, &parent->transform.matrix);

	if (weston_matrix_invert(inverse, matrix) < 0) {
		/* Oops, bad total transformation, not invertible */
		weston_log("error: weston_view %p"
			" transformation not invertible.\n", view);
		return -1;
	}

	pixman_region32_init_rect(&surfregion, 0, 0,
				  view->surface->width, view->surface->height);
	if (view->geometry.scissor_enabled)
		pixman_region32_intersect(&surfregion, &surfregion,
					  &view->geometry.scissor);
	surfbox = pixman_region32_extents(&surfregion);

	view_compute_bbox(view, surfbox, &view->transform.boundingbox);
	pixman_region32_fini(&surfregion);

	return 0;
}

static struct weston_layer *
get_view_layer(struct weston_view *view)
{
	if (view->parent_view)
		return get_view_layer(view->parent_view);
	return view->layer_link.layer;
}

WL_EXPORT void
weston_view_update_transform(struct weston_view *view)
{
	struct weston_view *parent = view->geometry.parent;
	struct weston_layer *layer;
	pixman_region32_t mask;

	if (!view->transform.dirty)
		return;

	if (parent)
		weston_view_update_transform(parent);

	view->transform.dirty = 0;

	weston_view_damage_below(view);

	pixman_region32_fini(&view->transform.boundingbox);
	pixman_region32_fini(&view->transform.opaque);
	pixman_region32_init(&view->transform.opaque);

	/* transform.position is always in transformation_list */
	if (view->geometry.transformation_list.next ==
	    &view->transform.position.link &&
	    view->geometry.transformation_list.prev ==
	    &view->transform.position.link &&
	    !parent) {
		weston_view_update_transform_disable(view);
	} else {
		if (weston_view_update_transform_enable(view) < 0)
			weston_view_update_transform_disable(view);
	}

	layer = get_view_layer(view);
	if (layer) {
		pixman_region32_init_with_extents(&mask, &layer->mask);
		pixman_region32_intersect(&view->transform.boundingbox,
					  &view->transform.boundingbox, &mask);
		pixman_region32_intersect(&view->transform.opaque,
					  &view->transform.opaque, &mask);
		pixman_region32_fini(&mask);
	}

	if (parent) {
		if (parent->geometry.scissor_enabled) {
			view->geometry.scissor_enabled = true;
			weston_view_transfer_scissor(parent, view);
		} else {
			view->geometry.scissor_enabled = false;
		}
	}

	weston_view_damage_below(view);

	weston_view_assign_output(view);

	wl_signal_emit(&view->surface->compositor->transform_signal,
		       view->surface);
}

WL_EXPORT void
weston_view_geometry_dirty(struct weston_view *view)
{
	struct weston_view *child;

	/*
	 * The invariant: if view->geometry.dirty, then all views
	 * in view->geometry.child_list have geometry.dirty too.
	 * Corollary: if not parent->geometry.dirty, then all ancestors
	 * are not dirty.
	 */

	if (view->transform.dirty)
		return;

	view->transform.dirty = 1;

	wl_list_for_each(child, &view->geometry.child_list,
			 geometry.parent_link)
		weston_view_geometry_dirty(child);
}

WL_EXPORT void
weston_view_to_global_fixed(struct weston_view *view,
			    wl_fixed_t vx, wl_fixed_t vy,
			    wl_fixed_t *x, wl_fixed_t *y)
{
	float xf, yf;

	weston_view_to_global_float(view,
				    wl_fixed_to_double(vx),
				    wl_fixed_to_double(vy),
				    &xf, &yf);
	*x = wl_fixed_from_double(xf);
	*y = wl_fixed_from_double(yf);
}

WL_EXPORT void
weston_view_from_global_float(struct weston_view *view,
			      float x, float y, float *vx, float *vy)
{
	if (view->transform.enabled) {
		struct weston_vector v = { { x, y, 0.0f, 1.0f } };

		weston_matrix_transform(&view->transform.inverse, &v);

		if (fabsf(v.f[3]) < 1e-6) {
			weston_log("warning: numerical instability in "
				"weston_view_from_global(), divisor = %g\n",
				v.f[3]);
			*vx = 0;
			*vy = 0;
			return;
		}

		*vx = v.f[0] / v.f[3];
		*vy = v.f[1] / v.f[3];
	} else {
		*vx = x - view->geometry.x;
		*vy = y - view->geometry.y;
	}
}

WL_EXPORT void
weston_view_from_global_fixed(struct weston_view *view,
			      wl_fixed_t x, wl_fixed_t y,
			      wl_fixed_t *vx, wl_fixed_t *vy)
{
	float vxf, vyf;

	weston_view_from_global_float(view,
				      wl_fixed_to_double(x),
				      wl_fixed_to_double(y),
				      &vxf, &vyf);
	*vx = wl_fixed_from_double(vxf);
	*vy = wl_fixed_from_double(vyf);
}

WL_EXPORT void
weston_view_from_global(struct weston_view *view,
			int32_t x, int32_t y, int32_t *vx, int32_t *vy)
{
	float vxf, vyf;

	weston_view_from_global_float(view, x, y, &vxf, &vyf);
	*vx = floorf(vxf);
	*vy = floorf(vyf);
}

WL_EXPORT void
weston_surface_schedule_repaint(struct weston_surface *surface)
{
	struct weston_output *output;

	wl_list_for_each(output, &surface->compositor->output_list, link)
		if (surface->output_mask & (1 << output->id))
			weston_output_schedule_repaint(output);
}

WL_EXPORT void
weston_view_schedule_repaint(struct weston_view *view)
{
	struct weston_output *output;

	wl_list_for_each(output, &view->surface->compositor->output_list, link)
		if (view->output_mask & (1 << output->id))
			weston_output_schedule_repaint(output);
}

/**
 * XXX: This function does it the wrong way.
 * surface->damage is the damage from the client, and causes
 * surface_flush_damage() to copy pixels. No window management action can
 * cause damage to the client-provided content, warranting re-upload!
 *
 * Instead of surface->damage, this function should record the damage
 * with all the views for this surface to avoid extraneous texture
 * uploads.
 */
WL_EXPORT void
weston_surface_damage(struct weston_surface *surface)
{
	pixman_region32_union_rect(&surface->damage, &surface->damage,
				   0, 0, surface->width,
				   surface->height);

	weston_surface_schedule_repaint(surface);
}

WL_EXPORT void
weston_view_set_position(struct weston_view *view, float x, float y)
{
	if (view->geometry.x == x && view->geometry.y == y)
		return;

	view->geometry.x = x;
	view->geometry.y = y;
	weston_view_geometry_dirty(view);
}

static void
transform_parent_handle_parent_destroy(struct wl_listener *listener,
				       void *data)
{
	struct weston_view *view =
		container_of(listener, struct weston_view,
			     geometry.parent_destroy_listener);

	weston_view_set_transform_parent(view, NULL);
}

WL_EXPORT void
weston_view_set_transform_parent(struct weston_view *view,
				 struct weston_view *parent)
{
	if (view->geometry.parent) {
		wl_list_remove(&view->geometry.parent_destroy_listener.link);
		wl_list_remove(&view->geometry.parent_link);

		if (!parent)
			view->geometry.scissor_enabled = false;
	}

	view->geometry.parent = parent;

	view->geometry.parent_destroy_listener.notify =
		transform_parent_handle_parent_destroy;
	if (parent) {
		wl_signal_add(&parent->destroy_signal,
			      &view->geometry.parent_destroy_listener);
		wl_list_insert(&parent->geometry.child_list,
			       &view->geometry.parent_link);
	}

	weston_view_geometry_dirty(view);
}

/** Set a clip mask rectangle on a view
 *
 * \param view The view to set the clip mask on.
 * \param x Top-left corner X coordinate of the clip rectangle.
 * \param y Top-left corner Y coordinate of the clip rectangle.
 * \param width Width of the clip rectangle, non-negative.
 * \param height Height of the clip rectangle, non-negative.
 *
 * A shell may set a clip mask rectangle on a view. Everything outside
 * the rectangle is cut away for input and output purposes: it is
 * not drawn and cannot be hit by hit-test based input like pointer
 * motion or touch-downs. Everything inside the rectangle will behave
 * normally. Clients are unaware of clipping.
 *
 * The rectangle is set in the surface local coordinates. Setting a clip
 * mask rectangle does not affect the view position, the view is positioned
 * as it would be without a clip. The clip also does not change
 * weston_surface::width,height.
 *
 * The clip mask rectangle is part of transformation inheritance
 * (weston_view_set_transform_parent()). A clip set in the root of the
 * transformation inheritance tree will affect all views in the tree.
 * A clip can be set only on the root view. Attempting to set a clip
 * on view that has a transformation parent will fail. Assigning a parent
 * to a view that has a clip set will cause the clip to be forgotten.
 *
 * Because the clip mask is an axis-aligned rectangle, it poses restrictions
 * on the additional transformations in the child views. These transformations
 * may not rotate the coordinate axes, i.e., only translation and scaling
 * are allowed. Violating this restriction causes the clipping to malfunction.
 * Furthermore, using scaling may cause rounding errors in child clipping.
 *
 * The clip mask rectangle is not automatically adjusted based on
 * wl_surface.attach dx and dy arguments.
 *
 * A clip mask rectangle can be set only if the compositor capability
 * WESTON_CAP_VIEW_CLIP_MASK is present.
 *
 * This function sets the clip mask rectangle and schedules a repaint for
 * the view.
 */
WL_EXPORT void
weston_view_set_mask(struct weston_view *view,
		     int x, int y, int width, int height)
{
	struct weston_compositor *compositor = view->surface->compositor;

	if (!(compositor->capabilities & WESTON_CAP_VIEW_CLIP_MASK)) {
		weston_log("%s not allowed without capability!\n", __func__);
		return;
	}

	if (view->geometry.parent) {
		weston_log("view %p has a parent, clip forbidden!\n", view);
		return;
	}

	if (width < 0 || height < 0) {
		weston_log("%s: illegal args %d, %d, %d, %d\n", __func__,
			   x, y, width, height);
		return;
	}

	pixman_region32_fini(&view->geometry.scissor);
	pixman_region32_init_rect(&view->geometry.scissor, x, y, width, height);
	view->geometry.scissor_enabled = true;
	weston_view_geometry_dirty(view);
	weston_view_schedule_repaint(view);
}

/** Remove the clip mask from a view
 *
 * \param view The view to remove the clip mask from.
 *
 * Removed the clip mask rectangle and schedules a repaint.
 *
 * \sa weston_view_set_mask
 */
WL_EXPORT void
weston_view_set_mask_infinite(struct weston_view *view)
{
	view->geometry.scissor_enabled = false;
	weston_view_geometry_dirty(view);
	weston_view_schedule_repaint(view);
}

WL_EXPORT bool
weston_view_is_mapped(struct weston_view *view)
{
	if (view->output)
		return true;
	else
		return false;
}

WL_EXPORT bool
weston_surface_is_mapped(struct weston_surface *surface)
{
	if (surface->output)
		return true;
	else
		return false;
}

static void
surface_set_size(struct weston_surface *surface, int32_t width, int32_t height)
{
	struct weston_view *view;

	if (surface->width == width && surface->height == height)
		return;

	surface->width = width;
	surface->height = height;

	wl_list_for_each(view, &surface->views, surface_link)
		weston_view_geometry_dirty(view);
}

WL_EXPORT void
weston_surface_set_size(struct weston_surface *surface,
			int32_t width, int32_t height)
{
	assert(!surface->resource);
	surface_set_size(surface, width, height);
}

static int
fixed_round_up_to_int(wl_fixed_t f)
{
	return wl_fixed_to_int(wl_fixed_from_int(1) - 1 + f);
}

static void
weston_surface_calculate_size_from_buffer(struct weston_surface *surface)
{
	struct weston_buffer_viewport *vp = &surface->buffer_viewport;
	int32_t width, height;

	if (!surface->buffer_ref.buffer) {
		surface->width_from_buffer = 0;
		surface->height_from_buffer = 0;
		return;
	}

	switch (vp->buffer.transform) {
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		width = surface->buffer_ref.buffer->height / vp->buffer.scale;
		height = surface->buffer_ref.buffer->width / vp->buffer.scale;
		break;
	default:
		width = surface->buffer_ref.buffer->width / vp->buffer.scale;
		height = surface->buffer_ref.buffer->height / vp->buffer.scale;
		break;
	}

	surface->width_from_buffer = width;
	surface->height_from_buffer = height;
}

static void
weston_surface_update_size(struct weston_surface *surface)
{
	struct weston_buffer_viewport *vp = &surface->buffer_viewport;
	int32_t width, height;

	width = surface->width_from_buffer;
	height = surface->height_from_buffer;

	if (width != 0 && vp->surface.width != -1) {
		surface_set_size(surface,
				 vp->surface.width, vp->surface.height);
		return;
	}

	if (width != 0 && vp->buffer.src_width != wl_fixed_from_int(-1)) {
		int32_t w = fixed_round_up_to_int(vp->buffer.src_width);
		int32_t h = fixed_round_up_to_int(vp->buffer.src_height);

		surface_set_size(surface, w ?: 1, h ?: 1);
		return;
	}

	surface_set_size(surface, width, height);
}

WL_EXPORT uint32_t
weston_compositor_get_time(void)
{
       struct timeval tv;

       gettimeofday(&tv, NULL);

       return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

WL_EXPORT struct weston_view *
weston_compositor_pick_view(struct weston_compositor *compositor,
			    wl_fixed_t x, wl_fixed_t y,
			    wl_fixed_t *vx, wl_fixed_t *vy)
{
	struct weston_view *view;
	wl_fixed_t view_x, view_y;
	int view_ix, view_iy;
	int ix = wl_fixed_to_int(x);
	int iy = wl_fixed_to_int(y);

	wl_list_for_each(view, &compositor->view_list, link) {
		if (!pixman_region32_contains_point(
				&view->transform.boundingbox, ix, iy, NULL))
			continue;

		weston_view_from_global_fixed(view, x, y, &view_x, &view_y);
		view_ix = wl_fixed_to_int(view_x);
		view_iy = wl_fixed_to_int(view_y);

		if (!pixman_region32_contains_point(&view->surface->input,
						    view_ix, view_iy, NULL))
			continue;

		if (view->geometry.scissor_enabled &&
		    !pixman_region32_contains_point(&view->geometry.scissor,
						    view_ix, view_iy, NULL))
			continue;

		*vx = view_x;
		*vy = view_y;
		return view;
	}

	*vx = wl_fixed_from_int(-1000000);
	*vy = wl_fixed_from_int(-1000000);
	return NULL;
}

static void
weston_compositor_repick(struct weston_compositor *compositor)
{
	struct weston_seat *seat;

	if (!compositor->session_active)
		return;

	wl_list_for_each(seat, &compositor->seat_list, link)
		weston_seat_repick(seat);
}

WL_EXPORT void
weston_view_unmap(struct weston_view *view)
{
	struct weston_seat *seat;

	if (!weston_view_is_mapped(view))
		return;

	weston_view_damage_below(view);
	view->output = NULL;
	view->plane = NULL;
	weston_layer_entry_remove(&view->layer_link);
	wl_list_remove(&view->link);
	wl_list_init(&view->link);
	view->output_mask = 0;
	weston_surface_assign_output(view->surface);

	if (weston_surface_is_mapped(view->surface))
		return;

	wl_list_for_each(seat, &view->surface->compositor->seat_list, link) {
		struct weston_touch *touch = weston_seat_get_touch(seat);
		struct weston_pointer *pointer = weston_seat_get_pointer(seat);
		struct weston_keyboard *keyboard =
			weston_seat_get_keyboard(seat);

		if (keyboard && keyboard->focus == view->surface)
			weston_keyboard_set_focus(keyboard, NULL);
		if (pointer && pointer->focus == view)
			weston_pointer_clear_focus(pointer);
		if (touch && touch->focus == view)
			weston_touch_set_focus(touch, NULL);
	}
}

WL_EXPORT void
weston_surface_unmap(struct weston_surface *surface)
{
	struct weston_view *view;

	wl_list_for_each(view, &surface->views, surface_link)
		weston_view_unmap(view);
	surface->output = NULL;
}

static void
weston_surface_reset_pending_buffer(struct weston_surface *surface)
{
	weston_surface_state_set_buffer(&surface->pending, NULL);
	surface->pending.sx = 0;
	surface->pending.sy = 0;
	surface->pending.newly_attached = 0;
	surface->pending.buffer_viewport.changed = 0;
}

WL_EXPORT void
weston_view_destroy(struct weston_view *view)
{
	wl_signal_emit(&view->destroy_signal, view);

	assert(wl_list_empty(&view->geometry.child_list));

	if (weston_view_is_mapped(view)) {
		weston_view_unmap(view);
		weston_compositor_build_view_list(view->surface->compositor);
	}

	wl_list_remove(&view->link);
	weston_layer_entry_remove(&view->layer_link);

	pixman_region32_fini(&view->clip);
	pixman_region32_fini(&view->geometry.scissor);
	pixman_region32_fini(&view->transform.boundingbox);
	pixman_region32_fini(&view->transform.opaque);

	weston_view_set_transform_parent(view, NULL);

	wl_list_remove(&view->surface_link);

	free(view);
}

WL_EXPORT void
weston_surface_destroy(struct weston_surface *surface)
{
	struct weston_frame_callback *cb, *next;
	struct weston_view *ev, *nv;

	if (--surface->ref_count > 0)
		return;

	assert(surface->resource == NULL);

	wl_signal_emit(&surface->destroy_signal, surface);

	assert(wl_list_empty(&surface->subsurface_list_pending));
	assert(wl_list_empty(&surface->subsurface_list));

	wl_list_for_each_safe(ev, nv, &surface->views, surface_link)
		weston_view_destroy(ev);

	weston_surface_state_fini(&surface->pending);

	weston_buffer_reference(&surface->buffer_ref, NULL);

	pixman_region32_fini(&surface->damage);
	pixman_region32_fini(&surface->opaque);
	pixman_region32_fini(&surface->input);

	wl_list_for_each_safe(cb, next, &surface->frame_callback_list, link)
		wl_resource_destroy(cb->resource);

	weston_presentation_feedback_discard_list(&surface->feedback_list);

	free(surface);
}

static void
destroy_surface(struct wl_resource *resource)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);

	assert(surface);

	/* Set the resource to NULL, since we don't want to leave a
	 * dangling pointer if the surface was refcounted and survives
	 * the weston_surface_destroy() call. */
	surface->resource = NULL;
	weston_surface_destroy(surface);
}

static void
weston_buffer_destroy_handler(struct wl_listener *listener, void *data)
{
	struct weston_buffer *buffer =
		container_of(listener, struct weston_buffer, destroy_listener);

	wl_signal_emit(&buffer->destroy_signal, buffer);
	free(buffer);
}

WL_EXPORT struct weston_buffer *
weston_buffer_from_resource(struct wl_resource *resource)
{
	struct weston_buffer *buffer;
	struct wl_listener *listener;

	listener = wl_resource_get_destroy_listener(resource,
						    weston_buffer_destroy_handler);

	if (listener)
		return container_of(listener, struct weston_buffer,
				    destroy_listener);

	buffer = zalloc(sizeof *buffer);
	if (buffer == NULL)
		return NULL;

	buffer->resource = resource;
	wl_signal_init(&buffer->destroy_signal);
	buffer->destroy_listener.notify = weston_buffer_destroy_handler;
	buffer->y_inverted = 1;
	wl_resource_add_destroy_listener(resource, &buffer->destroy_listener);

	return buffer;
}

static void
weston_buffer_reference_handle_destroy(struct wl_listener *listener,
				       void *data)
{
	struct weston_buffer_reference *ref =
		container_of(listener, struct weston_buffer_reference,
			     destroy_listener);

	assert((struct weston_buffer *)data == ref->buffer);
	ref->buffer = NULL;
}

WL_EXPORT void
weston_buffer_reference(struct weston_buffer_reference *ref,
			struct weston_buffer *buffer)
{
	if (ref->buffer && buffer != ref->buffer) {
		ref->buffer->busy_count--;
		if (ref->buffer->busy_count == 0) {
			assert(wl_resource_get_client(ref->buffer->resource));
			wl_resource_queue_event(ref->buffer->resource,
						WL_BUFFER_RELEASE);
		}
		wl_list_remove(&ref->destroy_listener.link);
	}

	if (buffer && buffer != ref->buffer) {
		buffer->busy_count++;
		wl_signal_add(&buffer->destroy_signal,
			      &ref->destroy_listener);
	}

	ref->buffer = buffer;
	ref->destroy_listener.notify = weston_buffer_reference_handle_destroy;
}

static void
weston_surface_attach(struct weston_surface *surface,
		      struct weston_buffer *buffer)
{
	weston_buffer_reference(&surface->buffer_ref, buffer);

	if (!buffer) {
		if (weston_surface_is_mapped(surface))
			weston_surface_unmap(surface);
	}

	surface->compositor->renderer->attach(surface, buffer);

	weston_surface_calculate_size_from_buffer(surface);
	weston_presentation_feedback_discard_list(&surface->feedback_list);
}

WL_EXPORT void
weston_compositor_damage_all(struct weston_compositor *compositor)
{
	struct weston_output *output;

	wl_list_for_each(output, &compositor->output_list, link)
		weston_output_damage(output);
}

WL_EXPORT void
weston_output_damage(struct weston_output *output)
{
	struct weston_compositor *compositor = output->compositor;

	pixman_region32_union(&compositor->primary_plane.damage,
			      &compositor->primary_plane.damage,
			      &output->region);
	weston_output_schedule_repaint(output);
}

static void
surface_flush_damage(struct weston_surface *surface)
{
	if (surface->buffer_ref.buffer &&
	    wl_shm_buffer_get(surface->buffer_ref.buffer->resource))
		surface->compositor->renderer->flush_damage(surface);

	if (weston_timeline_enabled_ &&
	    pixman_region32_not_empty(&surface->damage))
		TL_POINT("core_flush_damage", TLP_SURFACE(surface),
			 TLP_OUTPUT(surface->output), TLP_END);

	pixman_region32_clear(&surface->damage);
}

static void
view_accumulate_damage(struct weston_view *view,
		       pixman_region32_t *opaque)
{
	pixman_region32_t damage;

	pixman_region32_init(&damage);
	if (view->transform.enabled) {
		pixman_box32_t *extents;

		extents = pixman_region32_extents(&view->surface->damage);
		view_compute_bbox(view, extents, &damage);
	} else {
		pixman_region32_copy(&damage, &view->surface->damage);
		pixman_region32_translate(&damage,
					  view->geometry.x, view->geometry.y);
	}

	pixman_region32_intersect(&damage, &damage,
				  &view->transform.boundingbox);
	pixman_region32_subtract(&damage, &damage, opaque);
	pixman_region32_union(&view->plane->damage,
			      &view->plane->damage, &damage);
	pixman_region32_fini(&damage);
	pixman_region32_copy(&view->clip, opaque);
	pixman_region32_union(opaque, opaque, &view->transform.opaque);
}

static void
compositor_accumulate_damage(struct weston_compositor *ec)
{
	struct weston_plane *plane;
	struct weston_view *ev;
	pixman_region32_t opaque, clip;

	pixman_region32_init(&clip);

	wl_list_for_each(plane, &ec->plane_list, link) {
		pixman_region32_copy(&plane->clip, &clip);

		pixman_region32_init(&opaque);

		wl_list_for_each(ev, &ec->view_list, link) {
			if (ev->plane != plane)
				continue;

			view_accumulate_damage(ev, &opaque);
		}

		pixman_region32_union(&clip, &clip, &opaque);
		pixman_region32_fini(&opaque);
	}

	pixman_region32_fini(&clip);

	wl_list_for_each(ev, &ec->view_list, link)
		ev->surface->touched = 0;

	wl_list_for_each(ev, &ec->view_list, link) {
		if (ev->surface->touched)
			continue;
		ev->surface->touched = 1;

		surface_flush_damage(ev->surface);

		/* Both the renderer and the backend have seen the buffer
		 * by now. If renderer needs the buffer, it has its own
		 * reference set. If the backend wants to keep the buffer
		 * around for migrating the surface into a non-primary plane
		 * later, keep_buffer is true. Otherwise, drop the core
		 * reference now, and allow early buffer release. This enables
		 * clients to use single-buffering.
		 */
		if (!ev->surface->keep_buffer)
			weston_buffer_reference(&ev->surface->buffer_ref, NULL);
	}
}

static void
surface_stash_subsurface_views(struct weston_surface *surface)
{
	struct weston_subsurface *sub;

	wl_list_for_each(sub, &surface->subsurface_list, parent_link) {
		if (sub->surface == surface)
			continue;

		wl_list_insert_list(&sub->unused_views, &sub->surface->views);
		wl_list_init(&sub->surface->views);

		surface_stash_subsurface_views(sub->surface);
	}
}

static void
surface_free_unused_subsurface_views(struct weston_surface *surface)
{
	struct weston_subsurface *sub;
	struct weston_view *view, *nv;

	wl_list_for_each(sub, &surface->subsurface_list, parent_link) {
		if (sub->surface == surface)
			continue;

		wl_list_for_each_safe(view, nv, &sub->unused_views, surface_link) {
			weston_view_unmap (view);
			weston_view_destroy(view);
		}

		surface_free_unused_subsurface_views(sub->surface);
	}
}

static void
view_list_add_subsurface_view(struct weston_compositor *compositor,
			      struct weston_subsurface *sub,
			      struct weston_view *parent)
{
	struct weston_subsurface *child;
	struct weston_view *view = NULL, *iv;

	if (!weston_surface_is_mapped(sub->surface))
		return;

	wl_list_for_each(iv, &sub->unused_views, surface_link) {
		if (iv->geometry.parent == parent) {
			view = iv;
			break;
		}
	}

	if (view) {
		/* Put it back in the surface's list of views */
		wl_list_remove(&view->surface_link);
		wl_list_insert(&sub->surface->views, &view->surface_link);
	} else {
		view = weston_view_create(sub->surface);
		weston_view_set_position(view,
					 sub->position.x,
					 sub->position.y);
		weston_view_set_transform_parent(view, parent);
	}

	view->parent_view = parent;
	weston_view_update_transform(view);

	if (wl_list_empty(&sub->surface->subsurface_list)) {
		wl_list_insert(compositor->view_list.prev, &view->link);
		return;
	}

	wl_list_for_each(child, &sub->surface->subsurface_list, parent_link) {
		if (child->surface == sub->surface)
			wl_list_insert(compositor->view_list.prev, &view->link);
		else
			view_list_add_subsurface_view(compositor, child, view);
	}
}

static void
view_list_add(struct weston_compositor *compositor,
	      struct weston_view *view)
{
	struct weston_subsurface *sub;

	weston_view_update_transform(view);

	if (wl_list_empty(&view->surface->subsurface_list)) {
		wl_list_insert(compositor->view_list.prev, &view->link);
		return;
	}

	wl_list_for_each(sub, &view->surface->subsurface_list, parent_link) {
		if (sub->surface == view->surface)
			wl_list_insert(compositor->view_list.prev, &view->link);
		else
			view_list_add_subsurface_view(compositor, sub, view);
	}
}

static void
weston_compositor_build_view_list(struct weston_compositor *compositor)
{
	struct weston_view *view;
	struct weston_layer *layer;

	wl_list_for_each(layer, &compositor->layer_list, link)
		wl_list_for_each(view, &layer->view_list.link, layer_link.link)
			surface_stash_subsurface_views(view->surface);

	wl_list_init(&compositor->view_list);
	wl_list_for_each(layer, &compositor->layer_list, link) {
		wl_list_for_each(view, &layer->view_list.link, layer_link.link) {
			view_list_add(compositor, view);
		}
	}

	wl_list_for_each(layer, &compositor->layer_list, link)
		wl_list_for_each(view, &layer->view_list.link, layer_link.link)
			surface_free_unused_subsurface_views(view->surface);
}

static void
weston_output_take_feedback_list(struct weston_output *output,
				 struct weston_surface *surface)
{
	struct weston_view *view;
	struct weston_presentation_feedback *feedback;
	uint32_t flags = 0xffffffff;

	if (wl_list_empty(&surface->feedback_list))
		return;

	/* All views must have the flag for the flag to survive. */
	wl_list_for_each(view, &surface->views, surface_link) {
		/* ignore views that are not on this output at all */
		if (view->output_mask & (1u << output->id))
			flags &= view->psf_flags;
	}

	wl_list_for_each(feedback, &surface->feedback_list, link)
		feedback->psf_flags = flags;

	wl_list_insert_list(&output->feedback_list, &surface->feedback_list);
	wl_list_init(&surface->feedback_list);
}

static int
weston_output_repaint(struct weston_output *output)
{
	struct weston_compositor *ec = output->compositor;
	struct weston_view *ev;
	struct weston_animation *animation, *next;
	struct weston_frame_callback *cb, *cnext;
	struct wl_list frame_callback_list;
	pixman_region32_t output_damage;
	int r;

	if (output->destroying)
		return 0;

	TL_POINT("core_repaint_begin", TLP_OUTPUT(output), TLP_END);

	/* Rebuild the surface list and update surface transforms up front. */
	weston_compositor_build_view_list(ec);

	if (output->assign_planes && !output->disable_planes) {
		output->assign_planes(output);
	} else {
		wl_list_for_each(ev, &ec->view_list, link) {
			weston_view_move_to_plane(ev, &ec->primary_plane);
			ev->psf_flags = 0;
		}
	}

	wl_list_init(&frame_callback_list);
	wl_list_for_each(ev, &ec->view_list, link) {
		/* Note: This operation is safe to do multiple times on the
		 * same surface.
		 */
		if (ev->surface->output == output) {
			wl_list_insert_list(&frame_callback_list,
					    &ev->surface->frame_callback_list);
			wl_list_init(&ev->surface->frame_callback_list);

			weston_output_take_feedback_list(output, ev->surface);
		}
	}

	compositor_accumulate_damage(ec);

	pixman_region32_init(&output_damage);
	pixman_region32_intersect(&output_damage,
				  &ec->primary_plane.damage, &output->region);
	pixman_region32_subtract(&output_damage,
				 &output_damage, &ec->primary_plane.clip);

	if (output->dirty)
		weston_output_update_matrix(output);

	r = output->repaint(output, &output_damage);

	pixman_region32_fini(&output_damage);

	output->repaint_needed = 0;

	weston_compositor_repick(ec);
	wl_event_loop_dispatch(ec->input_loop, 0);

	wl_list_for_each_safe(cb, cnext, &frame_callback_list, link) {
		wl_callback_send_done(cb->resource, output->frame_time);
		wl_resource_destroy(cb->resource);
	}

	wl_list_for_each_safe(animation, next, &output->animation_list, link) {
		animation->frame_counter++;
		animation->frame(animation, output, output->frame_time);
	}

	TL_POINT("core_repaint_posted", TLP_OUTPUT(output), TLP_END);

	return r;
}

static int
weston_compositor_read_input(int fd, uint32_t mask, void *data)
{
	struct weston_compositor *compositor = data;

	wl_event_loop_dispatch(compositor->input_loop, 0);

	return 1;
}

static void
weston_output_schedule_repaint_reset(struct weston_output *output)
{
	struct weston_compositor *compositor = output->compositor;
	struct wl_event_loop *loop;
	int fd;

	output->repaint_scheduled = 0;
	TL_POINT("core_repaint_exit_loop", TLP_OUTPUT(output), TLP_END);

	if (compositor->input_loop_source)
		return;

	loop = wl_display_get_event_loop(compositor->wl_display);
	fd = wl_event_loop_get_fd(compositor->input_loop);
	compositor->input_loop_source =
		wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
				     weston_compositor_read_input, compositor);
}

static int
output_repaint_timer_handler(void *data)
{
	struct weston_output *output = data;
	struct weston_compositor *compositor = output->compositor;

	if (output->repaint_needed &&
	    compositor->state != WESTON_COMPOSITOR_SLEEPING &&
	    compositor->state != WESTON_COMPOSITOR_OFFSCREEN &&
	    weston_output_repaint(output) == 0)
		return 0;

	weston_output_schedule_repaint_reset(output);

	return 0;
}

WL_EXPORT void
weston_output_finish_frame(struct weston_output *output,
			   const struct timespec *stamp,
			   uint32_t presented_flags)
{
	struct weston_compositor *compositor = output->compositor;
	int32_t refresh_nsec;
	struct timespec now;
	struct timespec gone;
	int msec;

	TL_POINT("core_repaint_finished", TLP_OUTPUT(output),
		 TLP_VBLANK(stamp), TLP_END);

	refresh_nsec = millihz_to_nsec(output->current_mode->refresh);
	weston_presentation_feedback_present_list(&output->feedback_list,
						  output, refresh_nsec, stamp,
						  output->msc,
						  presented_flags);

	output->frame_time = stamp->tv_sec * 1000 + stamp->tv_nsec / 1000000;

	weston_compositor_read_presentation_clock(compositor, &now);
	timespec_sub(&gone, &now, stamp);
	msec = (refresh_nsec - timespec_to_nsec(&gone)) / 1000000; /* floor */
	msec -= compositor->repaint_msec;

	if (msec < -1000 || msec > 1000) {
		static bool warned;

		if (!warned)
			weston_log("Warning: computed repaint delay is "
				   "insane: %d msec\n", msec);
		warned = true;

		msec = 0;
	}

	/* Called from restart_repaint_loop and restart happens already after
	 * the deadline given by repaint_msec? In that case we delay until
	 * the deadline of the next frame, to give clients a more predictable
	 * timing of the repaint cycle to lock on. */
	if (presented_flags == PRESENTATION_FEEDBACK_INVALID && msec < 0)
		msec += refresh_nsec / 1000000;

	if (msec < 1)
		output_repaint_timer_handler(output);
	else
		wl_event_source_timer_update(output->repaint_timer, msec);
}

static void
idle_repaint(void *data)
{
	struct weston_output *output = data;

	output->start_repaint_loop(output);
}

WL_EXPORT void
weston_layer_entry_insert(struct weston_layer_entry *list,
			  struct weston_layer_entry *entry)
{
	wl_list_insert(&list->link, &entry->link);
	entry->layer = list->layer;
}

WL_EXPORT void
weston_layer_entry_remove(struct weston_layer_entry *entry)
{
	wl_list_remove(&entry->link);
	wl_list_init(&entry->link);
	entry->layer = NULL;
}

WL_EXPORT void
weston_layer_init(struct weston_layer *layer, struct wl_list *below)
{
	wl_list_init(&layer->view_list.link);
	layer->view_list.layer = layer;
	weston_layer_set_mask_infinite(layer);
	if (below != NULL)
		wl_list_insert(below, &layer->link);
}

WL_EXPORT void
weston_layer_set_mask(struct weston_layer *layer,
		      int x, int y, int width, int height)
{
	struct weston_view *view;

	layer->mask.x1 = x;
	layer->mask.x2 = x + width;
	layer->mask.y1 = y;
	layer->mask.y2 = y + height;

	wl_list_for_each(view, &layer->view_list.link, layer_link.link) {
		weston_view_geometry_dirty(view);
	}
}

WL_EXPORT void
weston_layer_set_mask_infinite(struct weston_layer *layer)
{
	weston_layer_set_mask(layer, INT32_MIN, INT32_MIN,
				     UINT32_MAX, UINT32_MAX);
}

WL_EXPORT void
weston_output_schedule_repaint(struct weston_output *output)
{
	struct weston_compositor *compositor = output->compositor;
	struct wl_event_loop *loop;

	if (compositor->state == WESTON_COMPOSITOR_SLEEPING ||
	    compositor->state == WESTON_COMPOSITOR_OFFSCREEN)
		return;

	if (!output->repaint_needed)
		TL_POINT("core_repaint_req", TLP_OUTPUT(output), TLP_END);

	loop = wl_display_get_event_loop(compositor->wl_display);
	output->repaint_needed = 1;
	if (output->repaint_scheduled)
		return;

	wl_event_loop_add_idle(loop, idle_repaint, output);
	output->repaint_scheduled = 1;
	TL_POINT("core_repaint_enter_loop", TLP_OUTPUT(output), TLP_END);


	if (compositor->input_loop_source) {
		wl_event_source_remove(compositor->input_loop_source);
		compositor->input_loop_source = NULL;
	}
}

WL_EXPORT void
weston_compositor_schedule_repaint(struct weston_compositor *compositor)
{
	struct weston_output *output;

	wl_list_for_each(output, &compositor->output_list, link)
		weston_output_schedule_repaint(output);
}

static void
surface_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_resource *resource,
	       struct wl_resource *buffer_resource, int32_t sx, int32_t sy)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);
	struct weston_buffer *buffer = NULL;

	if (buffer_resource) {
		buffer = weston_buffer_from_resource(buffer_resource);
		if (buffer == NULL) {
			wl_client_post_no_memory(client);
			return;
		}
	}

	/* Attach, attach, without commit in between does not send
	 * wl_buffer.release. */
	weston_surface_state_set_buffer(&surface->pending, buffer);

	surface->pending.sx = sx;
	surface->pending.sy = sy;
	surface->pending.newly_attached = 1;
}

static void
surface_damage(struct wl_client *client,
	       struct wl_resource *resource,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);

	pixman_region32_union_rect(&surface->pending.damage,
				   &surface->pending.damage,
				   x, y, width, height);
}

static void
destroy_frame_callback(struct wl_resource *resource)
{
	struct weston_frame_callback *cb = wl_resource_get_user_data(resource);

	wl_list_remove(&cb->link);
	free(cb);
}

static void
surface_frame(struct wl_client *client,
	      struct wl_resource *resource, uint32_t callback)
{
	struct weston_frame_callback *cb;
	struct weston_surface *surface = wl_resource_get_user_data(resource);

	cb = malloc(sizeof *cb);
	if (cb == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	cb->resource = wl_resource_create(client, &wl_callback_interface, 1,
					  callback);
	if (cb->resource == NULL) {
		free(cb);
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_resource_set_implementation(cb->resource, NULL, cb,
				       destroy_frame_callback);

	wl_list_insert(surface->pending.frame_callback_list.prev, &cb->link);
}

static void
surface_set_opaque_region(struct wl_client *client,
			  struct wl_resource *resource,
			  struct wl_resource *region_resource)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);
	struct weston_region *region;

	if (region_resource) {
		region = wl_resource_get_user_data(region_resource);
		pixman_region32_copy(&surface->pending.opaque,
				     &region->region);
	} else {
		pixman_region32_clear(&surface->pending.opaque);
	}
}

static void
surface_set_input_region(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *region_resource)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);
	struct weston_region *region;

	if (region_resource) {
		region = wl_resource_get_user_data(region_resource);
		pixman_region32_copy(&surface->pending.input,
				     &region->region);
	} else {
		pixman_region32_fini(&surface->pending.input);
		region_init_infinite(&surface->pending.input);
	}
}

static void
weston_surface_commit_subsurface_order(struct weston_surface *surface)
{
	struct weston_subsurface *sub;

	wl_list_for_each_reverse(sub, &surface->subsurface_list_pending,
				 parent_link_pending) {
		wl_list_remove(&sub->parent_link);
		wl_list_insert(&surface->subsurface_list, &sub->parent_link);
	}
}

static void
weston_surface_build_buffer_matrix(struct weston_surface *surface,
				   struct weston_matrix *matrix)
{
	struct weston_buffer_viewport *vp = &surface->buffer_viewport;
	double src_width, src_height, dest_width, dest_height;

	weston_matrix_init(matrix);

	if (vp->buffer.src_width == wl_fixed_from_int(-1)) {
		src_width = surface->width_from_buffer;
		src_height = surface->height_from_buffer;
	} else {
		src_width = wl_fixed_to_double(vp->buffer.src_width);
		src_height = wl_fixed_to_double(vp->buffer.src_height);
	}

	if (vp->surface.width == -1) {
		dest_width = src_width;
		dest_height = src_height;
	} else {
		dest_width = vp->surface.width;
		dest_height = vp->surface.height;
	}

	if (src_width != dest_width || src_height != dest_height)
		weston_matrix_scale(matrix,
				    src_width / dest_width,
				    src_height / dest_height, 1);

	if (vp->buffer.src_width != wl_fixed_from_int(-1))
		weston_matrix_translate(matrix,
					wl_fixed_to_double(vp->buffer.src_x),
					wl_fixed_to_double(vp->buffer.src_y),
					0);

	switch (vp->buffer.transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		weston_matrix_scale(matrix, -1, 1, 1);
		weston_matrix_translate(matrix,
					surface->width_from_buffer, 0, 0);
		break;
	}

	switch (vp->buffer.transform) {
	default:
	case WL_OUTPUT_TRANSFORM_NORMAL:
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		break;
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		weston_matrix_rotate_xy(matrix, 0, 1);
		weston_matrix_translate(matrix,
					surface->height_from_buffer, 0, 0);
		break;
	case WL_OUTPUT_TRANSFORM_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		weston_matrix_rotate_xy(matrix, -1, 0);
		weston_matrix_translate(matrix,
					surface->width_from_buffer,
					surface->height_from_buffer, 0);
		break;
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		weston_matrix_rotate_xy(matrix, 0, -1);
		weston_matrix_translate(matrix,
					0, surface->width_from_buffer, 0);
		break;
	}

	weston_matrix_scale(matrix, vp->buffer.scale, vp->buffer.scale, 1);
}

static void
weston_surface_commit_state(struct weston_surface *surface,
			    struct weston_surface_state *state)
{
	struct weston_view *view;
	pixman_region32_t opaque;

	/* wl_surface.set_buffer_transform */
	/* wl_surface.set_buffer_scale */
	/* wl_viewport.set */
	surface->buffer_viewport = state->buffer_viewport;

	/* wl_surface.attach */
	if (state->newly_attached)
		weston_surface_attach(surface, state->buffer);
	weston_surface_state_set_buffer(state, NULL);

	weston_surface_build_buffer_matrix(surface,
					   &surface->surface_to_buffer_matrix);
	weston_matrix_invert(&surface->buffer_to_surface_matrix,
			     &surface->surface_to_buffer_matrix);

	if (state->newly_attached || state->buffer_viewport.changed) {
		weston_surface_update_size(surface);
		if (surface->configure)
			surface->configure(surface, state->sx, state->sy);
	}

	state->sx = 0;
	state->sy = 0;
	state->newly_attached = 0;
	state->buffer_viewport.changed = 0;

	/* wl_surface.damage */
	if (weston_timeline_enabled_ &&
	    pixman_region32_not_empty(&state->damage))
		TL_POINT("core_commit_damage", TLP_SURFACE(surface), TLP_END);
	pixman_region32_union(&surface->damage, &surface->damage,
			      &state->damage);
	pixman_region32_intersect_rect(&surface->damage, &surface->damage,
				       0, 0, surface->width, surface->height);
	pixman_region32_clear(&state->damage);

	/* wl_surface.set_opaque_region */
	pixman_region32_init(&opaque);
	pixman_region32_intersect_rect(&opaque, &state->opaque,
				       0, 0, surface->width, surface->height);

	if (!pixman_region32_equal(&opaque, &surface->opaque)) {
		pixman_region32_copy(&surface->opaque, &opaque);
		wl_list_for_each(view, &surface->views, surface_link)
			weston_view_geometry_dirty(view);
	}

	pixman_region32_fini(&opaque);

	/* wl_surface.set_input_region */
	pixman_region32_intersect_rect(&surface->input, &state->input,
				       0, 0, surface->width, surface->height);

	/* wl_surface.frame */
	wl_list_insert_list(&surface->frame_callback_list,
			    &state->frame_callback_list);
	wl_list_init(&state->frame_callback_list);

	/* XXX:
	 * What should happen with a feedback request, if there
	 * is no wl_buffer attached for this commit?
	 */

	/* presentation.feedback */
	wl_list_insert_list(&surface->feedback_list,
			    &state->feedback_list);
	wl_list_init(&state->feedback_list);
}

static void
weston_surface_commit(struct weston_surface *surface)
{
	weston_surface_commit_state(surface, &surface->pending);

	weston_surface_commit_subsurface_order(surface);

	weston_surface_schedule_repaint(surface);
}

static void
weston_subsurface_commit(struct weston_subsurface *sub);

static void
weston_subsurface_parent_commit(struct weston_subsurface *sub,
				int parent_is_synchronized);

static void
surface_commit(struct wl_client *client, struct wl_resource *resource)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);
	struct weston_subsurface *sub = weston_surface_to_subsurface(surface);

	if (sub) {
		weston_subsurface_commit(sub);
		return;
	}

	weston_surface_commit(surface);

	wl_list_for_each(sub, &surface->subsurface_list, parent_link) {
		if (sub->surface != surface)
			weston_subsurface_parent_commit(sub, 0);
	}
}

static void
surface_set_buffer_transform(struct wl_client *client,
			     struct wl_resource *resource, int transform)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);

	/* if wl_output.transform grows more members this will need to be updated. */
	if (transform < 0 ||
	    transform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
		wl_resource_post_error(resource,
			WL_SURFACE_ERROR_INVALID_TRANSFORM,
			"buffer transform must be a valid transform "
			"('%d' specified)", transform);
		return;
	}

	surface->pending.buffer_viewport.buffer.transform = transform;
	surface->pending.buffer_viewport.changed = 1;
}

static void
surface_set_buffer_scale(struct wl_client *client,
			 struct wl_resource *resource,
			 int32_t scale)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);

	if (scale < 1) {
		wl_resource_post_error(resource,
			WL_SURFACE_ERROR_INVALID_SCALE,
			"buffer scale must be at least one "
			"('%d' specified)", scale);
		return;
	}

	surface->pending.buffer_viewport.buffer.scale = scale;
	surface->pending.buffer_viewport.changed = 1;
}

static const struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_damage,
	surface_frame,
	surface_set_opaque_region,
	surface_set_input_region,
	surface_commit,
	surface_set_buffer_transform,
	surface_set_buffer_scale
};

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_resource *resource, uint32_t id)
{
	struct weston_compositor *ec = wl_resource_get_user_data(resource);
	struct weston_surface *surface;

	surface = weston_surface_create(ec);
	if (surface == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	surface->resource =
		wl_resource_create(client, &wl_surface_interface,
				   wl_resource_get_version(resource), id);
	if (surface->resource == NULL) {
		weston_surface_destroy(surface);
		wl_resource_post_no_memory(resource);
		return;
	}
	wl_resource_set_implementation(surface->resource, &surface_interface,
				       surface, destroy_surface);

	wl_signal_emit(&ec->create_surface_signal, surface);
}

static void
destroy_region(struct wl_resource *resource)
{
	struct weston_region *region = wl_resource_get_user_data(resource);

	pixman_region32_fini(&region->region);
	free(region);
}

static void
region_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
region_add(struct wl_client *client, struct wl_resource *resource,
	   int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct weston_region *region = wl_resource_get_user_data(resource);

	pixman_region32_union_rect(&region->region, &region->region,
				   x, y, width, height);
}

static void
region_subtract(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct weston_region *region = wl_resource_get_user_data(resource);
	pixman_region32_t rect;

	pixman_region32_init_rect(&rect, x, y, width, height);
	pixman_region32_subtract(&region->region, &region->region, &rect);
	pixman_region32_fini(&rect);
}

static const struct wl_region_interface region_interface = {
	region_destroy,
	region_add,
	region_subtract
};

static void
compositor_create_region(struct wl_client *client,
			 struct wl_resource *resource, uint32_t id)
{
	struct weston_region *region;

	region = malloc(sizeof *region);
	if (region == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	pixman_region32_init(&region->region);

	region->resource =
		wl_resource_create(client, &wl_region_interface, 1, id);
	if (region->resource == NULL) {
		free(region);
		wl_resource_post_no_memory(resource);
		return;
	}
	wl_resource_set_implementation(region->resource, &region_interface,
				       region, destroy_region);
}

static const struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
	compositor_create_region
};

static void
weston_subsurface_commit_from_cache(struct weston_subsurface *sub)
{
	struct weston_surface *surface = sub->surface;

	weston_surface_commit_state(surface, &sub->cached);
	weston_buffer_reference(&sub->cached_buffer_ref, NULL);

	weston_surface_commit_subsurface_order(surface);

	weston_surface_schedule_repaint(surface);

	sub->has_cached_data = 0;
}

static void
weston_subsurface_commit_to_cache(struct weston_subsurface *sub)
{
	struct weston_surface *surface = sub->surface;

	/*
	 * If this commit would cause the surface to move by the
	 * attach(dx, dy) parameters, the old damage region must be
	 * translated to correspond to the new surface coordinate system
	 * original_mode.
	 */
	pixman_region32_translate(&sub->cached.damage,
				  -surface->pending.sx, -surface->pending.sy);
	pixman_region32_union(&sub->cached.damage, &sub->cached.damage,
			      &surface->pending.damage);
	pixman_region32_clear(&surface->pending.damage);

	if (surface->pending.newly_attached) {
		sub->cached.newly_attached = 1;
		weston_surface_state_set_buffer(&sub->cached,
						surface->pending.buffer);
		weston_buffer_reference(&sub->cached_buffer_ref,
					surface->pending.buffer);
		weston_presentation_feedback_discard_list(
					&sub->cached.feedback_list);
	}
	sub->cached.sx += surface->pending.sx;
	sub->cached.sy += surface->pending.sy;

	sub->cached.buffer_viewport.changed |=
		surface->pending.buffer_viewport.changed;
	sub->cached.buffer_viewport.buffer =
		surface->pending.buffer_viewport.buffer;
	sub->cached.buffer_viewport.surface =
		surface->pending.buffer_viewport.surface;

	weston_surface_reset_pending_buffer(surface);

	pixman_region32_copy(&sub->cached.opaque, &surface->pending.opaque);

	pixman_region32_copy(&sub->cached.input, &surface->pending.input);

	wl_list_insert_list(&sub->cached.frame_callback_list,
			    &surface->pending.frame_callback_list);
	wl_list_init(&surface->pending.frame_callback_list);

	wl_list_insert_list(&sub->cached.feedback_list,
			    &surface->pending.feedback_list);
	wl_list_init(&surface->pending.feedback_list);

	sub->has_cached_data = 1;
}

static bool
weston_subsurface_is_synchronized(struct weston_subsurface *sub)
{
	while (sub) {
		if (sub->synchronized)
			return true;

		if (!sub->parent)
			return false;

		sub = weston_surface_to_subsurface(sub->parent);
	}

	return false;
}

static void
weston_subsurface_commit(struct weston_subsurface *sub)
{
	struct weston_surface *surface = sub->surface;
	struct weston_subsurface *tmp;

	/* Recursive check for effectively synchronized. */
	if (weston_subsurface_is_synchronized(sub)) {
		weston_subsurface_commit_to_cache(sub);
	} else {
		if (sub->has_cached_data) {
			/* flush accumulated state from cache */
			weston_subsurface_commit_to_cache(sub);
			weston_subsurface_commit_from_cache(sub);
		} else {
			weston_surface_commit(surface);
		}

		wl_list_for_each(tmp, &surface->subsurface_list, parent_link) {
			if (tmp->surface != surface)
				weston_subsurface_parent_commit(tmp, 0);
		}
	}
}

static void
weston_subsurface_synchronized_commit(struct weston_subsurface *sub)
{
	struct weston_surface *surface = sub->surface;
	struct weston_subsurface *tmp;

	/* From now on, commit_from_cache the whole sub-tree, regardless of
	 * the synchronized mode of each child. This sub-surface or some
	 * of its ancestors were synchronized, so we are synchronized
	 * all the way down.
	 */

	if (sub->has_cached_data)
		weston_subsurface_commit_from_cache(sub);

	wl_list_for_each(tmp, &surface->subsurface_list, parent_link) {
		if (tmp->surface != surface)
			weston_subsurface_parent_commit(tmp, 1);
	}
}

static void
weston_subsurface_parent_commit(struct weston_subsurface *sub,
				int parent_is_synchronized)
{
	struct weston_view *view;
	if (sub->position.set) {
		wl_list_for_each(view, &sub->surface->views, surface_link)
			weston_view_set_position(view,
						 sub->position.x,
						 sub->position.y);

		sub->position.set = 0;
	}

	if (parent_is_synchronized || sub->synchronized)
		weston_subsurface_synchronized_commit(sub);
}

static int
subsurface_get_label(struct weston_surface *surface, char *buf, size_t len)
{
	return snprintf(buf, len, "sub-surface");
}

static void
subsurface_configure(struct weston_surface *surface, int32_t dx, int32_t dy)
{
	struct weston_compositor *compositor = surface->compositor;
	struct weston_view *view;

	wl_list_for_each(view, &surface->views, surface_link)
		weston_view_set_position(view,
					 view->geometry.x + dx,
					 view->geometry.y + dy);

	/* No need to check parent mappedness, because if parent is not
	 * mapped, parent is not in a visible layer, so this sub-surface
	 * will not be drawn either.
	 */
	if (!weston_surface_is_mapped(surface)) {
		struct weston_output *output;

		/* Cannot call weston_view_update_transform(),
		 * because that would call it also for the parent surface,
		 * which might not be mapped yet. That would lead to
		 * inconsistent state, where the window could never be
		 * mapped.
		 *
		 * Instead just assign any output, to make
		 * weston_surface_is_mapped() return true, so that when the
		 * parent surface does get mapped, this one will get
		 * included, too. See view_list_add().
		 */
		assert(!wl_list_empty(&compositor->output_list));
		output = container_of(compositor->output_list.next,
				      struct weston_output, link);

		surface->output = output;
		weston_surface_update_output_mask(surface, 1 << output->id);
	}
}

static struct weston_subsurface *
weston_surface_to_subsurface(struct weston_surface *surface)
{
	if (surface->configure == subsurface_configure)
		return surface->configure_private;

	return NULL;
}

WL_EXPORT struct weston_surface *
weston_surface_get_main_surface(struct weston_surface *surface)
{
	struct weston_subsurface *sub;

	while (surface && (sub = weston_surface_to_subsurface(surface)))
		surface = sub->parent;

	return surface;
}

WL_EXPORT int
weston_surface_set_role(struct weston_surface *surface,
			const char *role_name,
			struct wl_resource *error_resource,
			uint32_t error_code)
{
	assert(role_name);

	if (surface->role_name == NULL ||
	    surface->role_name == role_name ||
	    strcmp(surface->role_name, role_name) == 0) {
		surface->role_name = role_name;

		return 0;
	}

	wl_resource_post_error(error_resource, error_code,
			       "Cannot assign role %s to wl_surface@%d,"
			       " already has role %s\n",
			       role_name,
			       wl_resource_get_id(surface->resource),
			       surface->role_name);
	return -1;
}

WL_EXPORT void
weston_surface_set_label_func(struct weston_surface *surface,
			      int (*desc)(struct weston_surface *,
					  char *, size_t))
{
	surface->get_label = desc;
	surface->timeline.force_refresh = 1;
}

/** Get the size of surface contents
 *
 * \param surface The surface to query.
 * \param width Returns the width of raw contents.
 * \param height Returns the height of raw contents.
 *
 * Retrieves the raw surface content size in pixels for the given surface.
 * This is the whole content size in buffer pixels. If the surface
 * has no content or the renderer does not implement this feature,
 * zeroes are returned.
 *
 * This function is used to determine the buffer size needed for
 * a weston_surface_copy_content() call.
 */
WL_EXPORT void
weston_surface_get_content_size(struct weston_surface *surface,
				int *width, int *height)
{
	struct weston_renderer *rer = surface->compositor->renderer;

	if (!rer->surface_get_content_size) {
		*width = 0;
		*height = 0;
		return;
	}

	rer->surface_get_content_size(surface, width, height);
}

/** Copy surface contents to system memory.
 *
 * \param surface The surface to copy from.
 * \param target Pointer to the target memory buffer.
 * \param size Size of the target buffer in bytes.
 * \param src_x X location on contents to copy from.
 * \param src_y Y location on contents to copy from.
 * \param width Width in pixels of the area to copy.
 * \param height Height in pixels of the area to copy.
 * \return 0 for success, -1 for failure.
 *
 * Surface contents are maintained by the renderer. They can be in a
 * reserved weston_buffer or as a copy, e.g. a GL texture, or something
 * else.
 *
 * Surface contents are copied into memory pointed to by target,
 * which has size bytes of space available. The target memory
 * may be larger than needed, but being smaller returns an error.
 * The extra bytes in target may or may not be written; their content is
 * unspecified. Size must be large enough to hold the image.
 *
 * The image in the target memory will be arranged in rows from
 * top to bottom, and pixels on a row from left to right. The pixel
 * format is PIXMAN_a8b8g8r8, 4 bytes per pixel, and stride is exactly
 * width * 4.
 *
 * Parameters src_x and src_y define the upper-left corner in buffer
 * coordinates (pixels) to copy from. Parameters width and height
 * define the size of the area to copy in pixels.
 *
 * The rectangle defined by src_x, src_y, width, height must fit in
 * the surface contents. Otherwise an error is returned.
 *
 * Use surface_get_data_size to determine the content size; the
 * needed target buffer size and rectangle limits.
 *
 * CURRENT IMPLEMENTATION RESTRICTIONS:
 * - the machine must be little-endian due to Pixman formats.
 *
 * NOTE: Pixman formats are premultiplied.
 */
WL_EXPORT int
weston_surface_copy_content(struct weston_surface *surface,
			    void *target, size_t size,
			    int src_x, int src_y,
			    int width, int height)
{
	struct weston_renderer *rer = surface->compositor->renderer;
	int cw, ch;
	const size_t bytespp = 4; /* PIXMAN_a8b8g8r8 */

	if (!rer->surface_copy_content)
		return -1;

	weston_surface_get_content_size(surface, &cw, &ch);

	if (src_x < 0 || src_y < 0)
		return -1;

	if (width <= 0 || height <= 0)
		return -1;

	if (src_x + width > cw || src_y + height > ch)
		return -1;

	if (width * bytespp * height > size)
		return -1;

	return rer->surface_copy_content(surface, target, size,
					 src_x, src_y, width, height);
}

static void
subsurface_set_position(struct wl_client *client,
			struct wl_resource *resource, int32_t x, int32_t y)
{
	struct weston_subsurface *sub = wl_resource_get_user_data(resource);

	if (!sub)
		return;

	sub->position.x = x;
	sub->position.y = y;
	sub->position.set = 1;
}

static struct weston_subsurface *
subsurface_from_surface(struct weston_surface *surface)
{
	struct weston_subsurface *sub;

	sub = weston_surface_to_subsurface(surface);
	if (sub)
		return sub;

	wl_list_for_each(sub, &surface->subsurface_list, parent_link)
		if (sub->surface == surface)
			return sub;

	return NULL;
}

static struct weston_subsurface *
subsurface_sibling_check(struct weston_subsurface *sub,
			 struct weston_surface *surface,
			 const char *request)
{
	struct weston_subsurface *sibling;

	sibling = subsurface_from_surface(surface);

	if (!sibling) {
		wl_resource_post_error(sub->resource,
			WL_SUBSURFACE_ERROR_BAD_SURFACE,
			"%s: wl_surface@%d is not a parent or sibling",
			request, wl_resource_get_id(surface->resource));
		return NULL;
	}

	if (sibling->parent != sub->parent) {
		wl_resource_post_error(sub->resource,
			WL_SUBSURFACE_ERROR_BAD_SURFACE,
			"%s: wl_surface@%d has a different parent",
			request, wl_resource_get_id(surface->resource));
		return NULL;
	}

	return sibling;
}

static void
subsurface_place_above(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *sibling_resource)
{
	struct weston_subsurface *sub = wl_resource_get_user_data(resource);
	struct weston_surface *surface =
		wl_resource_get_user_data(sibling_resource);
	struct weston_subsurface *sibling;

	if (!sub)
		return;

	sibling = subsurface_sibling_check(sub, surface, "place_above");
	if (!sibling)
		return;

	wl_list_remove(&sub->parent_link_pending);
	wl_list_insert(sibling->parent_link_pending.prev,
		       &sub->parent_link_pending);
}

static void
subsurface_place_below(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *sibling_resource)
{
	struct weston_subsurface *sub = wl_resource_get_user_data(resource);
	struct weston_surface *surface =
		wl_resource_get_user_data(sibling_resource);
	struct weston_subsurface *sibling;

	if (!sub)
		return;

	sibling = subsurface_sibling_check(sub, surface, "place_below");
	if (!sibling)
		return;

	wl_list_remove(&sub->parent_link_pending);
	wl_list_insert(&sibling->parent_link_pending,
		       &sub->parent_link_pending);
}

static void
subsurface_set_sync(struct wl_client *client, struct wl_resource *resource)
{
	struct weston_subsurface *sub = wl_resource_get_user_data(resource);

	if (sub)
		sub->synchronized = 1;
}

static void
subsurface_set_desync(struct wl_client *client, struct wl_resource *resource)
{
	struct weston_subsurface *sub = wl_resource_get_user_data(resource);

	if (sub && sub->synchronized) {
		sub->synchronized = 0;

		/* If sub became effectively desynchronized, flush. */
		if (!weston_subsurface_is_synchronized(sub))
			weston_subsurface_synchronized_commit(sub);
	}
}

static void
weston_subsurface_unlink_parent(struct weston_subsurface *sub)
{
	wl_list_remove(&sub->parent_link);
	wl_list_remove(&sub->parent_link_pending);
	wl_list_remove(&sub->parent_destroy_listener.link);
	sub->parent = NULL;
}

static void
weston_subsurface_destroy(struct weston_subsurface *sub);

static void
subsurface_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct weston_subsurface *sub =
		container_of(listener, struct weston_subsurface,
			     surface_destroy_listener);
	assert(data == sub->surface);

	/* The protocol object (wl_resource) is left inert. */
	if (sub->resource)
		wl_resource_set_user_data(sub->resource, NULL);

	weston_subsurface_destroy(sub);
}

static void
subsurface_handle_parent_destroy(struct wl_listener *listener, void *data)
{
	struct weston_subsurface *sub =
		container_of(listener, struct weston_subsurface,
			     parent_destroy_listener);
	assert(data == sub->parent);
	assert(sub->surface != sub->parent);

	if (weston_surface_is_mapped(sub->surface))
		weston_surface_unmap(sub->surface);

	weston_subsurface_unlink_parent(sub);
}

static void
subsurface_resource_destroy(struct wl_resource *resource)
{
	struct weston_subsurface *sub = wl_resource_get_user_data(resource);

	if (sub)
		weston_subsurface_destroy(sub);
}

static void
subsurface_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
weston_subsurface_link_parent(struct weston_subsurface *sub,
			      struct weston_surface *parent)
{
	sub->parent = parent;
	sub->parent_destroy_listener.notify = subsurface_handle_parent_destroy;
	wl_signal_add(&parent->destroy_signal,
		      &sub->parent_destroy_listener);

	wl_list_insert(&parent->subsurface_list, &sub->parent_link);
	wl_list_insert(&parent->subsurface_list_pending,
		       &sub->parent_link_pending);
}

static void
weston_subsurface_link_surface(struct weston_subsurface *sub,
			       struct weston_surface *surface)
{
	sub->surface = surface;
	sub->surface_destroy_listener.notify =
		subsurface_handle_surface_destroy;
	wl_signal_add(&surface->destroy_signal,
		      &sub->surface_destroy_listener);
}

static void
weston_subsurface_destroy(struct weston_subsurface *sub)
{
	struct weston_view *view, *next;

	assert(sub->surface);

	if (sub->resource) {
		assert(weston_surface_to_subsurface(sub->surface) == sub);
		assert(sub->parent_destroy_listener.notify ==
		       subsurface_handle_parent_destroy);

		wl_list_for_each_safe(view, next, &sub->surface->views, surface_link) {
			weston_view_unmap(view);
			weston_view_destroy(view);
		}

		if (sub->parent)
			weston_subsurface_unlink_parent(sub);

		weston_surface_state_fini(&sub->cached);
		weston_buffer_reference(&sub->cached_buffer_ref, NULL);

		sub->surface->configure = NULL;
		sub->surface->configure_private = NULL;
		weston_surface_set_label_func(sub->surface, NULL);
	} else {
		/* the dummy weston_subsurface for the parent itself */
		assert(sub->parent_destroy_listener.notify == NULL);
		wl_list_remove(&sub->parent_link);
		wl_list_remove(&sub->parent_link_pending);
	}

	wl_list_remove(&sub->surface_destroy_listener.link);
	free(sub);
}

static const struct wl_subsurface_interface subsurface_implementation = {
	subsurface_destroy,
	subsurface_set_position,
	subsurface_place_above,
	subsurface_place_below,
	subsurface_set_sync,
	subsurface_set_desync
};

static struct weston_subsurface *
weston_subsurface_create(uint32_t id, struct weston_surface *surface,
			 struct weston_surface *parent)
{
	struct weston_subsurface *sub;
	struct wl_client *client = wl_resource_get_client(surface->resource);

	sub = zalloc(sizeof *sub);
	if (sub == NULL)
		return NULL;

	wl_list_init(&sub->unused_views);

	sub->resource =
		wl_resource_create(client, &wl_subsurface_interface, 1, id);
	if (!sub->resource) {
		free(sub);
		return NULL;
	}

	wl_resource_set_implementation(sub->resource,
				       &subsurface_implementation,
				       sub, subsurface_resource_destroy);
	weston_subsurface_link_surface(sub, surface);
	weston_subsurface_link_parent(sub, parent);
	weston_surface_state_init(&sub->cached);
	sub->cached_buffer_ref.buffer = NULL;
	sub->synchronized = 1;

	return sub;
}

/* Create a dummy subsurface for having the parent itself in its
 * sub-surface lists. Makes stacking order manipulation easy.
 */
static struct weston_subsurface *
weston_subsurface_create_for_parent(struct weston_surface *parent)
{
	struct weston_subsurface *sub;

	sub = zalloc(sizeof *sub);
	if (sub == NULL)
		return NULL;

	weston_subsurface_link_surface(sub, parent);
	sub->parent = parent;
	wl_list_insert(&parent->subsurface_list, &sub->parent_link);
	wl_list_insert(&parent->subsurface_list_pending,
		       &sub->parent_link_pending);

	return sub;
}

static void
subcompositor_get_subsurface(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t id,
			     struct wl_resource *surface_resource,
			     struct wl_resource *parent_resource)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct weston_surface *parent =
		wl_resource_get_user_data(parent_resource);
	struct weston_subsurface *sub;
	static const char where[] = "get_subsurface: wl_subsurface@";

	if (surface == parent) {
		wl_resource_post_error(resource,
			WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
			"%s%d: wl_surface@%d cannot be its own parent",
			where, id, wl_resource_get_id(surface_resource));
		return;
	}

	if (weston_surface_to_subsurface(surface)) {
		wl_resource_post_error(resource,
			WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
			"%s%d: wl_surface@%d is already a sub-surface",
			where, id, wl_resource_get_id(surface_resource));
		return;
	}

	if (weston_surface_set_role(surface, "wl_subsurface", resource,
				    WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE) < 0)
		return;

	if (weston_surface_get_main_surface(parent) == surface) {
		wl_resource_post_error(resource,
			WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
			"%s%d: wl_surface@%d is an ancestor of parent",
			where, id, wl_resource_get_id(surface_resource));
		return;
	}

	/* make sure the parent is in its own list */
	if (wl_list_empty(&parent->subsurface_list)) {
		if (!weston_subsurface_create_for_parent(parent)) {
			wl_resource_post_no_memory(resource);
			return;
		}
	}

	sub = weston_subsurface_create(id, surface, parent);
	if (!sub) {
		wl_resource_post_no_memory(resource);
		return;
	}

	surface->configure = subsurface_configure;
	surface->configure_private = sub;
	weston_surface_set_label_func(surface, subsurface_get_label);
}

static void
subcompositor_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_subcompositor_interface subcompositor_interface = {
	subcompositor_destroy,
	subcompositor_get_subsurface
};

static void
bind_subcompositor(struct wl_client *client,
		   void *data, uint32_t version, uint32_t id)
{
	struct weston_compositor *compositor = data;
	struct wl_resource *resource;

	resource =
		wl_resource_create(client, &wl_subcompositor_interface, 1, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &subcompositor_interface,
				       compositor, NULL);
}

static void
weston_compositor_dpms(struct weston_compositor *compositor,
		       enum dpms_enum state)
{
        struct weston_output *output;

        wl_list_for_each(output, &compositor->output_list, link)
		if (output->set_dpms)
			output->set_dpms(output, state);
}

WL_EXPORT void
weston_compositor_wake(struct weston_compositor *compositor)
{
	uint32_t old_state = compositor->state;

	/* The state needs to be changed before emitting the wake
	 * signal because that may try to schedule a repaint which
	 * will not work if the compositor is still sleeping */
	compositor->state = WESTON_COMPOSITOR_ACTIVE;

	switch (old_state) {
	case WESTON_COMPOSITOR_SLEEPING:
		weston_compositor_dpms(compositor, WESTON_DPMS_ON);
		/* fall through */
	case WESTON_COMPOSITOR_IDLE:
	case WESTON_COMPOSITOR_OFFSCREEN:
		wl_signal_emit(&compositor->wake_signal, compositor);
		/* fall through */
	default:
		wl_event_source_timer_update(compositor->idle_source,
					     compositor->idle_time * 1000);
	}
}

WL_EXPORT void
weston_compositor_offscreen(struct weston_compositor *compositor)
{
	switch (compositor->state) {
	case WESTON_COMPOSITOR_OFFSCREEN:
		return;
	case WESTON_COMPOSITOR_SLEEPING:
		weston_compositor_dpms(compositor, WESTON_DPMS_ON);
		/* fall through */
	default:
		compositor->state = WESTON_COMPOSITOR_OFFSCREEN;
		wl_event_source_timer_update(compositor->idle_source, 0);
	}
}

WL_EXPORT void
weston_compositor_sleep(struct weston_compositor *compositor)
{
	if (compositor->state == WESTON_COMPOSITOR_SLEEPING)
		return;

	wl_event_source_timer_update(compositor->idle_source, 0);
	compositor->state = WESTON_COMPOSITOR_SLEEPING;
	weston_compositor_dpms(compositor, WESTON_DPMS_OFF);
}

static int
idle_handler(void *data)
{
	struct weston_compositor *compositor = data;

	if (compositor->idle_inhibit)
		return 1;

	compositor->state = WESTON_COMPOSITOR_IDLE;
	wl_signal_emit(&compositor->idle_signal, compositor);

	return 1;
}

WL_EXPORT void
weston_plane_init(struct weston_plane *plane,
			struct weston_compositor *ec,
			int32_t x, int32_t y)
{
	pixman_region32_init(&plane->damage);
	pixman_region32_init(&plane->clip);
	plane->x = x;
	plane->y = y;
	plane->compositor = ec;

	/* Init the link so that the call to wl_list_remove() when releasing
	 * the plane without ever stacking doesn't lead to a crash */
	wl_list_init(&plane->link);
}

WL_EXPORT void
weston_plane_release(struct weston_plane *plane)
{
	struct weston_view *view;

	pixman_region32_fini(&plane->damage);
	pixman_region32_fini(&plane->clip);

	wl_list_for_each(view, &plane->compositor->view_list, link) {
		if (view->plane == plane)
			view->plane = NULL;
	}

	wl_list_remove(&plane->link);
}

WL_EXPORT void
weston_compositor_stack_plane(struct weston_compositor *ec,
			      struct weston_plane *plane,
			      struct weston_plane *above)
{
	if (above)
		wl_list_insert(above->link.prev, &plane->link);
	else
		wl_list_insert(&ec->plane_list, &plane->link);
}

static void unbind_resource(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

static void
bind_output(struct wl_client *client,
	    void *data, uint32_t version, uint32_t id)
{
	struct weston_output *output = data;
	struct weston_mode *mode;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &wl_output_interface,
				      MIN(version, 2), id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_list_insert(&output->resource_list, wl_resource_get_link(resource));
	wl_resource_set_implementation(resource, NULL, data, unbind_resource);

	wl_output_send_geometry(resource,
				output->x,
				output->y,
				output->mm_width,
				output->mm_height,
				output->subpixel,
				output->make, output->model,
				output->transform);
	if (version >= WL_OUTPUT_SCALE_SINCE_VERSION)
		wl_output_send_scale(resource,
				     output->current_scale);

	wl_list_for_each (mode, &output->mode_list, link) {
		wl_output_send_mode(resource,
				    mode->flags,
				    mode->width,
				    mode->height,
				    mode->refresh);
	}

	if (version >= WL_OUTPUT_DONE_SINCE_VERSION)
		wl_output_send_done(resource);
}

/* Move other outputs when one is removed so the space remains contiguos. */
static void
weston_compositor_remove_output(struct weston_compositor *compositor,
				struct weston_output *remove_output)
{
	struct weston_output *output;
	int offset = 0;

	wl_list_for_each(output, &compositor->output_list, link) {
		if (output == remove_output) {
			offset = output->width;
			continue;
		}

		if (offset > 0) {
			weston_output_move(output,
					   output->x - offset, output->y);
			output->dirty = 1;
		}
	}
}

WL_EXPORT void
weston_output_destroy(struct weston_output *output)
{
	struct wl_resource *resource;
	struct weston_view *view;

	output->destroying = 1;

	wl_list_for_each(view, &output->compositor->view_list, link) {
		if (view->output_mask & (1 << output->id))
			weston_view_assign_output(view);
	}

	wl_event_source_remove(output->repaint_timer);

	weston_presentation_feedback_discard_list(&output->feedback_list);

	weston_compositor_remove_output(output->compositor, output);
	wl_list_remove(&output->link);

	wl_signal_emit(&output->compositor->output_destroyed_signal, output);
	wl_signal_emit(&output->destroy_signal, output);

	free(output->name);
	pixman_region32_fini(&output->region);
	pixman_region32_fini(&output->previous_damage);
	output->compositor->output_id_pool &= ~(1 << output->id);

	wl_resource_for_each(resource, &output->resource_list) {
		wl_resource_set_destructor(resource, NULL);
	}

	wl_global_destroy(output->global);
}

WL_EXPORT void
weston_output_update_matrix(struct weston_output *output)
{
	float magnification;

	weston_matrix_init(&output->matrix);
	weston_matrix_translate(&output->matrix, -output->x, -output->y, 0);

	if (output->zoom.active) {
		magnification = 1 / (1 - output->zoom.spring_z.current);
		weston_output_update_zoom(output);
		weston_matrix_translate(&output->matrix, -output->zoom.trans_x,
					-output->zoom.trans_y, 0);
		weston_matrix_scale(&output->matrix, magnification,
				    magnification, 1.0);
	}

	switch (output->transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		weston_matrix_translate(&output->matrix, -output->width, 0, 0);
		weston_matrix_scale(&output->matrix, -1, 1, 1);
		break;
	}

	switch (output->transform) {
	default:
	case WL_OUTPUT_TRANSFORM_NORMAL:
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		break;
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		weston_matrix_translate(&output->matrix, 0, -output->height, 0);
		weston_matrix_rotate_xy(&output->matrix, 0, 1);
		break;
	case WL_OUTPUT_TRANSFORM_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		weston_matrix_translate(&output->matrix,
					-output->width, -output->height, 0);
		weston_matrix_rotate_xy(&output->matrix, -1, 0);
		break;
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		weston_matrix_translate(&output->matrix, -output->width, 0, 0);
		weston_matrix_rotate_xy(&output->matrix, 0, -1);
		break;
	}

	if (output->current_scale != 1)
		weston_matrix_scale(&output->matrix,
				    output->current_scale,
				    output->current_scale, 1);

	output->dirty = 0;

	weston_matrix_invert(&output->inverse_matrix, &output->matrix);
}

static void
weston_output_transform_scale_init(struct weston_output *output, uint32_t transform, uint32_t scale)
{
	output->transform = transform;

	switch (transform) {
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		/* Swap width and height */
		output->width = output->current_mode->height;
		output->height = output->current_mode->width;
		break;
	case WL_OUTPUT_TRANSFORM_NORMAL:
	case WL_OUTPUT_TRANSFORM_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		output->width = output->current_mode->width;
		output->height = output->current_mode->height;
		break;
	default:
		break;
	}

	output->native_scale = output->current_scale = scale;
	output->width /= scale;
	output->height /= scale;
}

static void
weston_output_init_geometry(struct weston_output *output, int x, int y)
{
	output->x = x;
	output->y = y;

	pixman_region32_init(&output->previous_damage);
	pixman_region32_init_rect(&output->region, x, y,
				  output->width,
				  output->height);
}

WL_EXPORT void
weston_output_move(struct weston_output *output, int x, int y)
{
	struct wl_resource *resource;

	output->move_x = x - output->x;
	output->move_y = y - output->y;

	if (output->move_x == 0 && output->move_y == 0)
		return;

	weston_output_init_geometry(output, x, y);

	output->dirty = 1;

	/* Move views on this output. */
	wl_signal_emit(&output->compositor->output_moved_signal, output);

	/* Notify clients of the change for output position. */
	wl_resource_for_each(resource, &output->resource_list) {
		wl_output_send_geometry(resource,
					output->x,
					output->y,
					output->mm_width,
					output->mm_height,
					output->subpixel,
					output->make,
					output->model,
					output->transform);

		if (wl_resource_get_version(resource) >= 2)
			wl_output_send_done(resource);
	}
}

WL_EXPORT void
weston_output_init(struct weston_output *output, struct weston_compositor *c,
		   int x, int y, int mm_width, int mm_height, uint32_t transform,
		   int32_t scale)
{
	struct wl_event_loop *loop;

	output->compositor = c;
	output->x = x;
	output->y = y;
	output->mm_width = mm_width;
	output->mm_height = mm_height;
	output->dirty = 1;
	output->original_scale = scale;

	weston_output_transform_scale_init(output, transform, scale);
	weston_output_init_zoom(output);

	weston_output_init_geometry(output, x, y);
	weston_output_damage(output);

	wl_signal_init(&output->frame_signal);
	wl_signal_init(&output->destroy_signal);
	wl_list_init(&output->animation_list);
	wl_list_init(&output->resource_list);
	wl_list_init(&output->feedback_list);
	wl_list_init(&output->link);

	loop = wl_display_get_event_loop(c->wl_display);
	output->repaint_timer = wl_event_loop_add_timer(loop,
					output_repaint_timer_handler, output);

	output->id = ffs(~output->compositor->output_id_pool) - 1;
	output->compositor->output_id_pool |= 1 << output->id;

	output->global =
		wl_global_create(c->wl_display, &wl_output_interface, 2,
				 output, bind_output);
}

/** Adds an output to the compositor's output list and
 *  send the compositor's output_created signal.
 *
 * \param compositor The compositor instance.
 * \param output The output to be added.
 */
WL_EXPORT void
weston_compositor_add_output(struct weston_compositor *compositor,
                             struct weston_output *output)
{
	wl_list_insert(compositor->output_list.prev, &output->link);
	wl_signal_emit(&compositor->output_created_signal, output);
}

WL_EXPORT void
weston_output_transform_coordinate(struct weston_output *output,
				   wl_fixed_t device_x, wl_fixed_t device_y,
				   wl_fixed_t *x, wl_fixed_t *y)
{
	struct weston_vector p = { {
		wl_fixed_to_double(device_x),
		wl_fixed_to_double(device_y),
		0.0,
		1.0 } };

	weston_matrix_transform(&output->inverse_matrix, &p);

	*x = wl_fixed_from_double(p.f[0] / p.f[3]);
	*y = wl_fixed_from_double(p.f[1] / p.f[3]);
}

static void
destroy_viewport(struct wl_resource *resource)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(resource);

	surface->viewport_resource = NULL;
	surface->pending.buffer_viewport.buffer.src_width =
		wl_fixed_from_int(-1);
	surface->pending.buffer_viewport.surface.width = -1;
	surface->pending.buffer_viewport.changed = 1;
}

static void
viewport_destroy(struct wl_client *client,
		 struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
viewport_set(struct wl_client *client,
	     struct wl_resource *resource,
	     wl_fixed_t src_x,
	     wl_fixed_t src_y,
	     wl_fixed_t src_width,
	     wl_fixed_t src_height,
	     int32_t dst_width,
	     int32_t dst_height)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(resource);

	assert(surface->viewport_resource != NULL);

	if (wl_fixed_to_double(src_width) < 0 ||
	    wl_fixed_to_double(src_height) < 0) {
		wl_resource_post_error(resource,
			WL_VIEWPORT_ERROR_BAD_VALUE,
			"source dimensions must be non-negative (%fx%f)",
			wl_fixed_to_double(src_width),
			wl_fixed_to_double(src_height));
		return;
	}

	if (dst_width <= 0 || dst_height <= 0) {
		wl_resource_post_error(resource,
			WL_VIEWPORT_ERROR_BAD_VALUE,
			"destination dimensions must be positive (%dx%d)",
			dst_width, dst_height);
		return;
	}

	surface->pending.buffer_viewport.buffer.src_x = src_x;
	surface->pending.buffer_viewport.buffer.src_y = src_y;
	surface->pending.buffer_viewport.buffer.src_width = src_width;
	surface->pending.buffer_viewport.buffer.src_height = src_height;
	surface->pending.buffer_viewport.surface.width = dst_width;
	surface->pending.buffer_viewport.surface.height = dst_height;
	surface->pending.buffer_viewport.changed = 1;
}

static void
viewport_set_source(struct wl_client *client,
		    struct wl_resource *resource,
		    wl_fixed_t src_x,
		    wl_fixed_t src_y,
		    wl_fixed_t src_width,
		    wl_fixed_t src_height)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(resource);

	assert(surface->viewport_resource != NULL);

	if (src_width == wl_fixed_from_int(-1) &&
	    src_height == wl_fixed_from_int(-1)) {
		/* unset source size */
		surface->pending.buffer_viewport.buffer.src_width =
			wl_fixed_from_int(-1);
		surface->pending.buffer_viewport.changed = 1;
		return;
	}

	if (src_width <= 0 || src_height <= 0) {
		wl_resource_post_error(resource,
			WL_VIEWPORT_ERROR_BAD_VALUE,
			"source size must be positive (%fx%f)",
			wl_fixed_to_double(src_width),
			wl_fixed_to_double(src_height));
		return;
	}

	surface->pending.buffer_viewport.buffer.src_x = src_x;
	surface->pending.buffer_viewport.buffer.src_y = src_y;
	surface->pending.buffer_viewport.buffer.src_width = src_width;
	surface->pending.buffer_viewport.buffer.src_height = src_height;
	surface->pending.buffer_viewport.changed = 1;
}

static void
viewport_set_destination(struct wl_client *client,
			 struct wl_resource *resource,
			 int32_t dst_width,
			 int32_t dst_height)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(resource);

	assert(surface->viewport_resource != NULL);

	if (dst_width == -1 && dst_height == -1) {
		/* unset destination size */
		surface->pending.buffer_viewport.surface.width = -1;
		surface->pending.buffer_viewport.changed = 1;
		return;
	}

	if (dst_width <= 0 || dst_height <= 0) {
		wl_resource_post_error(resource,
			WL_VIEWPORT_ERROR_BAD_VALUE,
			"destination size must be positive (%dx%d)",
			dst_width, dst_height);
		return;
	}

	surface->pending.buffer_viewport.surface.width = dst_width;
	surface->pending.buffer_viewport.surface.height = dst_height;
	surface->pending.buffer_viewport.changed = 1;
}

static const struct wl_viewport_interface viewport_interface = {
	viewport_destroy,
	viewport_set,
	viewport_set_source,
	viewport_set_destination
};

static void
scaler_destroy(struct wl_client *client,
	       struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
scaler_get_viewport(struct wl_client *client,
		    struct wl_resource *scaler,
		    uint32_t id,
		    struct wl_resource *surface_resource)
{
	int version = wl_resource_get_version(scaler);
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct wl_resource *resource;

	if (surface->viewport_resource) {
		wl_resource_post_error(scaler,
			WL_SCALER_ERROR_VIEWPORT_EXISTS,
			"a viewport for that surface already exists");
		return;
	}

	resource = wl_resource_create(client, &wl_viewport_interface,
				      version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &viewport_interface,
				       surface, destroy_viewport);

	surface->viewport_resource = resource;
}

static const struct wl_scaler_interface scaler_interface = {
	scaler_destroy,
	scaler_get_viewport
};

static void
bind_scaler(struct wl_client *client,
	    void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &wl_scaler_interface,
				      MIN(version, 2), id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &scaler_interface,
				       NULL, NULL);
}

static void
destroy_presentation_feedback(struct wl_resource *feedback_resource)
{
	struct weston_presentation_feedback *feedback;

	feedback = wl_resource_get_user_data(feedback_resource);

	wl_list_remove(&feedback->link);
	free(feedback);
}

static void
presentation_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
presentation_feedback(struct wl_client *client,
		      struct wl_resource *presentation_resource,
		      struct wl_resource *surface_resource,
		      uint32_t callback)
{
	struct weston_surface *surface;
	struct weston_presentation_feedback *feedback;

	surface = wl_resource_get_user_data(surface_resource);

	feedback = zalloc(sizeof *feedback);
	if (feedback == NULL)
		goto err_calloc;

	feedback->resource = wl_resource_create(client,
					&presentation_feedback_interface,
					1, callback);
	if (!feedback->resource)
		goto err_create;

	wl_resource_set_implementation(feedback->resource, NULL, feedback,
				       destroy_presentation_feedback);

	wl_list_insert(&surface->pending.feedback_list, &feedback->link);

	return;

err_create:
	free(feedback);

err_calloc:
	wl_client_post_no_memory(client);
}

static const struct presentation_interface presentation_implementation = {
	presentation_destroy,
	presentation_feedback
};

static void
bind_presentation(struct wl_client *client,
		  void *data, uint32_t version, uint32_t id)
{
	struct weston_compositor *compositor = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &presentation_interface,
				      MIN(version, 1), id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &presentation_implementation,
				       compositor, NULL);
	presentation_send_clock_id(resource, compositor->presentation_clock);
}

static void
compositor_bind(struct wl_client *client,
		void *data, uint32_t version, uint32_t id)
{
	struct weston_compositor *compositor = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &wl_compositor_interface,
				      MIN(version, 3), id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &compositor_interface,
				       compositor, NULL);
}

WL_EXPORT int
weston_environment_get_fd(const char *env)
{
	char *e, *end;
	int fd, flags;

	e = getenv(env);
	if (!e)
		return -1;
	fd = strtol(e, &end, 0);
	if (*end != '\0')
		return -1;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		return -1;

	fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	unsetenv(env);

	return fd;
}

static void
timeline_key_binding_handler(struct weston_keyboard *keyboard, uint32_t time,
			     uint32_t key, void *data)
{
	struct weston_compositor *compositor = data;

	if (weston_timeline_enabled_)
		weston_timeline_close();
	else
		weston_timeline_open(compositor);
}

/** Create the compositor.
 *
 * This functions creates and initializes a compositor instance.
 *
 * \param display The Wayland display to be used.
 * \param user_data A pointer to an object that can later be retrieved
 * using the \ref weston_compositor_get_user_data function.
 * \return The compositor instance on success or NULL on failure.
 */
WL_EXPORT struct weston_compositor *
weston_compositor_create(struct wl_display *display, void *user_data)
{
	struct weston_compositor *ec;
	struct wl_event_loop *loop;

	ec = zalloc(sizeof *ec);
	if (!ec)
		return NULL;

	ec->wl_display = display;
	ec->user_data = user_data;
	wl_signal_init(&ec->destroy_signal);
	wl_signal_init(&ec->create_surface_signal);
	wl_signal_init(&ec->activate_signal);
	wl_signal_init(&ec->transform_signal);
	wl_signal_init(&ec->kill_signal);
	wl_signal_init(&ec->idle_signal);
	wl_signal_init(&ec->wake_signal);
	wl_signal_init(&ec->show_input_panel_signal);
	wl_signal_init(&ec->hide_input_panel_signal);
	wl_signal_init(&ec->update_input_panel_signal);
	wl_signal_init(&ec->seat_created_signal);
	wl_signal_init(&ec->output_created_signal);
	wl_signal_init(&ec->output_destroyed_signal);
	wl_signal_init(&ec->output_moved_signal);
	wl_signal_init(&ec->session_signal);
	ec->session_active = 1;

	ec->output_id_pool = 0;
	ec->repaint_msec = DEFAULT_REPAINT_WINDOW;

	if (!wl_global_create(ec->wl_display, &wl_compositor_interface, 3,
			      ec, compositor_bind))
		goto fail;

	if (!wl_global_create(ec->wl_display, &wl_subcompositor_interface, 1,
			      ec, bind_subcompositor))
		goto fail;

	if (!wl_global_create(ec->wl_display, &wl_scaler_interface, 2,
			      ec, bind_scaler))
		goto fail;

	if (!wl_global_create(ec->wl_display, &presentation_interface, 1,
			      ec, bind_presentation))
		goto fail;

	wl_list_init(&ec->view_list);
	wl_list_init(&ec->plane_list);
	wl_list_init(&ec->layer_list);
	wl_list_init(&ec->seat_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->key_binding_list);
	wl_list_init(&ec->modifier_binding_list);
	wl_list_init(&ec->button_binding_list);
	wl_list_init(&ec->touch_binding_list);
	wl_list_init(&ec->axis_binding_list);
	wl_list_init(&ec->debug_binding_list);

	weston_plane_init(&ec->primary_plane, ec, 0, 0);
	weston_compositor_stack_plane(ec, &ec->primary_plane, NULL);

	wl_data_device_manager_init(ec->wl_display);

	wl_display_init_shm(ec->wl_display);

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->idle_source = wl_event_loop_add_timer(loop, idle_handler, ec);
	wl_event_source_timer_update(ec->idle_source, ec->idle_time * 1000);

	ec->input_loop = wl_event_loop_create();

	weston_layer_init(&ec->fade_layer, &ec->layer_list);
	weston_layer_init(&ec->cursor_layer, &ec->fade_layer.link);

	weston_compositor_add_debug_binding(ec, KEY_T,
					    timeline_key_binding_handler, ec);

	return ec;

fail:
	free(ec);
	return NULL;
}

WL_EXPORT void
weston_compositor_shutdown(struct weston_compositor *ec)
{
	struct weston_output *output, *next;

	wl_event_source_remove(ec->idle_source);
	if (ec->input_loop_source)
		wl_event_source_remove(ec->input_loop_source);

	/* Destroy all outputs associated with this compositor */
	wl_list_for_each_safe(output, next, &ec->output_list, link)
		output->destroy(output);

	if (ec->renderer)
		ec->renderer->destroy(ec);

	weston_binding_list_destroy_all(&ec->key_binding_list);
	weston_binding_list_destroy_all(&ec->modifier_binding_list);
	weston_binding_list_destroy_all(&ec->button_binding_list);
	weston_binding_list_destroy_all(&ec->touch_binding_list);
	weston_binding_list_destroy_all(&ec->axis_binding_list);
	weston_binding_list_destroy_all(&ec->debug_binding_list);

	weston_plane_release(&ec->primary_plane);

	wl_event_loop_destroy(ec->input_loop);
}

WL_EXPORT void
weston_compositor_exit_with_code(struct weston_compositor *compositor,
				 int exit_code)
{
	if (compositor->exit_code == EXIT_SUCCESS)
		compositor->exit_code = exit_code;

	weston_compositor_exit(compositor);
}

WL_EXPORT void
weston_compositor_set_default_pointer_grab(struct weston_compositor *ec,
			const struct weston_pointer_grab_interface *interface)
{
	struct weston_seat *seat;

	ec->default_pointer_grab = interface;
	wl_list_for_each(seat, &ec->seat_list, link) {
		struct weston_pointer *pointer = weston_seat_get_pointer(seat);

		if (pointer)
			weston_pointer_set_default_grab(pointer, interface);
	}
}

WL_EXPORT int
weston_compositor_set_presentation_clock(struct weston_compositor *compositor,
					 clockid_t clk_id)
{
	struct timespec ts;

	if (clock_gettime(clk_id, &ts) < 0)
		return -1;

	compositor->presentation_clock = clk_id;

	return 0;
}

/*
 * For choosing the software clock, when the display hardware or API
 * does not expose a compatible presentation timestamp.
 */
WL_EXPORT int
weston_compositor_set_presentation_clock_software(
					struct weston_compositor *compositor)
{
	/* In order of preference */
	static const clockid_t clocks[] = {
		CLOCK_MONOTONIC_RAW,	/* no jumps, no crawling */
		CLOCK_MONOTONIC_COARSE,	/* no jumps, may crawl, fast & coarse */
		CLOCK_MONOTONIC,	/* no jumps, may crawl */
		CLOCK_REALTIME_COARSE,	/* may jump and crawl, fast & coarse */
		CLOCK_REALTIME		/* may jump and crawl */
	};
	unsigned i;

	for (i = 0; i < ARRAY_LENGTH(clocks); i++)
		if (weston_compositor_set_presentation_clock(compositor,
							     clocks[i]) == 0)
			return 0;

	weston_log("Error: no suitable presentation clock available.\n");

	return -1;
}

/** Read the current time from the Presentation clock
 *
 * \param compositor
 * \param ts[out] The current time.
 *
 * \note Reading the current time in user space is always imprecise to some
 * degree.
 *
 * This function is never meant to fail. If reading the clock does fail,
 * an error message is logged and a zero time is returned. Callers are not
 * supposed to detect or react to failures.
 */
WL_EXPORT void
weston_compositor_read_presentation_clock(
			const struct weston_compositor *compositor,
			struct timespec *ts)
{
	static bool warned;
	int ret;

	ret = clock_gettime(compositor->presentation_clock, ts);
	if (ret < 0) {
		ts->tv_sec = 0;
		ts->tv_nsec = 0;

		if (!warned)
			weston_log("Error: failure to read "
				   "the presentation clock %#x: '%m' (%d)\n",
				   compositor->presentation_clock, errno);
		warned = true;
	}
}

/** Import dmabuf buffer into current renderer
 *
 * \param compositor
 * \param buffer the dmabuf buffer to import
 * \return true on usable buffers, false otherwise
 *
 * This function tests that the linux_dmabuf_buffer is usable
 * for the current renderer. Returns false on unusable buffers. Usually
 * usability is tested by importing the dmabufs for composition.
 *
 * This hook is also used for detecting if the renderer supports
 * dmabufs at all. If the renderer hook is NULL, dmabufs are not
 * supported.
 * */
WL_EXPORT bool
weston_compositor_import_dmabuf(struct weston_compositor *compositor,
				struct linux_dmabuf_buffer *buffer)
{
	struct weston_renderer *renderer;

	renderer = compositor->renderer;

	if (renderer->import_dmabuf == NULL)
		return false;

	return renderer->import_dmabuf(compositor, buffer);
}

WL_EXPORT void
weston_version(int *major, int *minor, int *micro)
{
	*major = WESTON_VERSION_MAJOR;
	*minor = WESTON_VERSION_MINOR;
	*micro = WESTON_VERSION_MICRO;
}

WL_EXPORT void *
weston_load_module(const char *name, const char *entrypoint)
{
	const char *builddir = getenv("WESTON_BUILD_DIR");
	char path[PATH_MAX];
	void *module, *init;

	if (name == NULL)
		return NULL;

	if (name[0] != '/') {
		if (builddir)
			snprintf(path, sizeof path, "%s/.libs/%s", builddir, name);
		else
			snprintf(path, sizeof path, "%s/%s", MODULEDIR, name);
	} else {
		snprintf(path, sizeof path, "%s", name);
	}

	module = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
	if (module) {
		weston_log("Module '%s' already loaded\n", path);
		dlclose(module);
		return NULL;
	}

	weston_log("Loading module '%s'\n", path);
	module = dlopen(path, RTLD_NOW);
	if (!module) {
		weston_log("Failed to load module: %s\n", dlerror());
		return NULL;
	}

	init = dlsym(module, entrypoint);
	if (!init) {
		weston_log("Failed to lookup init function: %s\n", dlerror());
		dlclose(module);
		return NULL;
	}

	return init;
}


/** Destroys the compositor.
 *
 * This function cleans up the compositor state and destroys it.
 *
 * \param compositor The compositor to be destroyed.
 */
WL_EXPORT void
weston_compositor_destroy(struct weston_compositor *compositor)
{
	/* prevent further rendering while shutting down */
	compositor->state = WESTON_COMPOSITOR_OFFSCREEN;

	wl_signal_emit(&compositor->destroy_signal, compositor);

	weston_compositor_xkb_destroy(compositor);

	compositor->backend->destroy(compositor);
	free(compositor);
}

/** Instruct the compositor to exit.
 *
 * This functions does not directly destroy the compositor object, it merely
 * command it to start the tear down process. It is not guaranteed that the
 * tear down will happen immediately.
 *
 * \param compositor The compositor to tear down.
 */
WL_EXPORT void
weston_compositor_exit(struct weston_compositor *compositor)
{
	compositor->exit(compositor);
}

/** Return the user data stored in the compositor.
 *
 * This function returns the user data pointer set with user_data parameter
 * to the \ref weston_compositor_create function.
 */
WL_EXPORT void *
weston_compositor_get_user_data(struct weston_compositor *compositor)
{
	return compositor->user_data;
}
