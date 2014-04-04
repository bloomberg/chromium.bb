/*
 * Copyright © 2010-2011 Intel Corporation
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2012 Collabora, Ltd.
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

#ifdef HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include "compositor.h"
#include "scaler-server-protocol.h"
#include "../shared/os-compatibility.h"
#include "git-version.h"
#include "version.h"

static struct wl_list child_process_list;
static struct weston_compositor *segv_compositor;

static int
sigchld_handler(int signal_number, void *data)
{
	struct weston_process *p;
	int status;
	pid_t pid;

	pid = waitpid(-1, &status, WNOHANG);
	if (!pid)
		return 1;

	wl_list_for_each(p, &child_process_list, link) {
		if (p->pid == pid)
			break;
	}

	if (&p->link == &child_process_list) {
		weston_log("unknown child process exited\n");
		return 1;
	}

	wl_list_remove(&p->link);
	p->cleanup(p, status);

	return 1;
}

static void
weston_output_transform_scale_init(struct weston_output *output,
				   uint32_t transform, uint32_t scale);

static void
weston_compositor_build_view_list(struct weston_compositor *compositor);

WL_EXPORT int
weston_output_switch_mode(struct weston_output *output, struct weston_mode *mode,
		int32_t scale, enum weston_mode_switch_op op)
{
	struct weston_seat *seat;
	struct wl_resource *resource;
	pixman_region32_t old_output_region;
	int ret, notify_mode_changed, notify_scale_changed;
	int temporary_mode, temporary_scale;

	if (!output->switch_mode)
		return -1;

	temporary_mode = (output->original_mode != 0);
	temporary_scale = (output->current_scale != output->original_scale);
	ret = 0;

	notify_mode_changed = 0;
	notify_scale_changed = 0;
	switch(op) {
	case WESTON_MODE_SWITCH_SET_NATIVE:
		output->native_mode = mode;
		if (!temporary_mode) {
			notify_mode_changed = 1;
			ret = output->switch_mode(output, mode);
			if (ret < 0)
				return ret;
		}

		output->native_scale = scale;
		if(!temporary_scale)
			notify_scale_changed = 1;
		break;
	case WESTON_MODE_SWITCH_SET_TEMPORARY:
		if (!temporary_mode)
			output->original_mode = output->native_mode;
		if (!temporary_scale)
			output->original_scale = output->native_scale;

		ret = output->switch_mode(output, mode);
		if (ret < 0)
			return ret;

		output->current_scale = scale;
		break;
	case WESTON_MODE_SWITCH_RESTORE_NATIVE:
		if (!temporary_mode) {
			weston_log("already in the native mode\n");
			return -1;
		}

		notify_mode_changed = (output->original_mode != output->native_mode);

		ret = output->switch_mode(output, mode);
		if (ret < 0)
			return ret;

		if (output->original_scale != output->native_scale) {
			notify_scale_changed = 1;
			scale = output->native_scale;
			output->original_scale = scale;
		}
		output->original_mode = 0;

		output->current_scale = output->native_scale;
		break;
	default:
		weston_log("unknown weston_switch_mode_op %d\n", op);
		break;
	}

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
		struct weston_pointer *pointer = seat->pointer;
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

	/* notify clients of the changes */
	if (notify_mode_changed || notify_scale_changed) {
		wl_resource_for_each(resource, &output->resource_list) {
			if(notify_mode_changed) {
				wl_output_send_mode(resource,
						mode->flags | WL_OUTPUT_MODE_CURRENT,
						mode->width,
						mode->height,
						mode->refresh);
			}

			if (notify_scale_changed)
				wl_output_send_scale(resource, scale);

			if (wl_resource_get_version(resource) >= 2)
				   wl_output_send_done(resource);
		}
	}

	return ret;
}

WL_EXPORT void
weston_watch_process(struct weston_process *process)
{
	wl_list_insert(&child_process_list, &process->link);
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

static void
surface_handle_pending_buffer_destroy(struct wl_listener *listener, void *data)
{
	struct weston_surface *surface =
		container_of(listener, struct weston_surface,
			     pending.buffer_destroy_listener);

	surface->pending.buffer = NULL;
}

static void
empty_region(pixman_region32_t *region)
{
	pixman_region32_fini(region);
	pixman_region32_init(region);
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

	view = calloc(1, sizeof *view);
	if (view == NULL)
		return NULL;

	view->surface = surface;

	/* Assign to surface */
	wl_list_insert(&surface->views, &view->surface_link);

	wl_signal_init(&view->destroy_signal);
	wl_list_init(&view->link);
	wl_list_init(&view->layer_link);

	view->plane = NULL;

	pixman_region32_init(&view->clip);

	view->alpha = 1.0;
	pixman_region32_init(&view->transform.opaque);

	wl_list_init(&view->geometry.transformation_list);
	wl_list_insert(&view->geometry.transformation_list,
		       &view->transform.position.link);
	weston_matrix_init(&view->transform.position.matrix);
	wl_list_init(&view->geometry.child_list);
	pixman_region32_init(&view->transform.boundingbox);
	view->transform.dirty = 1;

	view->output = NULL;

	return view;
}

WL_EXPORT struct weston_surface *
weston_surface_create(struct weston_compositor *compositor)
{
	struct weston_surface *surface;

	surface = calloc(1, sizeof *surface);
	if (surface == NULL)
		return NULL;

	wl_signal_init(&surface->destroy_signal);

	surface->resource = NULL;

	surface->compositor = compositor;
	surface->ref_count = 1;

	surface->buffer_viewport.buffer.transform = WL_OUTPUT_TRANSFORM_NORMAL;
	surface->buffer_viewport.buffer.scale = 1;
	surface->buffer_viewport.buffer.src_width = wl_fixed_from_int(-1);
	surface->buffer_viewport.surface.width = -1;
	surface->pending.buffer_viewport = surface->buffer_viewport;
	surface->output = NULL;
	surface->pending.newly_attached = 0;

	surface->viewport_resource = NULL;

	pixman_region32_init(&surface->damage);
	pixman_region32_init(&surface->opaque);
	region_init_infinite(&surface->input);

	wl_list_init(&surface->views);

	wl_list_init(&surface->frame_callback_list);

	surface->pending.buffer_destroy_listener.notify =
		surface_handle_pending_buffer_destroy;
	pixman_region32_init(&surface->pending.damage);
	pixman_region32_init(&surface->pending.opaque);
	region_init_infinite(&surface->pending.input);
	wl_list_init(&surface->pending.frame_callback_list);

	wl_list_init(&surface->subsurface_list);
	wl_list_init(&surface->subsurface_list_pending);

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

	weston_transformed_coord(surface->width, surface->height,
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

	return weston_transformed_rect(surface->width, surface->height,
				       vp->buffer.transform, vp->buffer.scale,
				       rect);
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
view_compute_bbox(struct weston_view *view, int32_t sx, int32_t sy,
		  int32_t width, int32_t height,
		  pixman_region32_t *bbox)
{
	float min_x = HUGE_VALF,  min_y = HUGE_VALF;
	float max_x = -HUGE_VALF, max_y = -HUGE_VALF;
	int32_t s[4][2] = {
		{ sx,         sy },
		{ sx,         sy + height },
		{ sx + width, sy },
		{ sx + width, sy + height }
	};
	float int_x, int_y;
	int i;

	if (width == 0 || height == 0) {
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
				  view->geometry.x,
				  view->geometry.y,
				  view->surface->width,
				  view->surface->height);

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

	view_compute_bbox(view, 0, 0,
			  view->surface->width, view->surface->height,
			  &view->transform.boundingbox);

	return 0;
}

WL_EXPORT void
weston_view_update_transform(struct weston_view *view)
{
	struct weston_view *parent = view->geometry.parent;

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

WL_EXPORT int
weston_view_is_mapped(struct weston_view *view)
{
	if (view->output)
		return 1;
	else
		return 0;
}

WL_EXPORT int
weston_surface_is_mapped(struct weston_surface *surface)
{
	if (surface->output)
		return 1;
	else
		return 0;
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
weston_surface_set_size_from_buffer(struct weston_surface *surface)
{
	struct weston_buffer_viewport *vp = &surface->buffer_viewport;
	int32_t width, height;

	if (!surface->buffer_ref.buffer) {
		surface_set_size(surface, 0, 0);
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

	if (vp->surface.width != -1) {
		surface_set_size(surface,
				 vp->surface.width, vp->surface.height);
		return;
	}

	if (vp->buffer.src_width != wl_fixed_from_int(-1)) {
		surface_set_size(surface,
				 fixed_round_up_to_int(vp->buffer.src_width),
				 fixed_round_up_to_int(vp->buffer.src_height));
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

	wl_list_for_each(view, &compositor->view_list, link) {
		weston_view_from_global_fixed(view, x, y, vx, vy);
		if (pixman_region32_contains_point(&view->surface->input,
						   wl_fixed_to_int(*vx),
						   wl_fixed_to_int(*vy),
						   NULL))
			return view;
	}

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
	wl_list_remove(&view->layer_link);
	wl_list_init(&view->layer_link);
	wl_list_remove(&view->link);
	wl_list_init(&view->link);
	view->output_mask = 0;
	weston_surface_assign_output(view->surface);

	if (weston_surface_is_mapped(view->surface))
		return;

	wl_list_for_each(seat, &view->surface->compositor->seat_list, link) {
		if (seat->keyboard && seat->keyboard->focus == view->surface)
			weston_keyboard_set_focus(seat->keyboard, NULL);
		if (seat->pointer && seat->pointer->focus == view)
			weston_pointer_set_focus(seat->pointer,
						 NULL,
						 wl_fixed_from_int(0),
						 wl_fixed_from_int(0));
		if (seat->touch && seat->touch->focus == view)
			weston_touch_set_focus(seat, NULL);
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
	if (surface->pending.buffer)
		wl_list_remove(&surface->pending.buffer_destroy_listener.link);
	surface->pending.buffer = NULL;
	surface->pending.sx = 0;
	surface->pending.sy = 0;
	surface->pending.newly_attached = 0;
}

struct weston_frame_callback {
	struct wl_resource *resource;
	struct wl_list link;
};

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
	wl_list_remove(&view->layer_link);

	pixman_region32_fini(&view->clip);
	pixman_region32_fini(&view->transform.boundingbox);

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

	wl_signal_emit(&surface->destroy_signal, &surface->resource);

	assert(wl_list_empty(&surface->subsurface_list_pending));
	assert(wl_list_empty(&surface->subsurface_list));

	wl_list_for_each_safe(ev, nv, &surface->views, surface_link)
		weston_view_destroy(ev);

	wl_list_for_each_safe(cb, next,
			      &surface->pending.frame_callback_list, link)
		wl_resource_destroy(cb->resource);

	pixman_region32_fini(&surface->pending.input);
	pixman_region32_fini(&surface->pending.opaque);
	pixman_region32_fini(&surface->pending.damage);

	if (surface->pending.buffer)
		wl_list_remove(&surface->pending.buffer_destroy_listener.link);

	weston_buffer_reference(&surface->buffer_ref, NULL);

	pixman_region32_fini(&surface->damage);
	pixman_region32_fini(&surface->opaque);
	pixman_region32_fini(&surface->input);

	wl_list_for_each_safe(cb, next, &surface->frame_callback_list, link)
		wl_resource_destroy(cb->resource);

	free(surface);
}

static void
destroy_surface(struct wl_resource *resource)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);

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

	weston_surface_set_size_from_buffer(surface);
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

	empty_region(&surface->damage);
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
		view_compute_bbox(view, extents->x1, extents->y1,
				  extents->x2 - extents->x1,
				  extents->y2 - extents->y1,
				  &damage);
		pixman_region32_translate(&damage,
					  -view->plane->x,
					  -view->plane->y);
	} else {
		pixman_region32_copy(&damage, &view->surface->damage);
		pixman_region32_translate(&damage,
					  view->geometry.x - view->plane->x,
					  view->geometry.y - view->plane->y);
	}

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

		wl_list_for_each_safe(view, nv, &sub->unused_views, surface_link)
			weston_view_destroy(view);

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
		wl_list_for_each(view, &layer->view_list, layer_link)
			surface_stash_subsurface_views(view->surface);

	wl_list_init(&compositor->view_list);
	wl_list_for_each(layer, &compositor->layer_list, link) {
		wl_list_for_each(view, &layer->view_list, layer_link) {
			view_list_add(compositor, view);
		}
	}

	wl_list_for_each(layer, &compositor->layer_list, link)
		wl_list_for_each(view, &layer->view_list, layer_link)
			surface_free_unused_subsurface_views(view->surface);
}

static int
weston_output_repaint(struct weston_output *output, uint32_t msecs)
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

	/* Rebuild the surface list and update surface transforms up front. */
	weston_compositor_build_view_list(ec);

	if (output->assign_planes && !output->disable_planes)
		output->assign_planes(output);
	else
		wl_list_for_each(ev, &ec->view_list, link)
			weston_view_move_to_plane(ev, &ec->primary_plane);

	wl_list_init(&frame_callback_list);
	wl_list_for_each(ev, &ec->view_list, link) {
		/* Note: This operation is safe to do multiple times on the
		 * same surface.
		 */
		if (ev->surface->output == output) {
			wl_list_insert_list(&frame_callback_list,
					    &ev->surface->frame_callback_list);
			wl_list_init(&ev->surface->frame_callback_list);
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
		wl_callback_send_done(cb->resource, msecs);
		wl_resource_destroy(cb->resource);
	}

	wl_list_for_each_safe(animation, next, &output->animation_list, link) {
		animation->frame_counter++;
		animation->frame(animation, output, msecs);
	}

	return r;
}

static int
weston_compositor_read_input(int fd, uint32_t mask, void *data)
{
	struct weston_compositor *compositor = data;

	wl_event_loop_dispatch(compositor->input_loop, 0);

	return 1;
}

WL_EXPORT void
weston_output_finish_frame(struct weston_output *output, uint32_t msecs)
{
	struct weston_compositor *compositor = output->compositor;
	struct wl_event_loop *loop =
		wl_display_get_event_loop(compositor->wl_display);
	int fd, r;

	output->frame_time = msecs;

	if (output->repaint_needed &&
	    compositor->state != WESTON_COMPOSITOR_SLEEPING &&
	    compositor->state != WESTON_COMPOSITOR_OFFSCREEN) {
		r = weston_output_repaint(output, msecs);
		if (!r)
			return;
	}

	output->repaint_scheduled = 0;
	if (compositor->input_loop_source)
		return;

	fd = wl_event_loop_get_fd(compositor->input_loop);
	compositor->input_loop_source =
		wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
				     weston_compositor_read_input, compositor);
}

static void
idle_repaint(void *data)
{
	struct weston_output *output = data;

	output->start_repaint_loop(output);
}

WL_EXPORT void
weston_layer_init(struct weston_layer *layer, struct wl_list *below)
{
	wl_list_init(&layer->view_list);
	if (below != NULL)
		wl_list_insert(below, &layer->link);
}

WL_EXPORT void
weston_output_schedule_repaint(struct weston_output *output)
{
	struct weston_compositor *compositor = output->compositor;
	struct wl_event_loop *loop;

	if (compositor->state == WESTON_COMPOSITOR_SLEEPING ||
	    compositor->state == WESTON_COMPOSITOR_OFFSCREEN)
		return;

	loop = wl_display_get_event_loop(compositor->wl_display);
	output->repaint_needed = 1;
	if (output->repaint_scheduled)
		return;

	wl_event_loop_add_idle(loop, idle_repaint, output);
	output->repaint_scheduled = 1;

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
	if (surface->pending.buffer)
		wl_list_remove(&surface->pending.buffer_destroy_listener.link);

	surface->pending.sx = sx;
	surface->pending.sy = sy;
	surface->pending.buffer = buffer;
	surface->pending.newly_attached = 1;
	if (buffer) {
		wl_signal_add(&buffer->destroy_signal,
			      &surface->pending.buffer_destroy_listener);
	}
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
		empty_region(&surface->pending.opaque);
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
weston_surface_commit(struct weston_surface *surface)
{
	struct weston_view *view;
	pixman_region32_t opaque;

	/* XXX: wl_viewport.set without an attach should call configure */

	/* wl_surface.set_buffer_transform */
	/* wl_surface.set_buffer_scale */
	/* wl_viewport.set */
	surface->buffer_viewport = surface->pending.buffer_viewport;

	/* wl_surface.attach */
	if (surface->pending.newly_attached)
		weston_surface_attach(surface, surface->pending.buffer);

	if (surface->configure && surface->pending.newly_attached)
		surface->configure(surface,
				   surface->pending.sx, surface->pending.sy);

	weston_surface_reset_pending_buffer(surface);

	/* wl_surface.damage */
	pixman_region32_union(&surface->damage, &surface->damage,
			      &surface->pending.damage);
	pixman_region32_intersect_rect(&surface->damage, &surface->damage,
				       0, 0,
				       surface->width,
				       surface->height);
	empty_region(&surface->pending.damage);

	/* wl_surface.set_opaque_region */
	pixman_region32_init_rect(&opaque, 0, 0,
				  surface->width,
				  surface->height);
	pixman_region32_intersect(&opaque,
				  &opaque, &surface->pending.opaque);

	if (!pixman_region32_equal(&opaque, &surface->opaque)) {
		pixman_region32_copy(&surface->opaque, &opaque);
		wl_list_for_each(view, &surface->views, surface_link)
			weston_view_geometry_dirty(view);
	}

	pixman_region32_fini(&opaque);

	/* wl_surface.set_input_region */
	pixman_region32_fini(&surface->input);
	pixman_region32_init_rect(&surface->input, 0, 0,
				  surface->width,
				  surface->height);
	pixman_region32_intersect(&surface->input,
				  &surface->input, &surface->pending.input);

	/* wl_surface.frame */
	wl_list_insert_list(&surface->frame_callback_list,
			    &surface->pending.frame_callback_list);
	wl_list_init(&surface->pending.frame_callback_list);

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

	surface->pending.buffer_viewport.buffer.transform = transform;
}

static void
surface_set_buffer_scale(struct wl_client *client,
			 struct wl_resource *resource,
			 int32_t scale)
{
	struct weston_surface *surface = wl_resource_get_user_data(resource);

	surface->pending.buffer_viewport.buffer.scale = scale;
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
	struct weston_view *view;
	pixman_region32_t opaque;

	/* wl_surface.set_buffer_transform */
	/* wl_surface.set_buffer_scale */
	/* wl_viewport.set */
	surface->buffer_viewport = sub->cached.buffer_viewport;

	/* wl_surface.attach */
	if (sub->cached.newly_attached)
		weston_surface_attach(surface, sub->cached.buffer_ref.buffer);
	weston_buffer_reference(&sub->cached.buffer_ref, NULL);

	if (surface->configure && sub->cached.newly_attached)
		surface->configure(surface, sub->cached.sx, sub->cached.sy);
	sub->cached.sx = 0;
	sub->cached.sy = 0;
	sub->cached.newly_attached = 0;

	/* wl_surface.damage */
	pixman_region32_union(&surface->damage, &surface->damage,
			      &sub->cached.damage);
	pixman_region32_intersect_rect(&surface->damage, &surface->damage,
				       0, 0,
				       surface->width,
				       surface->height);
	empty_region(&sub->cached.damage);

	/* wl_surface.set_opaque_region */
	pixman_region32_init_rect(&opaque, 0, 0,
				  surface->width,
				  surface->height);
	pixman_region32_intersect(&opaque,
				  &opaque, &sub->cached.opaque);

	if (!pixman_region32_equal(&opaque, &surface->opaque)) {
		pixman_region32_copy(&surface->opaque, &opaque);
		wl_list_for_each(view, &surface->views, surface_link)
			weston_view_geometry_dirty(view);
	}

	pixman_region32_fini(&opaque);

	/* wl_surface.set_input_region */
	pixman_region32_fini(&surface->input);
	pixman_region32_init_rect(&surface->input, 0, 0,
				  surface->width,
				  surface->height);
	pixman_region32_intersect(&surface->input,
				  &surface->input, &sub->cached.input);

	/* wl_surface.frame */
	wl_list_insert_list(&surface->frame_callback_list,
			    &sub->cached.frame_callback_list);
	wl_list_init(&sub->cached.frame_callback_list);

	weston_surface_commit_subsurface_order(surface);

	weston_surface_schedule_repaint(surface);

	sub->cached.has_data = 0;
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
	empty_region(&surface->pending.damage);

	if (surface->pending.newly_attached) {
		sub->cached.newly_attached = 1;
		weston_buffer_reference(&sub->cached.buffer_ref,
					surface->pending.buffer);
	}
	sub->cached.sx += surface->pending.sx;
	sub->cached.sy += surface->pending.sy;

	weston_surface_reset_pending_buffer(surface);

	sub->cached.buffer_viewport = surface->pending.buffer_viewport;

	pixman_region32_copy(&sub->cached.opaque, &surface->pending.opaque);

	pixman_region32_copy(&sub->cached.input, &surface->pending.input);

	wl_list_insert_list(&sub->cached.frame_callback_list,
			    &surface->pending.frame_callback_list);
	wl_list_init(&surface->pending.frame_callback_list);

	sub->cached.has_data = 1;
}

static int
weston_subsurface_is_synchronized(struct weston_subsurface *sub)
{
	while (sub) {
		if (sub->synchronized)
			return 1;

		if (!sub->parent)
			return 0;

		sub = weston_surface_to_subsurface(sub->parent);
	}

	return 0;
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
		if (sub->cached.has_data) {
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

	if (sub->cached.has_data)
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
		/* Cannot call weston_surface_update_transform(),
		 * because that would call it also for the parent surface,
		 * which might not be mapped yet. That would lead to
		 * inconsistent state, where the window could never be
		 * mapped.
		 *
		 * Instead just assing any output, to make
		 * weston_surface_is_mapped() return true, so that when the
		 * parent surface does get mapped, this one will get
		 * included, too. See surface_list_add().
		 */
		assert(!wl_list_empty(&compositor->output_list));
		surface->output = container_of(compositor->output_list.next,
					       struct weston_output, link);
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
weston_subsurface_cache_init(struct weston_subsurface *sub)
{
	pixman_region32_init(&sub->cached.damage);
	pixman_region32_init(&sub->cached.opaque);
	pixman_region32_init(&sub->cached.input);
	wl_list_init(&sub->cached.frame_callback_list);
	sub->cached.buffer_ref.buffer = NULL;
}

static void
weston_subsurface_cache_fini(struct weston_subsurface *sub)
{
	struct weston_frame_callback *cb, *tmp;

	wl_list_for_each_safe(cb, tmp, &sub->cached.frame_callback_list, link)
		wl_resource_destroy(cb->resource);

	weston_buffer_reference(&sub->cached.buffer_ref, NULL);
	pixman_region32_fini(&sub->cached.damage);
	pixman_region32_fini(&sub->cached.opaque);
	pixman_region32_fini(&sub->cached.input);
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
	assert(data == &sub->surface->resource);

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
	assert(data == &sub->parent->resource);
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

		wl_list_for_each_safe(view, next, &sub->surface->views, surface_link)
			weston_view_destroy(view);

		if (sub->parent)
			weston_subsurface_unlink_parent(sub);

		weston_subsurface_cache_fini(sub);

		sub->surface->configure = NULL;
		sub->surface->configure_private = NULL;
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

	sub = calloc(1, sizeof *sub);
	if (!sub)
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
	weston_subsurface_cache_init(sub);
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

	sub = calloc(1, sizeof *sub);
	if (!sub)
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

	if (surface->configure) {
		wl_resource_post_error(resource,
			WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
			"%s%d: wl_surface@%d already has a role",
			where, id, wl_resource_get_id(surface_resource));
		return;
	}

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
	if (version >= 2)
		wl_output_send_scale(resource,
				     output->current_scale);

	wl_list_for_each (mode, &output->mode_list, link) {
		wl_output_send_mode(resource,
				    mode->flags,
				    mode->width,
				    mode->height,
				    mode->refresh);
	}

	if (version >= 2)
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
	output->destroying = 1;

	weston_compositor_remove_output(output->compositor, output);
	wl_list_remove(&output->link);

	wl_signal_emit(&output->compositor->output_destroyed_signal, output);
	wl_signal_emit(&output->destroy_signal, output);

	free(output->name);
	pixman_region32_fini(&output->region);
	pixman_region32_fini(&output->previous_damage);
	output->compositor->output_id_pool &= ~(1 << output->id);

	wl_global_destroy(output->global);
}

static void
weston_output_compute_transform(struct weston_output *output)
{
	struct weston_matrix transform;
	int flip;

	weston_matrix_init(&transform);
	transform.type = WESTON_MATRIX_TRANSFORM_ROTATE;

	switch(output->transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		transform.type |= WESTON_MATRIX_TRANSFORM_OTHER;
		flip = -1;
		break;
	default:
		flip = 1;
		break;
	}

        switch(output->transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
        case WL_OUTPUT_TRANSFORM_FLIPPED:
                transform.d[0] = flip;
                transform.d[1] = 0;
                transform.d[4] = 0;
                transform.d[5] = 1;
                break;
        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
                transform.d[0] = 0;
                transform.d[1] = -flip;
                transform.d[4] = 1;
                transform.d[5] = 0;
                break;
        case WL_OUTPUT_TRANSFORM_180:
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
                transform.d[0] = -flip;
                transform.d[1] = 0;
                transform.d[4] = 0;
                transform.d[5] = -1;
                break;
        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
                transform.d[0] = 0;
                transform.d[1] = flip;
                transform.d[4] = -1;
                transform.d[5] = 0;
                break;
        default:
                break;
        }

	weston_matrix_multiply(&output->matrix, &transform);
}

WL_EXPORT void
weston_output_update_matrix(struct weston_output *output)
{
	float magnification;
	struct weston_matrix camera;
	struct weston_matrix modelview;

	weston_matrix_init(&output->matrix);
	weston_matrix_translate(&output->matrix,
				-(output->x + output->width / 2.0),
				-(output->y + output->height / 2.0), 0);

	weston_matrix_scale(&output->matrix,
			    2.0 / output->width,
			    -2.0 / output->height, 1);

	weston_output_compute_transform(output);

	if (output->zoom.active) {
		magnification = 1 / (1 - output->zoom.spring_z.current);
		weston_matrix_init(&camera);
		weston_matrix_init(&modelview);
		weston_output_update_zoom(output);
		weston_matrix_translate(&camera, output->zoom.trans_x,
					-output->zoom.trans_y, 0);
		weston_matrix_invert(&modelview, &camera);
		weston_matrix_scale(&modelview, magnification, magnification, 1.0);
		weston_matrix_multiply(&output->matrix, &modelview);
	}

	output->dirty = 0;
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
	pixman_region32_t old_region;
	struct wl_resource *resource;

	output->move_x = x - output->x;
	output->move_y = y - output->y;

	if (output->move_x == 0 && output->move_y == 0)
		return;

	pixman_region32_init(&old_region);
	pixman_region32_copy(&old_region, &output->region);

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

	output->id = ffs(~output->compositor->output_id_pool) - 1;
	output->compositor->output_id_pool |= 1 << output->id;

	output->global =
		wl_global_create(c->wl_display, &wl_output_interface, 2,
				 output, bind_output);
	wl_signal_emit(&c->output_created_signal, output);
}

WL_EXPORT void
weston_output_transform_coordinate(struct weston_output *output,
				   wl_fixed_t device_x, wl_fixed_t device_y,
				   wl_fixed_t *x, wl_fixed_t *y)
{
	wl_fixed_t tx, ty;
	wl_fixed_t width, height;

	width = wl_fixed_from_int(output->width * output->current_scale - 1);
	height = wl_fixed_from_int(output->height * output->current_scale - 1);

	switch(output->transform) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
	default:
		tx = device_x;
		ty = device_y;
		break;
	case WL_OUTPUT_TRANSFORM_90:
		tx = device_y;
		ty = height - device_x;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		tx = width - device_x;
		ty = height - device_y;
		break;
	case WL_OUTPUT_TRANSFORM_270:
		tx = width - device_y;
		ty = device_x;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		tx = width - device_x;
		ty = device_y;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		tx = width - device_y;
		ty = height - device_x;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		tx = device_x;
		ty = height - device_y;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		tx = device_y;
		ty = device_x;
		break;
	}

	*x = tx / output->current_scale + wl_fixed_from_int(output->x);
	*y = ty / output->current_scale + wl_fixed_from_int(output->y);
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

static void
log_uname(void)
{
	struct utsname usys;

	uname(&usys);

	weston_log("OS: %s, %s, %s, %s\n", usys.sysname, usys.release,
						usys.version, usys.machine);
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

WL_EXPORT int
weston_compositor_init(struct weston_compositor *ec,
		       struct wl_display *display,
		       int *argc, char *argv[],
		       struct weston_config *config)
{
	struct wl_event_loop *loop;
	struct xkb_rule_names xkb_names;
	struct weston_config_section *s;

	ec->config = config;
	ec->wl_display = display;
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

	if (!wl_global_create(display, &wl_compositor_interface, 3,
			      ec, compositor_bind))
		return -1;

	if (!wl_global_create(display, &wl_subcompositor_interface, 1,
			      ec, bind_subcompositor))
		return -1;

	if (!wl_global_create(ec->wl_display, &wl_scaler_interface, 2,
			      ec, bind_scaler))
		return -1;

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

	s = weston_config_get_section(ec->config, "keyboard", NULL, NULL);
	weston_config_section_get_string(s, "keymap_rules",
					 (char **) &xkb_names.rules, NULL);
	weston_config_section_get_string(s, "keymap_model",
					 (char **) &xkb_names.model, NULL);
	weston_config_section_get_string(s, "keymap_layout",
					 (char **) &xkb_names.layout, NULL);
	weston_config_section_get_string(s, "keymap_variant",
					 (char **) &xkb_names.variant, NULL);
	weston_config_section_get_string(s, "keymap_options",
					 (char **) &xkb_names.options, NULL);

	if (weston_compositor_xkb_init(ec, &xkb_names) < 0)
		return -1;

	text_backend_init(ec);

	wl_data_device_manager_init(ec->wl_display);

	wl_display_init_shm(display);

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->idle_source = wl_event_loop_add_timer(loop, idle_handler, ec);
	wl_event_source_timer_update(ec->idle_source, ec->idle_time * 1000);

	ec->input_loop = wl_event_loop_create();

	weston_layer_init(&ec->fade_layer, &ec->layer_list);
	weston_layer_init(&ec->cursor_layer, &ec->fade_layer.link);

	weston_compositor_schedule_repaint(ec);

	return 0;
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
	weston_binding_list_destroy_all(&ec->button_binding_list);
	weston_binding_list_destroy_all(&ec->touch_binding_list);
	weston_binding_list_destroy_all(&ec->axis_binding_list);
	weston_binding_list_destroy_all(&ec->debug_binding_list);

	weston_plane_release(&ec->primary_plane);

	wl_event_loop_destroy(ec->input_loop);

	weston_config_destroy(ec->config);
}

WL_EXPORT void
weston_compositor_set_default_pointer_grab(struct weston_compositor *ec,
			const struct weston_pointer_grab_interface *interface)
{
	struct weston_seat *seat;

	ec->default_pointer_grab = interface;
	wl_list_for_each(seat, &ec->seat_list, link) {
		if (seat->pointer) {
			weston_pointer_set_default_grab(seat->pointer,
							interface);
		}
	}
}

WL_EXPORT void
weston_version(int *major, int *minor, int *micro)
{
	*major = WESTON_VERSION_MAJOR;
	*minor = WESTON_VERSION_MINOR;
	*micro = WESTON_VERSION_MICRO;
}

static const struct {
	uint32_t bit; /* enum weston_capability */
	const char *desc;
} capability_strings[] = {
	{ WESTON_CAP_ROTATION_ANY, "arbitrary surface rotation:" },
	{ WESTON_CAP_CAPTURE_YFLIP, "screen capture uses y-flip:" },
};

static void
weston_compositor_log_capabilities(struct weston_compositor *compositor)
{
	unsigned i;
	int yes;

	weston_log("Compositor capabilities:\n");
	for (i = 0; i < ARRAY_LENGTH(capability_strings); i++) {
		yes = compositor->capabilities & capability_strings[i].bit;
		weston_log_continue(STAMP_SPACE "%s %s\n",
				    capability_strings[i].desc,
				    yes ? "yes" : "no");
	}
}

static int on_term_signal(int signal_number, void *data)
{
	struct wl_display *display = data;

	weston_log("caught signal %d\n", signal_number);
	wl_display_terminate(display);

	return 1;
}

#ifdef HAVE_LIBUNWIND

static void
print_backtrace(void)
{
	unw_cursor_t cursor;
	unw_context_t context;
	unw_word_t off;
	unw_proc_info_t pip;
	int ret, i = 0;
	char procname[256];
	const char *filename;
	Dl_info dlinfo;

	pip.unwind_info = NULL;
	ret = unw_getcontext(&context);
	if (ret) {
		weston_log("unw_getcontext: %d\n", ret);
		return;
	}

	ret = unw_init_local(&cursor, &context);
	if (ret) {
		weston_log("unw_init_local: %d\n", ret);
		return;
	}

	ret = unw_step(&cursor);
	while (ret > 0) {
		ret = unw_get_proc_info(&cursor, &pip);
		if (ret) {
			weston_log("unw_get_proc_info: %d\n", ret);
			break;
		}

		ret = unw_get_proc_name(&cursor, procname, 256, &off);
		if (ret && ret != -UNW_ENOMEM) {
			if (ret != -UNW_EUNSPEC)
				weston_log("unw_get_proc_name: %d\n", ret);
			procname[0] = '?';
			procname[1] = 0;
		}

		if (dladdr((void *)(pip.start_ip + off), &dlinfo) && dlinfo.dli_fname &&
		    *dlinfo.dli_fname)
			filename = dlinfo.dli_fname;
		else
			filename = "?";

		weston_log("%u: %s (%s%s+0x%x) [%p]\n", i++, filename, procname,
			   ret == -UNW_ENOMEM ? "..." : "", (int)off, (void *)(pip.start_ip + off));

		ret = unw_step(&cursor);
		if (ret < 0)
			weston_log("unw_step: %d\n", ret);
	}
}

#else

static void
print_backtrace(void)
{
	void *buffer[32];
	int i, count;
	Dl_info info;

	count = backtrace(buffer, ARRAY_LENGTH(buffer));
	for (i = 0; i < count; i++) {
		dladdr(buffer[i], &info);
		weston_log("  [%016lx]  %s  (%s)\n",
			(long) buffer[i],
			info.dli_sname ? info.dli_sname : "--",
			info.dli_fname);
	}
}

#endif

static void
on_caught_signal(int s, siginfo_t *siginfo, void *context)
{
	/* This signal handler will do a best-effort backtrace, and
	 * then call the backend restore function, which will switch
	 * back to the vt we launched from or ungrab X etc and then
	 * raise SIGTRAP.  If we run weston under gdb from X or a
	 * different vt, and tell gdb "handle *s* nostop", this
	 * will allow weston to switch back to gdb on crash and then
	 * gdb will catch the crash with SIGTRAP.*/

	weston_log("caught signal: %d\n", s);

	print_backtrace();

	segv_compositor->restore(segv_compositor);

	raise(SIGTRAP);
}

WL_EXPORT void *
weston_load_module(const char *name, const char *entrypoint)
{
	char path[PATH_MAX];
	void *module, *init;

	if (name == NULL)
		return NULL;

	if (name[0] != '/')
		snprintf(path, sizeof path, "%s/%s", MODULEDIR, name);
	else
		snprintf(path, sizeof path, "%s", name);

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

static int
load_modules(struct weston_compositor *ec, const char *modules,
	     int *argc, char *argv[])
{
	const char *p, *end;
	char buffer[256];
	int (*module_init)(struct weston_compositor *ec,
			   int *argc, char *argv[]);

	if (modules == NULL)
		return 0;

	p = modules;
	while (*p) {
		end = strchrnul(p, ',');
		snprintf(buffer, sizeof buffer, "%.*s", (int) (end - p), p);
		module_init = weston_load_module(buffer, "module_init");
		if (module_init)
			module_init(ec, argc, argv);
		p = end;
		while (*p == ',')
			p++;

	}

	return 0;
}

static const char xdg_error_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR is not set.\n";

static const char xdg_wrong_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR\n"
	"is set to \"%s\", which is not a directory.\n";

static const char xdg_wrong_mode_message[] =
	"warning: XDG_RUNTIME_DIR \"%s\" is not configured\n"
	"correctly.  Unix access mode must be 0700 (current mode is %o),\n"
	"and must be owned by the user (current owner is UID %d).\n";

static const char xdg_detail_message[] =
	"Refer to your distribution on how to get it, or\n"
	"http://www.freedesktop.org/wiki/Specifications/basedir-spec\n"
	"on how to implement it.\n";

static void
verify_xdg_runtime_dir(void)
{
	char *dir = getenv("XDG_RUNTIME_DIR");
	struct stat s;

	if (!dir) {
		weston_log(xdg_error_message);
		weston_log_continue(xdg_detail_message);
		exit(EXIT_FAILURE);
	}

	if (stat(dir, &s) || !S_ISDIR(s.st_mode)) {
		weston_log(xdg_wrong_message, dir);
		weston_log_continue(xdg_detail_message);
		exit(EXIT_FAILURE);
	}

	if ((s.st_mode & 0777) != 0700 || s.st_uid != getuid()) {
		weston_log(xdg_wrong_mode_message,
			   dir, s.st_mode & 0777, s.st_uid);
		weston_log_continue(xdg_detail_message);
	}
}

static int
usage(int error_code)
{
	fprintf(stderr,
		"Usage: weston [OPTIONS]\n\n"
		"This is weston version " VERSION ", the Wayland reference compositor.\n"
		"Weston supports multiple backends, and depending on which backend is in use\n"
		"different options will be accepted.\n\n"


		"Core options:\n\n"
		"  --version\t\tPrint weston version\n"
		"  -B, --backend=MODULE\tBackend module, one of drm-backend.so,\n"
		"\t\t\t\tfbdev-backend.so, x11-backend.so or\n"
		"\t\t\t\twayland-backend.so\n"
		"  --shell=MODULE\tShell module, defaults to desktop-shell.so\n"
		"  -S, --socket=NAME\tName of socket to listen on\n"
		"  -i, --idle-time=SECS\tIdle time in seconds\n"
		"  --modules\t\tLoad the comma-separated list of modules\n"
		"  --log==FILE\t\tLog to the given file\n"
		"  -h, --help\t\tThis help message\n\n");

	fprintf(stderr,
		"Options for drm-backend.so:\n\n"
		"  --connector=ID\tBring up only this connector\n"
		"  --seat=SEAT\t\tThe seat that weston should run on\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --current-mode\tPrefer current KMS mode over EDID preferred mode\n\n");

	fprintf(stderr,
		"Options for fbdev-backend.so:\n\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --device=DEVICE\tThe framebuffer device to use\n\n");

	fprintf(stderr,
		"Options for x11-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of X window\n"
		"  --height=HEIGHT\tHeight of X window\n"
		"  --fullscreen\t\tRun in fullscreen mode\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --output-count=COUNT\tCreate multiple outputs\n"
		"  --no-input\t\tDont create input devices\n\n");

	fprintf(stderr,
		"Options for wayland-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of Wayland surface\n"
		"  --height=HEIGHT\tHeight of Wayland surface\n"
		"  --scale=SCALE\tScale factor of ouput\n"
		"  --fullscreen\t\tRun in fullscreen mode\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --output-count=COUNT\tCreate multiple outputs\n"
		"  --sprawl\t\tCreate one fullscreen output for every parent output\n"
		"  --display=DISPLAY\tWayland display to connect to\n\n");

#if defined(BUILD_RPI_COMPOSITOR) && defined(HAVE_BCM_HOST)
	fprintf(stderr,
		"Options for rpi-backend.so:\n\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --single-buffer\tUse single-buffered Dispmanx elements.\n"
		"  --transform=TR\tThe output transformation, TR is one of:\n"
		"\tnormal 90 180 270 flipped flipped-90 flipped-180 flipped-270\n"
		"  --opaque-regions\tEnable support for opaque regions, can be "
		"very slow without support in the GPU firmware.\n"
		"\n");
#endif

#if defined(BUILD_RDP_COMPOSITOR)
    fprintf(stderr,
       "Options for rdp-backend.so:\n\n"
       "  --width=WIDTH\t\tWidth of desktop\n"
       "  --height=HEIGHT\tHeight of desktop\n"
       "  --env-socket=SOCKET\tUse that socket as peer connection\n"
       "  --address=ADDR\tThe address to bind\n"
       "  --port=PORT\tThe port to listen on\n"
       "  --no-clients-resize\tThe RDP peers will be forced to the size of the desktop\n"
       "  --rdp4-key=FILE\tThe file containing the key for RDP4 encryption\n"
       "  --rdp-tls-cert=FILE\tThe file containing the certificate for TLS encryption\n"
       "  --rdp-tls-key=FILE\tThe file containing the private key for TLS encryption\n"
       "\n");
#endif

	exit(error_code);
}

static void
catch_signals(void)
{
	struct sigaction action;

	action.sa_flags = SA_SIGINFO | SA_RESETHAND;
	action.sa_sigaction = on_caught_signal;
	sigemptyset(&action.sa_mask);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGABRT, &action, NULL);
}

static void
handle_primary_client_destroyed(struct wl_listener *listener, void *data)
{
	struct wl_client *client = data;

	weston_log("Primary client died.  Closing...\n");

	wl_display_terminate(wl_client_get_display(client));
}

int main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;
	struct wl_display *display;
	struct weston_compositor *ec;
	struct wl_event_source *signals[4];
	struct wl_event_loop *loop;
	struct weston_compositor
		*(*backend_init)(struct wl_display *display,
				 int *argc, char *argv[],
				 struct weston_config *config);
	int i, fd;
	char *backend = NULL;
	char *option_backend = NULL;
	char *shell = NULL;
	char *option_shell = NULL;
	char *modules, *option_modules = NULL;
	char *log = NULL;
	char *server_socket = NULL, *end;
	int32_t idle_time = 300;
	int32_t help = 0;
	char *socket_name = "wayland-0";
	int32_t version = 0;
	struct weston_config *config;
	struct weston_config_section *section;
	struct wl_client *primary_client;
	struct wl_listener primary_client_destroyed;

	const struct weston_option core_options[] = {
		{ WESTON_OPTION_STRING, "backend", 'B', &option_backend },
		{ WESTON_OPTION_STRING, "shell", 0, &option_shell },
		{ WESTON_OPTION_STRING, "socket", 'S', &socket_name },
		{ WESTON_OPTION_INTEGER, "idle-time", 'i', &idle_time },
		{ WESTON_OPTION_STRING, "modules", 0, &option_modules },
		{ WESTON_OPTION_STRING, "log", 0, &log },
		{ WESTON_OPTION_BOOLEAN, "help", 'h', &help },
		{ WESTON_OPTION_BOOLEAN, "version", 0, &version },
	};

	parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);

	if (help)
		usage(EXIT_SUCCESS);

	if (version) {
		printf(PACKAGE_STRING "\n");
		return EXIT_SUCCESS;
	}

	weston_log_file_open(log);

	weston_log("%s\n"
		   STAMP_SPACE "%s\n"
		   STAMP_SPACE "Bug reports to: %s\n"
		   STAMP_SPACE "Build: %s\n",
		   PACKAGE_STRING, PACKAGE_URL, PACKAGE_BUGREPORT,
		   BUILD_ID);
	log_uname();

	verify_xdg_runtime_dir();

	display = wl_display_create();

	loop = wl_display_get_event_loop(display);
	signals[0] = wl_event_loop_add_signal(loop, SIGTERM, on_term_signal,
					      display);
	signals[1] = wl_event_loop_add_signal(loop, SIGINT, on_term_signal,
					      display);
	signals[2] = wl_event_loop_add_signal(loop, SIGQUIT, on_term_signal,
					      display);

	wl_list_init(&child_process_list);
	signals[3] = wl_event_loop_add_signal(loop, SIGCHLD, sigchld_handler,
					      NULL);

	config = weston_config_parse("weston.ini");
	if (config != NULL) {
		weston_log("Using config file '%s'\n",
			   weston_config_get_full_path(config));
	} else {
		weston_log("Starting with no config file.\n");
	}
	section = weston_config_get_section(config, "core", NULL, NULL);

	if (option_backend)
		backend = strdup(option_backend);
	else
		weston_config_section_get_string(section, "backend", &backend,
						 NULL);

	if (!backend) {
		if (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET"))
			backend = strdup("wayland-backend.so");
		else if (getenv("DISPLAY"))
			backend = strdup("x11-backend.so");
		else
			backend = strdup(WESTON_NATIVE_BACKEND);
	}

	backend_init = weston_load_module(backend, "backend_init");
	free(backend);
	if (!backend_init)
		exit(EXIT_FAILURE);

	ec = backend_init(display, &argc, argv, config);
	if (ec == NULL) {
		weston_log("fatal: failed to create compositor\n");
		exit(EXIT_FAILURE);
	}

	catch_signals();
	segv_compositor = ec;

	ec->idle_time = idle_time;
	ec->default_pointer_grab = NULL;

	setenv("WAYLAND_DISPLAY", socket_name, 1);

	if (option_shell)
		shell = strdup(option_shell);
	else
		weston_config_section_get_string(section, "shell", &shell,
						 "desktop-shell.so");

	if (load_modules(ec, shell, &argc, argv) < 0) {
		free(shell);
		goto out;
	}
	free(shell);

	weston_config_section_get_string(section, "modules", &modules, "");
	if (load_modules(ec, modules, &argc, argv) < 0) {
		free(modules);
		goto out;
	}
	free(modules);

	if (load_modules(ec, option_modules, &argc, argv) < 0)
		goto out;

	for (i = 1; i < argc; i++)
		weston_log("fatal: unhandled option: %s\n", argv[i]);
	if (argc > 1) {
		ret = EXIT_FAILURE;
		goto out;
	}

	weston_compositor_log_capabilities(ec);

	server_socket = getenv("WAYLAND_SERVER_SOCKET");
	if (server_socket) {
		weston_log("Running with single client\n");
		fd = strtol(server_socket, &end, 0);
		if (*end != '\0')
			fd = -1;
	} else {
		fd = -1;
	}

	if (fd != -1) {
		primary_client = wl_client_create(display, fd);
		if (!primary_client) {
			weston_log("fatal: failed to add client: %m\n");
			ret = EXIT_FAILURE;
			goto out;
		}
		primary_client_destroyed.notify =
			handle_primary_client_destroyed;
		wl_client_add_destroy_listener(primary_client,
					       &primary_client_destroyed);
	} else {
		if (wl_display_add_socket(display, socket_name)) {
			weston_log("fatal: failed to add socket: %m\n");
			ret = EXIT_FAILURE;
			goto out;
		}
	}

	weston_compositor_wake(ec);

	wl_display_run(display);

 out:
	/* prevent further rendering while shutting down */
	ec->state = WESTON_COMPOSITOR_OFFSCREEN;

	wl_signal_emit(&ec->destroy_signal, ec);

	for (i = ARRAY_LENGTH(signals); i;)
		wl_event_source_remove(signals[--i]);

	weston_compositor_xkb_destroy(ec);

	ec->destroy(ec);
	wl_display_destroy(display);

	weston_log_file_close();

	return ret;
}
