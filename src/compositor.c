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

#define _GNU_SOURCE

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
#include <ctype.h>

#include <wayland-server.h>
#include "compositor.h"
#include "../shared/os-compatibility.h"
#include "git-version.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))


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

WL_EXPORT int
weston_output_switch_mode(struct weston_output *output, struct weston_mode *mode)
{
	if (!output->switch_mode)
		return -1;

	return output->switch_mode(output, mode);
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

	/* Launch clients as the user. */
	seteuid(getuid());

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
		exit(-1);
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
update_shm_texture(struct weston_surface *surface);

static void
surface_handle_buffer_destroy(struct wl_listener *listener, void *data)
{
	struct weston_surface *es =
		container_of(listener, struct weston_surface, 
			     buffer_destroy_listener);

	if (es->buffer && wl_buffer_is_shm(es->buffer))
		update_shm_texture(es);

	es->buffer = NULL;
}

static const pixman_region32_data_t undef_region_data;

static void
undef_region(pixman_region32_t *region)
{
	if (region->data != &undef_region_data)
		pixman_region32_fini(region);
	region->data = (pixman_region32_data_t *) &undef_region_data;
}

static int
region_is_undefined(pixman_region32_t *region)
{
	return region->data == &undef_region_data;
}

static void
empty_region(pixman_region32_t *region)
{
	if (!region_is_undefined(region))
		pixman_region32_fini(region);

	pixman_region32_init(region);
}

WL_EXPORT struct weston_surface *
weston_surface_create(struct weston_compositor *compositor)
{
	struct weston_surface *surface;

	surface = calloc(1, sizeof *surface);
	if (surface == NULL)
		return NULL;

	wl_signal_init(&surface->surface.resource.destroy_signal);

	wl_list_init(&surface->link);
	wl_list_init(&surface->layer_link);

	surface->surface.resource.client = NULL;

	surface->compositor = compositor;
	surface->alpha = 1.0;
	surface->opaque_rect[0] = 0.0;
	surface->opaque_rect[1] = 0.0;
	surface->opaque_rect[2] = 0.0;
	surface->opaque_rect[3] = 0.0;
	surface->pitch = 1;

	surface->num_textures = 0;
	surface->num_images = 0;

	surface->buffer = NULL;
	surface->output = NULL;
	surface->plane = &compositor->primary_plane;

	pixman_region32_init(&surface->damage);
	pixman_region32_init(&surface->opaque);
	pixman_region32_init(&surface->clip);
	undef_region(&surface->input);
	pixman_region32_init(&surface->transform.opaque);
	wl_list_init(&surface->frame_callback_list);

	surface->buffer_destroy_listener.notify =
		surface_handle_buffer_destroy;

	wl_list_init(&surface->geometry.transformation_list);
	wl_list_insert(&surface->geometry.transformation_list,
		       &surface->transform.position.link);
	weston_matrix_init(&surface->transform.position.matrix);
	pixman_region32_init(&surface->transform.boundingbox);
	surface->geometry.dirty = 1;

	return surface;
}

WL_EXPORT void
weston_surface_set_color(struct weston_surface *surface,
		 GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	surface->color[0] = red;
	surface->color[1] = green;
	surface->color[2] = blue;
	surface->color[3] = alpha;
	surface->shader = &surface->compositor->solid_shader;
}

WL_EXPORT void
weston_surface_to_global_float(struct weston_surface *surface,
			       GLfloat sx, GLfloat sy, GLfloat *x, GLfloat *y)
{
	if (surface->transform.enabled) {
		struct weston_vector v = { { sx, sy, 0.0f, 1.0f } };

		weston_matrix_transform(&surface->transform.matrix, &v);

		if (fabsf(v.f[3]) < 1e-6) {
			weston_log("warning: numerical instability in "
				"weston_surface_to_global(), divisor = %g\n",
				v.f[3]);
			*x = 0;
			*y = 0;
			return;
		}

		*x = v.f[0] / v.f[3];
		*y = v.f[1] / v.f[3];
	} else {
		*x = sx + surface->geometry.x;
		*y = sy + surface->geometry.y;
	}
}

WL_EXPORT void
weston_surface_move_to_plane(struct weston_surface *surface,
			     struct weston_plane *plane)
{
	if (surface->plane == plane)
		return;

	weston_surface_damage_below(surface);
	surface->plane = plane;
	weston_surface_damage(surface);
}

WL_EXPORT void
weston_surface_damage_below(struct weston_surface *surface)
{
	pixman_region32_t damage;

	pixman_region32_init(&damage);
	pixman_region32_subtract(&damage, &surface->transform.boundingbox,
				 &surface->clip);
	pixman_region32_union(&surface->plane->damage,
			      &surface->plane->damage, &damage);
	pixman_region32_fini(&damage);
}

static void
surface_compute_bbox(struct weston_surface *surface, int32_t sx, int32_t sy,
		     int32_t width, int32_t height,
		     pixman_region32_t *bbox)
{
	GLfloat min_x = HUGE_VALF,  min_y = HUGE_VALF;
	GLfloat max_x = -HUGE_VALF, max_y = -HUGE_VALF;
	int32_t s[4][2] = {
		{ sx,         sy },
		{ sx,         sy + height },
		{ sx + width, sy },
		{ sx + width, sy + height }
	};
	GLfloat int_x, int_y;
	int i;

	for (i = 0; i < 4; ++i) {
		GLfloat x, y;
		weston_surface_to_global_float(surface,
					       s[i][0], s[i][1], &x, &y);
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
weston_surface_update_transform_disable(struct weston_surface *surface)
{
	surface->transform.enabled = 0;

	/* round off fractions when not transformed */
	surface->geometry.x = roundf(surface->geometry.x);
	surface->geometry.y = roundf(surface->geometry.y);

	pixman_region32_init_rect(&surface->transform.boundingbox,
				  surface->geometry.x,
				  surface->geometry.y,
				  surface->geometry.width,
				  surface->geometry.height);

	if (surface->alpha == 1.0) {
		pixman_region32_copy(&surface->transform.opaque,
				     &surface->opaque);
		pixman_region32_translate(&surface->transform.opaque,
					  surface->geometry.x,
					  surface->geometry.y);
	}
}

static int
weston_surface_update_transform_enable(struct weston_surface *surface)
{
	struct weston_matrix *matrix = &surface->transform.matrix;
	struct weston_matrix *inverse = &surface->transform.inverse;
	struct weston_transform *tform;

	surface->transform.enabled = 1;

	/* Otherwise identity matrix, but with x and y translation. */
	surface->transform.position.matrix.d[12] = surface->geometry.x;
	surface->transform.position.matrix.d[13] = surface->geometry.y;

	weston_matrix_init(matrix);
	wl_list_for_each(tform, &surface->geometry.transformation_list, link)
		weston_matrix_multiply(matrix, &tform->matrix);

	if (weston_matrix_invert(inverse, matrix) < 0) {
		/* Oops, bad total transformation, not invertible */
		weston_log("error: weston_surface %p"
			" transformation not invertible.\n", surface);
		return -1;
	}

	surface_compute_bbox(surface, 0, 0, surface->geometry.width,
			     surface->geometry.height,
			     &surface->transform.boundingbox);

	return 0;
}

WL_EXPORT void
weston_surface_update_transform(struct weston_surface *surface)
{
	if (!surface->geometry.dirty)
		return;

	surface->geometry.dirty = 0;

	weston_surface_damage_below(surface);

	pixman_region32_fini(&surface->transform.boundingbox);
	pixman_region32_fini(&surface->transform.opaque);
	pixman_region32_init(&surface->transform.opaque);

	if (region_is_undefined(&surface->input))
		pixman_region32_init_rect(&surface->input, 0, 0, 
					  surface->geometry.width,
					  surface->geometry.height);

	/* transform.position is always in transformation_list */
	if (surface->geometry.transformation_list.next ==
	    &surface->transform.position.link &&
	    surface->geometry.transformation_list.prev ==
	    &surface->transform.position.link) {
		weston_surface_update_transform_disable(surface);
	} else {
		if (weston_surface_update_transform_enable(surface) < 0)
			weston_surface_update_transform_disable(surface);
	}

	weston_surface_damage_below(surface);

	if (weston_surface_is_mapped(surface))
		weston_surface_assign_output(surface);
}

WL_EXPORT void
weston_surface_to_global_fixed(struct weston_surface *surface,
			       wl_fixed_t sx, wl_fixed_t sy,
			       wl_fixed_t *x, wl_fixed_t *y)
{
	GLfloat xf, yf;

	weston_surface_to_global_float(surface,
	                               wl_fixed_to_double(sx),
				       wl_fixed_to_double(sy),
				       &xf, &yf);
	*x = wl_fixed_from_double(xf);
	*y = wl_fixed_from_double(yf);
}

static void
surface_from_global_float(struct weston_surface *surface,
			  GLfloat x, GLfloat y, GLfloat *sx, GLfloat *sy)
{
	if (surface->transform.enabled) {
		struct weston_vector v = { { x, y, 0.0f, 1.0f } };

		weston_matrix_transform(&surface->transform.inverse, &v);

		if (fabsf(v.f[3]) < 1e-6) {
			weston_log("warning: numerical instability in "
				"weston_surface_from_global(), divisor = %g\n",
				v.f[3]);
			*sx = 0;
			*sy = 0;
			return;
		}

		*sx = v.f[0] / v.f[3];
		*sy = v.f[1] / v.f[3];
	} else {
		*sx = x - surface->geometry.x;
		*sy = y - surface->geometry.y;
	}
}

WL_EXPORT void
weston_surface_from_global_fixed(struct weston_surface *surface,
			         wl_fixed_t x, wl_fixed_t y,
			         wl_fixed_t *sx, wl_fixed_t *sy)
{
	GLfloat sxf, syf;

	surface_from_global_float(surface,
	                          wl_fixed_to_double(x),
				  wl_fixed_to_double(y),
				  &sxf, &syf);
	*sx = wl_fixed_from_double(sxf);
	*sy = wl_fixed_from_double(syf);
}

WL_EXPORT void
weston_surface_from_global(struct weston_surface *surface,
			   int32_t x, int32_t y, int32_t *sx, int32_t *sy)
{
	GLfloat sxf, syf;

	surface_from_global_float(surface, x, y, &sxf, &syf);
	*sx = floorf(sxf);
	*sy = floorf(syf);
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
weston_surface_damage(struct weston_surface *surface)
{
	pixman_region32_union_rect(&surface->damage, &surface->damage,
				   0, 0, surface->geometry.width,
				   surface->geometry.height);

	weston_surface_schedule_repaint(surface);
}

WL_EXPORT void
weston_surface_configure(struct weston_surface *surface,
			 GLfloat x, GLfloat y, int width, int height)
{
	surface->geometry.x = x;
	surface->geometry.y = y;
	surface->geometry.width = width;
	surface->geometry.height = height;
	surface->geometry.dirty = 1;
}

WL_EXPORT void
weston_surface_set_position(struct weston_surface *surface,
			    GLfloat x, GLfloat y)
{
	surface->geometry.x = x;
	surface->geometry.y = y;
	surface->geometry.dirty = 1;
}

WL_EXPORT int
weston_surface_is_mapped(struct weston_surface *surface)
{
	if (surface->output)
		return 1;
	else
		return 0;
}

WL_EXPORT uint32_t
weston_compositor_get_time(void)
{
       struct timeval tv;

       gettimeofday(&tv, NULL);

       return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static struct weston_surface *
weston_compositor_pick_surface(struct weston_compositor *compositor,
			       wl_fixed_t x, wl_fixed_t y,
			       wl_fixed_t *sx, wl_fixed_t *sy)
{
	struct weston_surface *surface;

	wl_list_for_each(surface, &compositor->surface_list, link) {
		weston_surface_from_global_fixed(surface, x, y, sx, sy);
		if (pixman_region32_contains_point(&surface->input,
						   wl_fixed_to_int(*sx),
						   wl_fixed_to_int(*sy),
						   NULL))
			return surface;
	}

	return NULL;
}

static void
weston_device_repick(struct weston_seat *seat)
{
	const struct wl_pointer_grab_interface *interface;
	struct weston_surface *surface, *focus;
	struct wl_pointer *pointer = seat->seat.pointer;

	if (!pointer)
		return;

	surface = weston_compositor_pick_surface(seat->compositor,
						 pointer->x,
						 pointer->y,
						 &pointer->current_x,
						 &pointer->current_y);

	if (&surface->surface != pointer->current) {
		interface = pointer->grab->interface;
		pointer->current = &surface->surface;
		interface->focus(pointer->grab, &surface->surface,
				 pointer->current_x,
				 pointer->current_y);
	}

	focus = (struct weston_surface *) pointer->grab->focus;
	if (focus)
		weston_surface_from_global_fixed(focus,
						 pointer->x,
						 pointer->y,
					         &pointer->grab->x,
					         &pointer->grab->y);
}

static void
weston_compositor_repick(struct weston_compositor *compositor)
{
	struct weston_seat *seat;

	if (!compositor->focus)
		return;

	wl_list_for_each(seat, &compositor->seat_list, link)
		weston_device_repick(seat);
}

WL_EXPORT void
weston_surface_unmap(struct weston_surface *surface)
{
	struct weston_seat *seat;

	weston_surface_damage_below(surface);
	surface->output = NULL;
	wl_list_remove(&surface->layer_link);

	wl_list_for_each(seat, &surface->compositor->seat_list, link) {
		if (seat->seat.keyboard &&
		    seat->seat.keyboard->focus == &surface->surface)
			wl_keyboard_set_focus(seat->seat.keyboard, NULL);
		if (seat->seat.pointer &&
		    seat->seat.pointer->focus == &surface->surface)
			wl_pointer_set_focus(seat->seat.pointer,
					     NULL,
					     wl_fixed_from_int(0),
					     wl_fixed_from_int(0));
	}

	weston_surface_schedule_repaint(surface);
}

struct weston_frame_callback {
	struct wl_resource resource;
	struct wl_list link;
};

static void
destroy_surface(struct wl_resource *resource)
{
	int i;

	struct weston_surface *surface =
		container_of(resource,
			     struct weston_surface, surface.resource);
	struct weston_compositor *compositor = surface->compositor;
	struct weston_frame_callback *cb, *next;

	if (weston_surface_is_mapped(surface))
		weston_surface_unmap(surface);

	glDeleteTextures(surface->num_textures, surface->textures);

	if (surface->buffer)
		wl_list_remove(&surface->buffer_destroy_listener.link);

	for (i = 0; i < surface->num_images; i++)
		compositor->destroy_image(compositor->egl_display,
					  surface->images[i]);

	pixman_region32_fini(&surface->transform.boundingbox);
	pixman_region32_fini(&surface->damage);
	pixman_region32_fini(&surface->opaque);
	pixman_region32_fini(&surface->clip);
	if (!region_is_undefined(&surface->input))
		pixman_region32_fini(&surface->input);

	wl_list_for_each_safe(cb, next, &surface->frame_callback_list, link)
		wl_resource_destroy(&cb->resource);

	free(surface);
}

WL_EXPORT void
weston_surface_destroy(struct weston_surface *surface)
{
	/* Not a valid way to destroy a client surface */
	assert(surface->surface.resource.client == NULL);

	wl_signal_emit(&surface->surface.resource.destroy_signal,
		       &surface->surface.resource);
	destroy_surface(&surface->surface.resource);
}

static void
ensure_textures(struct weston_surface *es, int num_textures)
{
	int i;

	if (num_textures <= es->num_textures)
		return;

	for (i = es->num_textures; i < num_textures; i++) {
		glGenTextures(1, &es->textures[i]);
		glBindTexture(es->target, es->textures[i]);
		glTexParameteri(es->target,
				GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(es->target,
				GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	es->num_textures = num_textures;
	glBindTexture(es->target, 0);
}

static void
weston_surface_attach(struct wl_surface *surface, struct wl_buffer *buffer)
{
	struct weston_surface *es = (struct weston_surface *) surface;
	struct weston_compositor *ec = es->compositor;
	EGLint attribs[3], format;
	int i, num_planes;

	if (es->buffer) {
		weston_buffer_post_release(es->buffer);
		wl_list_remove(&es->buffer_destroy_listener.link);
	}

	es->buffer = buffer;

	if (!buffer) {
		if (weston_surface_is_mapped(es))
			weston_surface_unmap(es);
		for (i = 0; i < es->num_images; i++) {
			ec->destroy_image(ec->egl_display, es->images[i]);
			es->images[i] = NULL;
		}
		es->num_images = 0;
		glDeleteTextures(es->num_textures, es->textures);
		es->num_textures = 0;
		return;
	}

	buffer->busy_count++;
	wl_signal_add(&es->buffer->resource.destroy_signal,
		      &es->buffer_destroy_listener);

	if (es->geometry.width != buffer->width ||
	    es->geometry.height != buffer->height) {
		undef_region(&es->input);
		pixman_region32_fini(&es->opaque);
		pixman_region32_init(&es->opaque);
	}

	if (wl_buffer_is_shm(buffer)) {
		es->pitch = wl_shm_buffer_get_stride(buffer) / 4;
		es->target = GL_TEXTURE_2D;

		ensure_textures(es, 1);
		glBindTexture(GL_TEXTURE_2D, es->textures[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     es->pitch, es->buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		if (wl_shm_buffer_get_format(buffer) == WL_SHM_FORMAT_XRGB8888)
			es->shader = &ec->texture_shader_rgbx;
		else
			es->shader = &ec->texture_shader_rgba;
	} else if (ec->query_buffer(ec->egl_display, buffer,
				    EGL_TEXTURE_FORMAT, &format)) {
		for (i = 0; i < es->num_images; i++)
			ec->destroy_image(ec->egl_display, es->images[i]);
		es->num_images = 0;
		es->target = GL_TEXTURE_2D;
		switch (format) {
		case EGL_TEXTURE_RGB:
		case EGL_TEXTURE_RGBA:
		default:
			num_planes = 1;
			es->shader = &ec->texture_shader_rgba;
			break;
		case EGL_TEXTURE_EXTERNAL_WL:
			num_planes = 1;
			es->target = GL_TEXTURE_EXTERNAL_OES;
			es->shader = &ec->texture_shader_egl_external;
			break;
		case EGL_TEXTURE_Y_UV_WL:
			num_planes = 2;
			es->shader = &ec->texture_shader_y_uv;
			break;
		case EGL_TEXTURE_Y_U_V_WL:
			num_planes = 3;
			es->shader = &ec->texture_shader_y_u_v;
			break;
		case EGL_TEXTURE_Y_XUXV_WL:
			num_planes = 2;
			es->shader = &ec->texture_shader_y_xuxv;
			break;
		}

		ensure_textures(es, num_planes);
		for (i = 0; i < num_planes; i++) {
			attribs[0] = EGL_WAYLAND_PLANE_WL;
			attribs[1] = i;
			attribs[2] = EGL_NONE;
			es->images[i] = ec->create_image(ec->egl_display,
							 NULL,
							 EGL_WAYLAND_BUFFER_WL,
							 buffer, attribs);
			if (!es->images[i]) {
				weston_log("failed to create img for plane %d\n", i);
				continue;
			}
			es->num_images++;

			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(es->target, es->textures[i]);
			ec->image_target_texture_2d(es->target,
						    es->images[i]);
		}

		es->pitch = buffer->width;
	} else {
		weston_log("unhandled buffer type!\n");
	}
}


#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) > (b)) ? (b) : (a))
#define clip(x, a, b)  min(max(x, a), b)
#define sign(x)   ((x) >= 0)

static int
calculate_edges(struct weston_surface *es, pixman_box32_t *rect,
		pixman_box32_t *surf_rect, GLfloat *ex, GLfloat *ey)
{
	int i, n = 0;
	GLfloat min_x, max_x, min_y, max_y;
	GLfloat x[4] = {
			surf_rect->x1, surf_rect->x2, surf_rect->x2, surf_rect->x1,
	};
	GLfloat y[4] = {
			surf_rect->y1, surf_rect->y1, surf_rect->y2, surf_rect->y2,
	};
	GLfloat cx1 = rect->x1;
	GLfloat cx2 = rect->x2;
	GLfloat cy1 = rect->y1;
	GLfloat cy2 = rect->y2;

	GLfloat dist_squared(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
	{
		GLfloat dx = (x1 - x2);
		GLfloat dy = (y1 - y2);
		return dx * dx + dy * dy;
	}

	void append_vertex(GLfloat x, GLfloat y)
	{
		/* don't emit duplicate vertices: */
		if ((n > 0) && (ex[n-1] == x) && (ey[n-1] == y))
			return;
		ex[n] = x;
		ey[n] = y;
		n++;
	}

	/* transform surface to screen space: */
	for (i = 0; i < 4; i++)
		weston_surface_to_global_float(es, x[i], y[i], &x[i], &y[i]);

	/* find bounding box: */
	min_x = max_x = x[0];
	min_y = max_y = y[0];

	for (i = 1; i < 4; i++) {
		min_x = min(min_x, x[i]);
		max_x = max(max_x, x[i]);
		min_y = min(min_y, y[i]);
		max_y = max(max_y, y[i]);
	}

	/* First, simple bounding box check to discard early transformed
	 * surface rects that do not intersect with the clip region:
	 */
	if ((min_x > cx2) || (max_x < cx1) ||
			(min_y > cy2) || (max_y < cy1))
		return 0;

	/* Simple case, bounding box edges are parallel to surface edges,
	 * there will be only four edges.  We just need to clip the surface
	 * vertices to the clip rect bounds:
	 */
	if (!es->transform.enabled) {
		for (i = 0; i < 4; i++) {
			ex[n] = clip(x[i], cx1, cx2);
			ey[n] = clip(y[i], cy1, cy2);
			n++;
		}
		return 4;
	}

	/* Hard case, transformation applied.  We need to find the vertices
	 * of the shape that is the intersection of the clip rect and
	 * transformed surface.  This can be anything from 3 to 8 sides.
	 *
	 * Observation: all the resulting vertices will be the intersection
	 * points of the transformed surface and the clip rect, plus the
	 * vertices of the clip rect which are enclosed by the transformed
	 * surface and the vertices of the transformed surface which are
	 * enclosed by the clip rect.
	 *
	 * Observation: there will be zero, one, or two resulting vertices
	 * for each edge of the src rect.
	 *
	 * Loop over four edges of the transformed rect:
	 */
	for (i = 0; i < 4; i++) {
		GLfloat x1, y1, x2, y2;
		int last_n = n;

		x1 = x[i];
		y1 = y[i];

		/* if this vertex is contained in the clip rect, use it as-is: */
		if ((cx1 <= x1) && (x1 <= cx2) &&
				(cy1 <= y1) && (y1 <= cy2))
			append_vertex(x1, y1);

		/* for remaining, we consider the point as part of a line: */
		x2 = x[(i+1) % 4];
		y2 = y[(i+1) % 4];

		if (x1 == x2) {
			append_vertex(clip(x1, cx1, cx2), clip(y1, cy1, cy2));
			append_vertex(clip(x2, cx1, cx2), clip(y2, cy1, cy2));
		} else if (y1 == y2) {
			append_vertex(clip(x1, cx1, cx2), clip(y1, cy1, cy2));
			append_vertex(clip(x2, cx1, cx2), clip(y2, cy1, cy2));
		} else {
			GLfloat m, c, p;
			GLfloat tx[2], ty[2];
			int tn = 0;

			int intersect_horiz(GLfloat y, GLfloat *p)
			{
				GLfloat x;

				/* if y does not lie between y1 and y2, no
				 * intersection possible
				 */
				if (sign(y-y1) == sign(y-y2))
					return 0;

				x = (y - c) / m;

				/* if x does not lie between cx1 and cx2, no
				 * intersection:
				 */
				if (sign(x-cx1) == sign(x-cx2))
					return 0;

				*p = x;
				return 1;
			}

			int intersect_vert(GLfloat x, GLfloat *p)
			{
				GLfloat y;

				if (sign(x-x1) == sign(x-x2))
					return 0;

				y = m * x + c;

				if (sign(y-cy1) == sign(y-cy2))
					return 0;

				*p = y;
				return 1;
			}

			/* y = mx + c */
			m = (y2 - y1) / (x2 - x1);
			c = y1 - m * x1;

			/* check for up to two intersections with the four edges
			 * of the clip rect.  Note that we don't know the orientation
			 * of the transformed surface wrt. the clip rect.  So if when
			 * there are two intersection points, we need to put the one
			 * closest to x1,y1 first:
			 */

			/* check top clip rect edge: */
			if (intersect_horiz(cy1, &p)) {
				ty[tn] = cy1;
				tx[tn] = p;
				tn++;
			}

			/* check right clip rect edge: */
			if (intersect_vert(cx2, &p)) {
				ty[tn] = p;
				tx[tn] = cx2;
				tn++;
				if (tn == 2)
					goto edge_check_done;
			}

			/* check bottom clip rect edge: */
			if (intersect_horiz(cy2, &p)) {
				ty[tn] = cy2;
				tx[tn] = p;
				tn++;
				if (tn == 2)
					goto edge_check_done;
			}

			/* check left clip rect edge: */
			if (intersect_vert(cx1, &p)) {
				ty[tn] = p;
				tx[tn] = cx1;
				tn++;
			}

edge_check_done:
			if (tn == 1) {
				append_vertex(tx[0], ty[0]);
			} else if (tn == 2) {
				if (dist_squared(x1, y1, tx[0], ty[0]) <
						dist_squared(x1, y1, tx[1], ty[1])) {
					append_vertex(tx[0], ty[0]);
					append_vertex(tx[1], ty[1]);
				} else {
					append_vertex(tx[1], ty[1]);
					append_vertex(tx[0], ty[0]);
				}
			}

			if (n == last_n) {
				GLfloat best_x=0, best_y=0;
				uint32_t d, best_d = (unsigned int)-1; /* distance squared */
				uint32_t max_d = dist_squared(x2, y2,
						x[(i+2) % 4], y[(i+2) % 4]);

				/* if there are no vertices on this line, it could be that
				 * there is a vertex of the clip rect that is enclosed by
				 * the transformed surface.  Find the vertex of the clip
				 * rect that is reached by the shortest line perpendicular
				 * to the current edge, if any.
				 *
				 * slope of perpendicular is 1/m, so
				 *
				 *   cy = -cx/m + c2
				 *   c2 = cy + cx/m
				 *
				 */

				int perp_intersect(GLfloat cx, GLfloat cy, uint32_t *d)
				{
					GLfloat c2 = cy + cx/m;
					GLfloat x = (c2 - c) / (m + 1/m);

					/* if the x position of the intersection of the
					 * perpendicular with the transformed edge does
					 * not lie within the bounds of the edge, then
					 * no intersection:
					 */
					if (sign(x-x1) == sign(x-x2))
						return 0;

					*d = dist_squared(cx, cy, x, (m * x) + c);

					/* if intersection distance is further away than
					 * opposite edge of surface region, it is invalid:
					 */
					if (*d > max_d)
						return 0;

					return 1;
				}

				if (perp_intersect(cx1, cy1, &d)) {
					best_x = cx1;
					best_y = cy1;
					best_d = d;
				}

				if (perp_intersect(cx1, cy2, &d) && (d < best_d)) {
					best_x = cx1;
					best_y = cy2;
					best_d = d;
				}

				if (perp_intersect(cx2, cy2, &d) && (d < best_d)) {
					best_x = cx2;
					best_y = cy2;
					best_d = d;
				}

				if (perp_intersect(cx2, cy1, &d) && (d < best_d)) {
					best_x = cx2;
					best_y = cy1;
					best_d = d;
				}

				if (best_d != (unsigned int)-1)  // XXX can this happen?
					append_vertex(best_x, best_y);
			}
		}

	}

	return n;
}

static int
texture_region(struct weston_surface *es, pixman_region32_t *region,
		pixman_region32_t *surf_region)
{
	struct weston_compositor *ec = es->compositor;
	GLfloat *v, inv_width, inv_height;
	unsigned int *vtxcnt, nvtx = 0;
	pixman_box32_t *rects, *surf_rects;
	int i, j, k, nrects, nsurf;

	rects = pixman_region32_rectangles(region, &nrects);
	surf_rects = pixman_region32_rectangles(surf_region, &nsurf);

	/* worst case we can have 10 vertices per rect (ie. clipped into
	 * an octagon):
	 */
	v = wl_array_add(&ec->vertices, nrects * nsurf * 10 * 4 * sizeof *v);
	vtxcnt = wl_array_add(&ec->vtxcnt, nrects * nsurf * sizeof *vtxcnt);

	inv_width = 1.0 / es->pitch;
	inv_height = 1.0 / es->geometry.height;

	for (i = 0; i < nrects; i++) {
		pixman_box32_t *rect = &rects[i];
		for (j = 0; j < nsurf; j++) {
			pixman_box32_t *surf_rect = &surf_rects[j];
			GLfloat cx, cy;
			GLfloat ex[8], ey[8];          /* edge points in screen space */
			int n;

			void emit_vertex(GLfloat gx, GLfloat gy)
			{
				GLfloat sx, sy;
				surface_from_global_float(es, gx, gy, &sx, &sy);
				/* In groups of 4 attributes, first two are 'position', 2nd two
				 * are 'texcoord'.
				 */
				*(v++) = gx;
				*(v++) = gy;
				*(v++) = sx * inv_width;
				*(v++) = sy * inv_height;
			}

			/* The transformed surface, after clipping to the clip region,
			 * can have as many as eight sides, emitted as a triangle-fan.
			 * The first vertex is the center, followed by each corner.
			 *
			 * If a corner of the transformed surface falls outside of the
			 * clip region, instead of emitting one vertex for the corner
			 * of the surface, up to two are emitted for two corresponding
			 * intersection point(s) between the surface and the clip region.
			 *
			 * To do this, we first calculate the (up to eight) points that
			 * form the intersection of the clip rect and the transformed
			 * surface. After that we calculate the average to determine
			 * the center point, and emit the center and edge vertices of
			 * the fan.
			 */
			n = calculate_edges(es, rect, surf_rect, ex, ey);
			if (n < 3)
				continue;

			/* calculate/emit center point: */
			cx = 0;
			cy = 0;
			for (k = 0; k < n; k++) {
				cx += ex[k];
				cy += ey[k];
			}
			cx /= n;
			cy /= n;
			emit_vertex(cx, cy);

			/* then emit edge points: */
			for (k = 0; k < n; k++)
				emit_vertex(ex[k], ey[k]);

			/* and close the fan: */
			emit_vertex(ex[0], ey[0]);

			vtxcnt[nvtx++] = n + 2;
		}
	}

	return nvtx;
}

static void
triangle_fan_debug(struct weston_surface *surface, int first, int count)
{
	struct weston_compositor *compositor = surface->compositor;
	int i;
	GLushort *buffer;
	GLushort *index;
	int nelems;
	static int color_idx = 0;
	static const GLfloat color[][4] = {
			{ 1.0, 0.0, 0.0, 1.0 },
			{ 0.0, 1.0, 0.0, 1.0 },
			{ 0.0, 0.0, 1.0, 1.0 },
			{ 1.0, 1.0, 1.0, 1.0 },
	};

	nelems = (count - 1 + count - 2) * 2;

	buffer = malloc(sizeof(GLushort) * nelems);
	index = buffer;

	for (i = 1; i < count; i++) {
		*index++ = first;
		*index++ = first + i;
	}

	for (i = 2; i < count; i++) {
		*index++ = first + i - 1;
		*index++ = first + i;
	}

	glUseProgram(compositor->solid_shader.program);
	glUniform4fv(compositor->solid_shader.color_uniform, 1,
			color[color_idx++ % ARRAY_SIZE(color)]);
	glDrawElements(GL_LINES, nelems, GL_UNSIGNED_SHORT, buffer);
	glUseProgram(surface->shader->program);
	free(buffer);
}

static void
repaint_region(struct weston_surface *es, pixman_region32_t *region,
		pixman_region32_t *surf_region)
{
	struct weston_compositor *ec = es->compositor;
	GLfloat *v;
	unsigned int *vtxcnt;
	int i, first, nfans;

	/* The final region to be painted is the intersection of
	 * 'region' and 'surf_region'. However, 'region' is in the global
	 * coordinates, and 'surf_region' is in the surface-local
	 * corodinates. texture_region() will iterate over all pairs of
	 * rectangles from both regions, compute the intersection
	 * polygon for each pair, and store it as a triangle fan if
	 * it has a non-zero area.
	 */
	nfans = texture_region(es, region, surf_region);

	v = ec->vertices.data;
	vtxcnt = ec->vtxcnt.data;

	/* position: */
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[0]);
	glEnableVertexAttribArray(0);

	/* texcoord: */
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[2]);
	glEnableVertexAttribArray(1);

	for (i = 0, first = 0; i < nfans; i++) {
		glDrawArrays(GL_TRIANGLE_FAN, first, vtxcnt[i]);
		if (ec->fan_debug)
			triangle_fan_debug(es, first, vtxcnt[i]);
		first += vtxcnt[i];
	}

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);

	ec->vertices.size = 0;
	ec->vtxcnt.size = 0;
}


WL_EXPORT void
weston_surface_draw(struct weston_surface *es, struct weston_output *output,
		    pixman_region32_t *damage) /* in global coordinates */
{
	struct weston_compositor *ec = es->compositor;
	/* repaint bounding region in global coordinates: */
	pixman_region32_t repaint;
	/* non-opaque region in surface coordinates: */
	pixman_region32_t surface_blend;
	GLint filter;
	int i;

	pixman_region32_init(&repaint);
	pixman_region32_init(&surface_blend);

	pixman_region32_intersect(&repaint,
				  &es->transform.boundingbox, damage);
	pixman_region32_subtract(&repaint, &repaint, &es->clip);

	if (!pixman_region32_not_empty(&repaint))
		goto out;

	pixman_region32_subtract(&ec->primary_plane.damage,
				 &ec->primary_plane.damage, &repaint);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	/* blended region is whole surface minus opaque region: */
	pixman_region32_init_rect(&surface_blend, 0, 0,
				  es->geometry.width, es->geometry.height);
	if (es->alpha >= 1.0)
		pixman_region32_subtract(&surface_blend, &surface_blend,
					 &es->opaque);

	if (ec->current_shader != es->shader) {
		glUseProgram(es->shader->program);
		ec->current_shader = es->shader;
	}

	glUniformMatrix4fv(es->shader->proj_uniform,
			   1, GL_FALSE, output->matrix.d);
	glUniform4fv(es->shader->color_uniform, 1, es->color);
	glUniform1f(es->shader->alpha_uniform, es->alpha);

	if (es->transform.enabled || output->zoom.active)
		filter = GL_LINEAR;
	else
		filter = GL_NEAREST;

	for (i = 0; i < es->num_textures; i++) {
		glUniform1i(es->shader->tex_uniforms[i], i);
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(es->target, es->textures[i]);
		glTexParameteri(es->target, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(es->target, GL_TEXTURE_MAG_FILTER, filter);
	}

	if (pixman_region32_not_empty(&es->opaque) && es->alpha >= 1.0) {
		glDisable(GL_BLEND);
		repaint_region(es, &repaint, &es->opaque);
	}

	if (pixman_region32_not_empty(&surface_blend)) {
		glEnable(GL_BLEND);
		repaint_region(es, &repaint, &surface_blend);
	}

out:
	pixman_region32_fini(&repaint);
	pixman_region32_fini(&surface_blend);
}

WL_EXPORT void
weston_surface_restack(struct weston_surface *surface, struct wl_list *below)
{
	wl_list_remove(&surface->layer_link);
	wl_list_insert(below, &surface->layer_link);
	weston_surface_damage_below(surface);
	weston_surface_damage(surface);
}

WL_EXPORT void
weston_compositor_damage_all(struct weston_compositor *compositor)
{
	struct weston_output *output;

	wl_list_for_each(output, &compositor->output_list, link)
		weston_output_damage(output);
}

WL_EXPORT void
weston_buffer_post_release(struct wl_buffer *buffer)
{
	if (--buffer->busy_count > 0)
		return;

	assert(buffer->resource.client != NULL);
	wl_resource_queue_event(&buffer->resource, WL_BUFFER_RELEASE);
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
fade_frame(struct weston_animation *animation,
	   struct weston_output *output, uint32_t msecs)
{
	struct weston_compositor *compositor =
		container_of(animation,
			     struct weston_compositor, fade.animation);
	struct weston_surface *surface;

	if (animation->frame_counter <= 1)
		compositor->fade.spring.timestamp = msecs;

	surface = compositor->fade.surface;
	weston_spring_update(&compositor->fade.spring, msecs);
	weston_surface_set_color(surface, 0.0, 0.0, 0.0,
				 compositor->fade.spring.current);
	weston_surface_damage(surface);

	if (weston_spring_done(&compositor->fade.spring)) {
		compositor->fade.spring.current =
			compositor->fade.spring.target;
		wl_list_remove(&animation->link);
		wl_list_init(&animation->link);

		if (compositor->fade.spring.current < 0.001) {
			destroy_surface(&surface->surface.resource);
			compositor->fade.surface = NULL;
		} else if (compositor->fade.spring.current > 0.999) {
			compositor->state = WESTON_COMPOSITOR_SLEEPING;
			wl_signal_emit(&compositor->lock_signal, compositor);
		}
	}
}

static void
update_shm_texture(struct weston_surface *surface)
{
#ifdef GL_UNPACK_ROW_LENGTH
	pixman_box32_t *rectangles;
	void *data;
	int i, n;
#endif

	glBindTexture(GL_TEXTURE_2D, surface->textures[0]);

	if (!surface->compositor->has_unpack_subimage) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     surface->pitch, surface->buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			     wl_shm_buffer_get_data(surface->buffer));

		return;
	}

#ifdef GL_UNPACK_ROW_LENGTH
	/* Mesa does not define GL_EXT_unpack_subimage */
	glPixelStorei(GL_UNPACK_ROW_LENGTH, surface->pitch);
	data = wl_shm_buffer_get_data(surface->buffer);
	rectangles = pixman_region32_rectangles(&surface->damage, &n);
	for (i = 0; i < n; i++) {
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, rectangles[i].x1);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, rectangles[i].y1);
		glTexSubImage2D(GL_TEXTURE_2D, 0,
				rectangles[i].x1, rectangles[i].y1,
				rectangles[i].x2 - rectangles[i].x1,
				rectangles[i].y2 - rectangles[i].y1,
				GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
	}
#endif
}

static void
surface_accumulate_damage(struct weston_surface *surface,
			  pixman_region32_t *opaque)
{
	if (surface->buffer && wl_buffer_is_shm(surface->buffer))
		update_shm_texture(surface);

	if (surface->transform.enabled) {
		pixman_box32_t *extents;

		extents = pixman_region32_extents(&surface->damage);
		surface_compute_bbox(surface, extents->x1, extents->y1,
				     extents->x2 - extents->x1,
				     extents->y2 - extents->y1,
				     &surface->damage);
		pixman_region32_translate(&surface->damage,
					  -surface->plane->x,
					  -surface->plane->y);
	} else {
		pixman_region32_translate(&surface->damage,
					  surface->geometry.x - surface->plane->x,
					  surface->geometry.y - surface->plane->y);
	}

	pixman_region32_subtract(&surface->damage, &surface->damage, opaque);
	pixman_region32_union(&surface->plane->damage,
			      &surface->plane->damage, &surface->damage);
	empty_region(&surface->damage);
	pixman_region32_copy(&surface->clip, opaque);
	pixman_region32_union(opaque, opaque, &surface->transform.opaque);
}

static void
weston_output_repaint(struct weston_output *output, uint32_t msecs)
{
	struct weston_compositor *ec = output->compositor;
	struct weston_surface *es;
	struct weston_layer *layer;
	struct weston_animation *animation, *next;
	struct weston_frame_callback *cb, *cnext;
	struct wl_list frame_callback_list;
	pixman_region32_t opaque, output_damage, new_damage;
	int32_t width, height;

	weston_compositor_update_drag_surfaces(ec);

	width = output->current->width +
		output->border.left + output->border.right;
	height = output->current->height +
		output->border.top + output->border.bottom;

	glViewport(0, 0, width, height);

	/* Rebuild the surface list and update surface transforms up front. */
	wl_list_init(&ec->surface_list);
	wl_list_init(&frame_callback_list);
	wl_list_for_each(layer, &ec->layer_list, link) {
		wl_list_for_each(es, &layer->surface_list, layer_link) {
			weston_surface_update_transform(es);
			wl_list_insert(ec->surface_list.prev, &es->link);
			if (es->output == output) {
				wl_list_insert_list(&frame_callback_list,
						    &es->frame_callback_list);
				wl_list_init(&es->frame_callback_list);
			}
		}
	}

	if (output->assign_planes && !output->disable_planes)
		output->assign_planes(output);
	else
		wl_list_for_each(es, &ec->surface_list, link)
			weston_surface_move_to_plane(es, &ec->primary_plane);

	pixman_region32_init(&opaque);

	wl_list_for_each(es, &ec->surface_list, link)
		surface_accumulate_damage(es, &opaque);

	pixman_region32_fini(&opaque);

	pixman_region32_init(&output_damage);

	pixman_region32_init(&new_damage);
	pixman_region32_copy(&new_damage, &ec->primary_plane.damage);

	pixman_region32_union(&ec->primary_plane.damage,
			      &ec->primary_plane.damage,
			      &output->previous_damage);

	pixman_region32_intersect(&output->previous_damage,
				  &new_damage, &output->region);

	pixman_region32_intersect(&output_damage,
				  &ec->primary_plane.damage, &output->region);

	pixman_region32_fini(&new_damage);

	if (output->dirty)
		weston_output_update_matrix(output);

	/* if debugging, redraw everything outside the damage to clean up
	 * debug lines from the previous draw on this buffer:
	 */
	if (ec->fan_debug) {
		pixman_region32_t undamaged;
		pixman_region32_init(&undamaged);
		pixman_region32_subtract(&undamaged, &output->region,
				&output_damage);
		ec->fan_debug = 0;
		output->repaint(output, &undamaged, 0);
		ec->fan_debug = 1;
		pixman_region32_fini(&undamaged);
	}

	output->repaint(output, &output_damage, 1);

	pixman_region32_fini(&output_damage);

	output->repaint_needed = 0;

	weston_compositor_repick(ec);
	wl_event_loop_dispatch(ec->input_loop, 0);

	wl_list_for_each_safe(cb, cnext, &frame_callback_list, link) {
		wl_callback_send_done(&cb->resource, msecs);
		wl_resource_destroy(&cb->resource);
	}
	wl_list_init(&frame_callback_list);

	wl_list_for_each_safe(animation, next, &output->animation_list, link) {
		animation->frame_counter++;
		animation->frame(animation, output, msecs);
	}
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
	int fd;

	output->frame_time = msecs;
	if (output->repaint_needed) {
		weston_output_repaint(output, msecs);
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

	weston_output_finish_frame(output, weston_compositor_get_time());
}

WL_EXPORT void
weston_layer_init(struct weston_layer *layer, struct wl_list *below)
{
	wl_list_init(&layer->surface_list);
	if (below != NULL)
		wl_list_insert(below, &layer->link);
}

WL_EXPORT void
weston_output_schedule_repaint(struct weston_output *output)
{
	struct weston_compositor *compositor = output->compositor;
	struct wl_event_loop *loop;

	if (compositor->state == WESTON_COMPOSITOR_SLEEPING)
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

WL_EXPORT void
weston_compositor_fade(struct weston_compositor *compositor, float tint)
{
	struct weston_output *output;
	struct weston_surface *surface;

	output = container_of(compositor->output_list.next,
                             struct weston_output, link);

	compositor->fade.spring.target = tint;
	if (weston_spring_done(&compositor->fade.spring))
		return;

	if (compositor->fade.surface == NULL) {
		surface = weston_surface_create(compositor);
		weston_surface_configure(surface, 0, 0, 8192, 8192);
		weston_surface_set_color(surface, 0.0, 0.0, 0.0, 0.0);
		wl_list_insert(&compositor->fade_layer.surface_list,
			       &surface->layer_link);
		weston_surface_assign_output(surface);
		compositor->fade.surface = surface;
		pixman_region32_init(&surface->input);
	}

	weston_surface_damage(compositor->fade.surface);
	if (wl_list_empty(&compositor->fade.animation.link)) {
		compositor->fade.animation.frame_counter = 0;
		wl_list_insert(output->animation_list.prev,
			       &compositor->fade.animation.link);
	}
}

static void
surface_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static struct wl_resource *
find_resource_for_client(struct wl_list *list, struct wl_client *client)
{
        struct wl_resource *r;

        wl_list_for_each(r, list, link) {
                if (r->client == client)
                        return r;
        }

        return NULL;
}

static void
weston_surface_update_output_mask(struct weston_surface *es, uint32_t mask)
{
	uint32_t different = es->output_mask ^ mask;
	uint32_t entered = mask & different;
	uint32_t left = es->output_mask & different;
	struct weston_output *output;
	struct wl_resource *resource = NULL;
	struct wl_client *client = es->surface.resource.client;

	es->output_mask = mask;
	if (es->surface.resource.client == NULL)
		return;
	if (different == 0)
		return;

	wl_list_for_each(output, &es->compositor->output_list, link) {
		if (1 << output->id & different)
			resource =
				find_resource_for_client(&output->resource_list,
							 client);
		if (resource == NULL)
			continue;
		if (1 << output->id & entered)
			wl_surface_send_enter(&es->surface.resource, resource);
		if (1 << output->id & left)
			wl_surface_send_leave(&es->surface.resource, resource);
	}
}

WL_EXPORT void
weston_surface_assign_output(struct weston_surface *es)
{
	struct weston_compositor *ec = es->compositor;
	struct weston_output *output, *new_output;
	pixman_region32_t region;
	uint32_t max, area, mask;
	pixman_box32_t *e;

	new_output = NULL;
	max = 0;
	mask = 0;
	pixman_region32_init(&region);
	wl_list_for_each(output, &ec->output_list, link) {
		pixman_region32_intersect(&region, &es->transform.boundingbox,
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

	es->output = new_output;
	weston_surface_update_output_mask(es, mask);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_resource *resource,
	       struct wl_resource *buffer_resource, int32_t sx, int32_t sy)
{
	struct weston_surface *es = resource->data;
	struct wl_buffer *buffer = NULL;

	if (buffer_resource)
		buffer = buffer_resource->data;

	weston_surface_attach(&es->surface, buffer);

	if (buffer && es->configure)
		es->configure(es, sx, sy);
}

static void
surface_damage(struct wl_client *client,
	       struct wl_resource *resource,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct weston_surface *es = resource->data;

	pixman_region32_union_rect(&es->damage, &es->damage,
				   x, y, width, height);
	weston_surface_schedule_repaint(es);
}

static void
destroy_frame_callback(struct wl_resource *resource)
{
	struct weston_frame_callback *cb = resource->data;

	wl_list_remove(&cb->link);
	free(cb);
}

static void
surface_frame(struct wl_client *client,
	      struct wl_resource *resource, uint32_t callback)
{
	struct weston_frame_callback *cb;
	struct weston_surface *es = resource->data;

	cb = malloc(sizeof *cb);
	if (cb == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}
		
	cb->resource.object.interface = &wl_callback_interface;
	cb->resource.object.id = callback;
	cb->resource.destroy = destroy_frame_callback;
	cb->resource.client = client;
	cb->resource.data = cb;

	wl_client_add_resource(client, &cb->resource);
	wl_list_insert(es->frame_callback_list.prev, &cb->link);
}

static void
surface_set_opaque_region(struct wl_client *client,
			  struct wl_resource *resource,
			  struct wl_resource *region_resource)
{
	struct weston_surface *surface = resource->data;
	struct weston_region *region;

	pixman_region32_fini(&surface->opaque);

	if (region_resource) {
		region = region_resource->data;
		pixman_region32_init_rect(&surface->opaque, 0, 0,
					  surface->geometry.width,
					  surface->geometry.height);
		pixman_region32_intersect(&surface->opaque,
					  &surface->opaque, &region->region);
	} else {
		pixman_region32_init(&surface->opaque);
	}

	surface->geometry.dirty = 1;
}

static void
surface_set_input_region(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *region_resource)
{
	struct weston_surface *surface = resource->data;
	struct weston_region *region;

	if (region_resource) {
		region = region_resource->data;
		pixman_region32_init_rect(&surface->input, 0, 0,
					  surface->geometry.width,
					  surface->geometry.height);
		pixman_region32_intersect(&surface->input,
					  &surface->input, &region->region);
	} else {
		pixman_region32_init_rect(&surface->input, 0, 0,
					  surface->geometry.width,
					  surface->geometry.height);
	}

	weston_surface_schedule_repaint(surface);
}

static const struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_damage,
	surface_frame,
	surface_set_opaque_region,
	surface_set_input_region
};

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_resource *resource, uint32_t id)
{
	struct weston_compositor *ec = resource->data;
	struct weston_surface *surface;

	surface = weston_surface_create(ec);
	if (surface == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	surface->surface.resource.destroy = destroy_surface;

	surface->surface.resource.object.id = id;
	surface->surface.resource.object.interface = &wl_surface_interface;
	surface->surface.resource.object.implementation =
		(void (**)(void)) &surface_interface;
	surface->surface.resource.data = surface;

	wl_client_add_resource(client, &surface->surface.resource);
}

static void
destroy_region(struct wl_resource *resource)
{
	struct weston_region *region =
		container_of(resource, struct weston_region, resource);

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
	struct weston_region *region = resource->data;

	pixman_region32_union_rect(&region->region, &region->region,
				   x, y, width, height);
}

static void
region_subtract(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct weston_region *region = resource->data;
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

	region->resource.destroy = destroy_region;

	region->resource.object.id = id;
	region->resource.object.interface = &wl_region_interface;
	region->resource.object.implementation =
		(void (**)(void)) &region_interface;
	region->resource.data = region;

	pixman_region32_init(&region->region);

	wl_client_add_resource(client, &region->resource);
}

static const struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
	compositor_create_region
};

WL_EXPORT void
weston_compositor_wake(struct weston_compositor *compositor)
{
	compositor->state = WESTON_COMPOSITOR_ACTIVE;
	weston_compositor_fade(compositor, 0.0);

	wl_event_source_timer_update(compositor->idle_source,
				     compositor->idle_time * 1000);
}

static void
weston_compositor_dpms_on(struct weston_compositor *compositor)
{
        struct weston_output *output;

        wl_list_for_each(output, &compositor->output_list, link)
		if (output->set_dpms)
			output->set_dpms(output, WESTON_DPMS_ON);
}

WL_EXPORT void
weston_compositor_activity(struct weston_compositor *compositor)
{
	if (compositor->state == WESTON_COMPOSITOR_ACTIVE) {
		weston_compositor_wake(compositor);
	} else {
		weston_compositor_dpms_on(compositor);
		wl_signal_emit(&compositor->unlock_signal, compositor);
	}
}

static void
weston_compositor_idle_inhibit(struct weston_compositor *compositor)
{
	weston_compositor_activity(compositor);
	compositor->idle_inhibit++;
}

static void
weston_compositor_idle_release(struct weston_compositor *compositor)
{
	compositor->idle_inhibit--;
	weston_compositor_activity(compositor);
}

static int
idle_handler(void *data)
{
	struct weston_compositor *compositor = data;

	if (compositor->idle_inhibit)
		return 1;

	weston_compositor_fade(compositor, 1.0);

	return 1;
}

WL_EXPORT void
weston_plane_init(struct weston_plane *plane, int32_t x, int32_t y)
{
	pixman_region32_init(&plane->damage);
	plane->x = x;
	plane->y = y;
}

WL_EXPORT void
weston_plane_release(struct weston_plane *plane)
{
	pixman_region32_fini(&plane->damage);
}

static  void
weston_seat_update_drag_surface(struct weston_seat *seat, int dx, int dy);

static void
clip_pointer_motion(struct weston_seat *seat, wl_fixed_t *fx, wl_fixed_t *fy)
{
	struct weston_compositor *ec = seat->compositor;
	struct weston_output *output, *prev = NULL;
	int x, y, old_x, old_y, valid = 0;

	x = wl_fixed_to_int(*fx);
	y = wl_fixed_to_int(*fy);
	old_x = wl_fixed_to_int(seat->seat.pointer->x);
	old_y = wl_fixed_to_int(seat->seat.pointer->y);

	wl_list_for_each(output, &ec->output_list, link) {
		if (pixman_region32_contains_point(&output->region,
						   x, y, NULL))
			valid = 1;
		if (pixman_region32_contains_point(&output->region,
						   old_x, old_y, NULL))
			prev = output;
	}

	if (!valid) {
		if (x < prev->x)
			*fx = wl_fixed_from_int(prev->x);
		else if (x >= prev->x + prev->width)
			*fx = wl_fixed_from_int(prev->x +
						prev->width - 1);
		if (y < prev->y)
			*fy = wl_fixed_from_int(prev->y);
		else if (y >= prev->y + prev->current->height)
			*fy = wl_fixed_from_int(prev->y +
						prev->height - 1);
	}
}

WL_EXPORT void
notify_motion(struct weston_seat *seat, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	const struct wl_pointer_grab_interface *interface;
	struct weston_compositor *ec = seat->compositor;
	struct weston_output *output;
	struct wl_pointer *pointer = seat->seat.pointer;
	int32_t ix, iy;

	weston_compositor_activity(ec);

	clip_pointer_motion(seat, &x, &y);

	weston_seat_update_drag_surface(seat, x - pointer->x, y - pointer->y);

	pointer->x = x;
	pointer->y = y;

	ix = wl_fixed_to_int(x);
	iy = wl_fixed_to_int(y);

	wl_list_for_each(output, &ec->output_list, link)
		if (output->zoom.active &&
		    pixman_region32_contains_point(&output->region,
						   ix, iy, NULL))
			weston_output_update_zoom(output, ZOOM_FOCUS_POINTER);

	weston_device_repick(seat);
	interface = pointer->grab->interface;
	interface->motion(pointer->grab, time,
			  pointer->grab->x, pointer->grab->y);

	if (seat->sprite) {
		weston_surface_set_position(seat->sprite,
					    ix - seat->hotspot_x,
					    iy - seat->hotspot_y);
		weston_surface_schedule_repaint(seat->sprite);
	}
}

WL_EXPORT void
weston_surface_activate(struct weston_surface *surface,
			struct weston_seat *seat)
{
	struct weston_compositor *compositor = seat->compositor;

	if (seat->seat.keyboard) {
		wl_keyboard_set_focus(seat->seat.keyboard, &surface->surface);
		wl_data_device_set_keyboard_focus(&seat->seat);
	}

	wl_signal_emit(&compositor->activate_signal, surface);
}

WL_EXPORT void
notify_button(struct weston_seat *seat, uint32_t time, int32_t button,
	      enum wl_pointer_button_state state)
{
	struct weston_compositor *compositor = seat->compositor;
	struct wl_pointer *pointer = seat->seat.pointer;
	struct weston_surface *focus =
		(struct weston_surface *) pointer->focus;
	uint32_t serial = wl_display_next_serial(compositor->wl_display);

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (compositor->ping_handler && focus)
			compositor->ping_handler(focus, serial);
		weston_compositor_idle_inhibit(compositor);
		if (pointer->button_count == 0) {
			pointer->grab_button = button;
			pointer->grab_time = time;
			pointer->grab_x = pointer->x;
			pointer->grab_y = pointer->y;
		}
		pointer->button_count++;
	} else {
		weston_compositor_idle_release(compositor);
		pointer->button_count--;
	}

	weston_compositor_run_button_binding(compositor, seat, time, button,
					     state);

	pointer->grab->interface->button(pointer->grab, time, button, state);

	if (pointer->button_count == 1)
		pointer->grab_serial =
			wl_display_get_serial(compositor->wl_display);
}

WL_EXPORT void
notify_axis(struct weston_seat *seat, uint32_t time, uint32_t axis,
	    wl_fixed_t value)
{
	struct weston_compositor *compositor = seat->compositor;
	struct wl_pointer *pointer = seat->seat.pointer;
	struct weston_surface *focus =
		(struct weston_surface *) pointer->focus;
	uint32_t serial = wl_display_next_serial(compositor->wl_display);

	if (compositor->ping_handler && focus)
		compositor->ping_handler(focus, serial);

	weston_compositor_activity(compositor);

	if (value)
		weston_compositor_run_axis_binding(compositor, seat,
						   time, axis, value);
	else
		return;

	if (pointer->focus_resource)
		wl_pointer_send_axis(pointer->focus_resource, time, axis,
				     value);
}

WL_EXPORT void
notify_modifiers(struct weston_seat *seat, uint32_t serial)
{
	struct wl_keyboard *keyboard = &seat->keyboard;
	struct wl_keyboard_grab *grab = keyboard->grab;
	uint32_t mods_depressed, mods_latched, mods_locked, group;
	uint32_t mods_lookup;
	enum weston_led leds = 0;
	int changed = 0;

	/* Serialize and update our internal state, checking to see if it's
	 * different to the previous state. */
	mods_depressed = xkb_state_serialize_mods(seat->xkb_state.state,
						  XKB_STATE_DEPRESSED);
	mods_latched = xkb_state_serialize_mods(seat->xkb_state.state,
						XKB_STATE_LATCHED);
	mods_locked = xkb_state_serialize_mods(seat->xkb_state.state,
					       XKB_STATE_LOCKED);
	group = xkb_state_serialize_group(seat->xkb_state.state,
					  XKB_STATE_EFFECTIVE);

	if (mods_depressed != seat->seat.keyboard->modifiers.mods_depressed ||
	    mods_latched != seat->seat.keyboard->modifiers.mods_latched ||
	    mods_locked != seat->seat.keyboard->modifiers.mods_locked ||
	    group != seat->seat.keyboard->modifiers.group)
		changed = 1;

	seat->seat.keyboard->modifiers.mods_depressed = mods_depressed;
	seat->seat.keyboard->modifiers.mods_latched = mods_latched;
	seat->seat.keyboard->modifiers.mods_locked = mods_locked;
	seat->seat.keyboard->modifiers.group = group;

	/* And update the modifier_state for bindings. */
	mods_lookup = mods_depressed | mods_latched;
	seat->modifier_state = 0;
	if (mods_lookup & (1 << seat->xkb_info.ctrl_mod))
		seat->modifier_state |= MODIFIER_CTRL;
	if (mods_lookup & (1 << seat->xkb_info.alt_mod))
		seat->modifier_state |= MODIFIER_ALT;
	if (mods_lookup & (1 << seat->xkb_info.super_mod))
		seat->modifier_state |= MODIFIER_SUPER;
	if (mods_lookup & (1 << seat->xkb_info.shift_mod))
		seat->modifier_state |= MODIFIER_SHIFT;

	/* Finally, notify the compositor that LEDs have changed. */
	if (xkb_state_led_index_is_active(seat->xkb_state.state,
					  seat->xkb_info.num_led))
		leds |= LED_NUM_LOCK;
	if (xkb_state_led_index_is_active(seat->xkb_state.state,
					  seat->xkb_info.caps_led))
		leds |= LED_CAPS_LOCK;
	if (xkb_state_led_index_is_active(seat->xkb_state.state,
					  seat->xkb_info.scroll_led))
		leds |= LED_SCROLL_LOCK;
	if (leds != seat->xkb_state.leds && seat->led_update)
		seat->led_update(seat, leds);
	seat->xkb_state.leds = leds;

	if (changed) {
		grab->interface->modifiers(grab,
					   serial,
					   keyboard->modifiers.mods_depressed,
					   keyboard->modifiers.mods_latched,
					   keyboard->modifiers.mods_locked,
					   keyboard->modifiers.group);
	}
}

static void
update_modifier_state(struct weston_seat *seat, uint32_t serial, uint32_t key,
		      enum wl_keyboard_key_state state)
{
	enum xkb_key_direction direction;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
		direction = XKB_KEY_DOWN;
	else
		direction = XKB_KEY_UP;

	/* Offset the keycode by 8, as the evdev XKB rules reflect X's
	 * broken keycode system, which starts at 8. */
	xkb_state_update_key(seat->xkb_state.state, key + 8, direction);

	notify_modifiers(seat, serial);
}

WL_EXPORT void
notify_key(struct weston_seat *seat, uint32_t time, uint32_t key,
	   enum wl_keyboard_key_state state,
	   enum weston_key_state_update update_state)
{
	struct weston_compositor *compositor = seat->compositor;
	struct wl_keyboard *keyboard = seat->seat.keyboard;
	struct weston_surface *focus =
		(struct weston_surface *) keyboard->focus;
	struct wl_keyboard_grab *grab = keyboard->grab;
	uint32_t serial = wl_display_next_serial(compositor->wl_display);
	uint32_t *k, *end;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		if (compositor->ping_handler && focus)
			compositor->ping_handler(focus, serial);

		weston_compositor_idle_inhibit(compositor);
		keyboard->grab_key = key;
		keyboard->grab_time = time;
	} else {
		weston_compositor_idle_release(compositor);
	}

	end = keyboard->keys.data + keyboard->keys.size;
	for (k = keyboard->keys.data; k < end; k++) {
		if (*k == key) {
			/* Ignore server-generated repeats. */
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				return;
			*k = *--end;
		}
	}
	keyboard->keys.size = (void *) end - keyboard->keys.data;
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		k = wl_array_add(&keyboard->keys, sizeof *k);
		*k = key;
	}

	if (grab == &keyboard->default_grab) {
		weston_compositor_run_key_binding(compositor, seat, time, key,
						  state);
		grab = keyboard->grab;
	}

	grab->interface->key(grab, time, key, state);

	if (update_state == STATE_UPDATE_AUTOMATIC) {
		update_modifier_state(seat,
				      wl_display_get_serial(compositor->wl_display),
				      key,
				      state);
	}
}

WL_EXPORT void
notify_pointer_focus(struct weston_seat *seat, struct weston_output *output,
		     wl_fixed_t x, wl_fixed_t y)
{
	struct weston_compositor *compositor = seat->compositor;
	struct wl_pointer *pointer = seat->seat.pointer;

	if (output) {
		weston_seat_update_drag_surface(seat,
						x - pointer->x,
						y - pointer->y);

		pointer->x = x;
		pointer->y = y;
		compositor->focus = 1;
		weston_compositor_repick(compositor);
	} else {
		compositor->focus = 0;
		/* FIXME: We should call wl_pointer_set_focus(seat,
		 * NULL) here, but somehow that breaks re-entry... */
	}
}

static void
destroy_device_saved_kbd_focus(struct wl_listener *listener, void *data)
{
	struct weston_seat *ws;

	ws = container_of(listener, struct weston_seat,
			  saved_kbd_focus_listener);

	ws->saved_kbd_focus = NULL;
}

WL_EXPORT void
notify_keyboard_focus_in(struct weston_seat *seat, struct wl_array *keys,
			 enum weston_key_state_update update_state)
{
	struct weston_compositor *compositor = seat->compositor;
	struct wl_keyboard *keyboard = seat->seat.keyboard;
	struct wl_surface *surface;
	uint32_t *k, serial;

	serial = wl_display_next_serial(compositor->wl_display);
	wl_array_copy(&keyboard->keys, keys);
	wl_array_for_each(k, &keyboard->keys) {
		weston_compositor_idle_inhibit(compositor);
		if (update_state == STATE_UPDATE_AUTOMATIC)
			update_modifier_state(seat, serial, *k,
					      WL_KEYBOARD_KEY_STATE_PRESSED);
	}

	/* Run key bindings after we've updated the state. */
	wl_array_for_each(k, &keyboard->keys) {
		weston_compositor_run_key_binding(compositor, seat, 0, *k,
						  WL_KEYBOARD_KEY_STATE_PRESSED);
	}

	surface = seat->saved_kbd_focus;

	if (surface) {
		wl_list_remove(&seat->saved_kbd_focus_listener.link);
		wl_keyboard_set_focus(keyboard, surface);
		seat->saved_kbd_focus = NULL;
	}
}

WL_EXPORT void
notify_keyboard_focus_out(struct weston_seat *seat)
{
	struct weston_compositor *compositor = seat->compositor;
	struct wl_keyboard *keyboard = seat->seat.keyboard;
	uint32_t *k, serial;

	serial = wl_display_next_serial(compositor->wl_display);
	wl_array_for_each(k, &keyboard->keys) {
		weston_compositor_idle_release(compositor);
		update_modifier_state(seat, serial, *k,
				      WL_KEYBOARD_KEY_STATE_RELEASED);
	}

	seat->modifier_state = 0;

	if (keyboard->focus) {
		seat->saved_kbd_focus = keyboard->focus;
		seat->saved_kbd_focus_listener.notify =
			destroy_device_saved_kbd_focus;
		wl_signal_add(&keyboard->focus->resource.destroy_signal,
			      &seat->saved_kbd_focus_listener);
	}

	wl_keyboard_set_focus(keyboard, NULL);
	/* FIXME: We really need keyboard grab cancel here to
	 * let the grab shut down properly.  As it is we leak
	 * the grab data. */
	wl_keyboard_end_grab(keyboard);
}

static void
touch_set_focus(struct weston_seat *ws, struct wl_surface *surface)
{
	struct wl_seat *seat = &ws->seat;
	struct wl_resource *resource;

	if (seat->touch->focus == surface)
		return;

	if (seat->touch->focus_resource)
		wl_list_remove(&seat->touch->focus_listener.link);
	seat->touch->focus = NULL;
	seat->touch->focus_resource = NULL;

	if (surface) {
		resource =
			find_resource_for_client(&seat->touch->resource_list,
						 surface->resource.client);
		if (!resource) {
			weston_log("couldn't find resource\n");
			return;
		}

		seat->touch->focus = surface;
		seat->touch->focus_resource = resource;
		wl_signal_add(&resource->destroy_signal,
			      &seat->touch->focus_listener);
	}
}

/**
 * notify_touch - emulates button touches and notifies surfaces accordingly.
 *
 * It assumes always the correct cycle sequence until it gets here: touch_down
 * → touch_update → ... → touch_update → touch_end. The driver is responsible
 * for sending along such order.
 *
 */
WL_EXPORT void
notify_touch(struct weston_seat *seat, uint32_t time, int touch_id,
             wl_fixed_t x, wl_fixed_t y, int touch_type)
{
	struct weston_compositor *ec = seat->compositor;
	struct wl_touch *touch = seat->seat.touch;
	struct weston_surface *es;
	wl_fixed_t sx, sy;
	uint32_t serial = 0;

	switch (touch_type) {
	case WL_TOUCH_DOWN:
		weston_compositor_idle_inhibit(ec);

		seat->num_tp++;

		/* the first finger down picks the surface, and all further go
		 * to that surface for the remainder of the touch session i.e.
		 * until all touch points are up again. */
		if (seat->num_tp == 1) {
			es = weston_compositor_pick_surface(ec, x, y, &sx, &sy);
			touch_set_focus(seat, &es->surface);
		} else if (touch->focus) {
			es = (struct weston_surface *) touch->focus;
			weston_surface_from_global_fixed(es, x, y, &sx, &sy);
		}

		if (touch->focus_resource && touch->focus)
			wl_touch_send_down(touch->focus_resource,
					   serial, time,
					   &touch->focus->resource,
					   touch_id, sx, sy);
		break;
	case WL_TOUCH_MOTION:
		es = (struct weston_surface *) touch->focus;
		if (!es)
			break;

		weston_surface_from_global_fixed(es, x, y, &sx, &sy);
		if (touch->focus_resource)
			wl_touch_send_motion(touch->focus_resource,
					     time, touch_id, sx, sy);
		break;
	case WL_TOUCH_UP:
		weston_compositor_idle_release(ec);
		seat->num_tp--;

		if (touch->focus_resource)
			wl_touch_send_up(touch->focus_resource,
					 serial, time, touch_id);
		if (seat->num_tp == 0)
			touch_set_focus(seat, NULL);
		break;
	}
}

static void
pointer_handle_sprite_destroy(struct wl_listener *listener, void *data)
{
	struct weston_seat *seat = container_of(listener, struct weston_seat,
						sprite_destroy_listener);

	seat->sprite = NULL;
}

static void
pointer_cursor_surface_configure(struct weston_surface *es,
				 int32_t dx, int32_t dy)
{
	struct weston_seat *seat = es->private;
	int x, y;

	assert(es == seat->sprite);

	seat->hotspot_x -= dx;
	seat->hotspot_y -= dy;

	x = wl_fixed_to_int(seat->seat.pointer->x) - seat->hotspot_x;
	y = wl_fixed_to_int(seat->seat.pointer->y) - seat->hotspot_y;

	weston_surface_configure(seat->sprite, x, y,
				 es->buffer->width, es->buffer->height);

	empty_region(&es->input);

	if (!weston_surface_is_mapped(es)) {
		wl_list_insert(&es->compositor->cursor_layer.surface_list,
			       &es->layer_link);
		weston_surface_assign_output(es);
	}
}

static void
pointer_unmap_sprite(struct weston_seat *seat)
{
	if (weston_surface_is_mapped(seat->sprite))
		weston_surface_unmap(seat->sprite);

	wl_list_remove(&seat->sprite_destroy_listener.link);
	seat->sprite->configure = NULL;
	seat->sprite->private = NULL;
	seat->sprite = NULL;
}

static void
pointer_set_cursor(struct wl_client *client, struct wl_resource *resource,
		   uint32_t serial, struct wl_resource *surface_resource,
		   int32_t x, int32_t y)
{
	struct weston_seat *seat = resource->data;
	struct weston_surface *surface = NULL;

	if (surface_resource)
		surface = container_of(surface_resource->data,
				       struct weston_surface, surface);

	if (seat->seat.pointer->focus == NULL)
		return;
	if (seat->seat.pointer->focus->resource.client != client)
		return;
	if (seat->seat.pointer->focus_serial - serial > UINT32_MAX / 2)
		return;

	if (surface && surface != seat->sprite) {
		if (surface->configure) {
			wl_resource_post_error(&surface->surface.resource,
					       WL_DISPLAY_ERROR_INVALID_OBJECT,
					       "surface->configure already "
					       "set");
			return;
		}
	}

	if (seat->sprite)
		pointer_unmap_sprite(seat);

	if (!surface)
		return;

	wl_signal_add(&surface->surface.resource.destroy_signal,
		      &seat->sprite_destroy_listener);

	surface->configure = pointer_cursor_surface_configure;
	surface->private = seat;
	seat->sprite = surface;
	seat->hotspot_x = x;
	seat->hotspot_y = y;

	if (surface->buffer)
		pointer_cursor_surface_configure(surface, 0, 0);
}

static const struct wl_pointer_interface pointer_interface = {
	pointer_set_cursor
};

static void
handle_drag_surface_destroy(struct wl_listener *listener, void *data)
{
	struct weston_seat *seat;

	seat = container_of(listener, struct weston_seat,
			    drag_surface_destroy_listener);

	seat->drag_surface = NULL;
}

static void unbind_resource(struct wl_resource *resource)
{
	wl_list_remove(&resource->link);
	free(resource);
}

static void
seat_get_pointer(struct wl_client *client, struct wl_resource *resource,
		 uint32_t id)
{
	struct weston_seat *seat = resource->data;
	struct wl_resource *cr;

	if (!seat->seat.pointer)
		return;

        cr = wl_client_add_object(client, &wl_pointer_interface,
				  &pointer_interface, id, seat);
	wl_list_insert(&seat->seat.pointer->resource_list, &cr->link);
	cr->destroy = unbind_resource;

	if (seat->seat.pointer->focus &&
	    seat->seat.pointer->focus->resource.client == client) {
		struct weston_surface *surface;
		wl_fixed_t sx, sy;

		surface = (struct weston_surface *) seat->seat.pointer->focus;
		weston_surface_from_global_fixed(surface,
						 seat->seat.pointer->x,
						 seat->seat.pointer->y,
						 &sx,
						 &sy);
		wl_pointer_set_focus(seat->seat.pointer,
				     seat->seat.pointer->focus,
				     sx,
				     sy);
	}
}

static void
seat_get_keyboard(struct wl_client *client, struct wl_resource *resource,
		  uint32_t id)
{
	struct weston_seat *seat = resource->data;
	struct wl_resource *cr;

	if (!seat->seat.keyboard)
		return;

        cr = wl_client_add_object(client, &wl_keyboard_interface, NULL, id,
				  seat);
	wl_list_insert(&seat->seat.keyboard->resource_list, &cr->link);
	cr->destroy = unbind_resource;

	wl_keyboard_send_keymap(cr, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
				seat->xkb_info.keymap_fd,
				seat->xkb_info.keymap_size);

	if (seat->seat.keyboard->focus &&
	    seat->seat.keyboard->focus->resource.client == client) {
		wl_keyboard_set_focus(seat->seat.keyboard,
				      seat->seat.keyboard->focus);
	}
}

static void
seat_get_touch(struct wl_client *client, struct wl_resource *resource,
	       uint32_t id)
{
	struct weston_seat *seat = resource->data;
	struct wl_resource *cr;

	if (!seat->seat.touch)
		return;

        cr = wl_client_add_object(client, &wl_touch_interface, NULL, id, seat);
	wl_list_insert(&seat->seat.touch->resource_list, &cr->link);
	cr->destroy = unbind_resource;
}

static const struct wl_seat_interface seat_interface = {
	seat_get_pointer,
	seat_get_keyboard,
	seat_get_touch,
};

static void
bind_seat(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_seat *seat = data;
	struct wl_resource *resource;
	enum wl_seat_capability caps = 0;

	resource = wl_client_add_object(client, &wl_seat_interface,
					&seat_interface, id, data);
	wl_list_insert(&seat->base_resource_list, &resource->link);
	resource->destroy = unbind_resource;

	if (seat->pointer)
		caps |= WL_SEAT_CAPABILITY_POINTER;
	if (seat->keyboard)
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	if (seat->touch)
		caps |= WL_SEAT_CAPABILITY_TOUCH;

	wl_seat_send_capabilities(resource, caps);
}

static void
device_handle_new_drag_icon(struct wl_listener *listener, void *data)
{
	struct weston_seat *seat;

	seat = container_of(listener, struct weston_seat,
			    new_drag_icon_listener);

	weston_seat_update_drag_surface(seat, 0, 0);
}

static void weston_compositor_xkb_init(struct weston_compositor *ec,
				       struct xkb_rule_names *names)
{
	if (ec->xkb_context == NULL) {
		ec->xkb_context = xkb_context_new(0);
		if (ec->xkb_context == NULL) {
			weston_log("failed to create XKB context\n");
			exit(1);
		}
	}

	if (names)
		ec->xkb_names = *names;
	if (!ec->xkb_names.rules)
		ec->xkb_names.rules = strdup("evdev");
	if (!ec->xkb_names.model)
		ec->xkb_names.model = strdup("pc105");
	if (!ec->xkb_names.layout)
		ec->xkb_names.layout = strdup("us");
}

static void xkb_info_destroy(struct weston_xkb_info *xkb_info)
{
	if (xkb_info->keymap)
		xkb_map_unref(xkb_info->keymap);

	if (xkb_info->keymap_area)
		munmap(xkb_info->keymap_area, xkb_info->keymap_size);
	if (xkb_info->keymap_fd >= 0)
		close(xkb_info->keymap_fd);
}

static void weston_compositor_xkb_destroy(struct weston_compositor *ec)
{
	free((char *) ec->xkb_names.rules);
	free((char *) ec->xkb_names.model);
	free((char *) ec->xkb_names.layout);
	free((char *) ec->xkb_names.variant);
	free((char *) ec->xkb_names.options);

	xkb_info_destroy(&ec->xkb_info);
	xkb_context_unref(ec->xkb_context);
}

static void
weston_xkb_info_new_keymap(struct weston_xkb_info *xkb_info)
{
	char *keymap_str;

	xkb_info->shift_mod = xkb_map_mod_get_index(xkb_info->keymap,
						    XKB_MOD_NAME_SHIFT);
	xkb_info->caps_mod = xkb_map_mod_get_index(xkb_info->keymap,
						   XKB_MOD_NAME_CAPS);
	xkb_info->ctrl_mod = xkb_map_mod_get_index(xkb_info->keymap,
						   XKB_MOD_NAME_CTRL);
	xkb_info->alt_mod = xkb_map_mod_get_index(xkb_info->keymap,
						  XKB_MOD_NAME_ALT);
	xkb_info->mod2_mod = xkb_map_mod_get_index(xkb_info->keymap, "Mod2");
	xkb_info->mod3_mod = xkb_map_mod_get_index(xkb_info->keymap, "Mod3");
	xkb_info->super_mod = xkb_map_mod_get_index(xkb_info->keymap,
						    XKB_MOD_NAME_LOGO);
	xkb_info->mod5_mod = xkb_map_mod_get_index(xkb_info->keymap, "Mod5");

	xkb_info->num_led = xkb_map_led_get_index(xkb_info->keymap,
						  XKB_LED_NAME_NUM);
	xkb_info->caps_led = xkb_map_led_get_index(xkb_info->keymap,
						   XKB_LED_NAME_CAPS);
	xkb_info->scroll_led = xkb_map_led_get_index(xkb_info->keymap,
						     XKB_LED_NAME_SCROLL);

	keymap_str = xkb_map_get_as_string(xkb_info->keymap);
	if (keymap_str == NULL) {
		weston_log("failed to get string version of keymap\n");
		exit(EXIT_FAILURE);
	}
	xkb_info->keymap_size = strlen(keymap_str) + 1;

	xkb_info->keymap_fd = os_create_anonymous_file(xkb_info->keymap_size);
	if (xkb_info->keymap_fd < 0) {
		weston_log("creating a keymap file for %lu bytes failed: %m\n",
			(unsigned long) xkb_info->keymap_size);
		goto err_keymap_str;
	}

	xkb_info->keymap_area = mmap(NULL, xkb_info->keymap_size,
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED, xkb_info->keymap_fd, 0);
	if (xkb_info->keymap_area == MAP_FAILED) {
		weston_log("failed to mmap() %lu bytes\n",
			(unsigned long) xkb_info->keymap_size);
		goto err_dev_zero;
	}
	strcpy(xkb_info->keymap_area, keymap_str);
	free(keymap_str);

	return;

err_dev_zero:
	close(xkb_info->keymap_fd);
	xkb_info->keymap_fd = -1;
err_keymap_str:
	free(keymap_str);
	exit(EXIT_FAILURE);
}

static void
weston_compositor_build_global_keymap(struct weston_compositor *ec)
{
	if (ec->xkb_info.keymap != NULL)
		return;

	ec->xkb_info.keymap = xkb_map_new_from_names(ec->xkb_context,
						     &ec->xkb_names,
						     0);
	if (ec->xkb_info.keymap == NULL) {
		weston_log("failed to compile global XKB keymap\n");
		weston_log("  tried rules %s, model %s, layout %s, variant %s, "
			"options %s",
			ec->xkb_names.rules, ec->xkb_names.model,
			ec->xkb_names.layout, ec->xkb_names.variant,
			ec->xkb_names.options);
		exit(1);
	}

	weston_xkb_info_new_keymap(&ec->xkb_info);
}

WL_EXPORT void
weston_seat_init_keyboard(struct weston_seat *seat, struct xkb_keymap *keymap)
{
	if (seat->has_keyboard)
		return;

	if (keymap != NULL) {
		seat->xkb_info.keymap = xkb_map_ref(keymap);
		weston_xkb_info_new_keymap(&seat->xkb_info);
	}
	else {
		weston_compositor_build_global_keymap(seat->compositor);
		seat->xkb_info = seat->compositor->xkb_info;
		seat->xkb_info.keymap = xkb_map_ref(seat->xkb_info.keymap);
	}

	seat->xkb_state.state = xkb_state_new(seat->xkb_info.keymap);
	if (seat->xkb_state.state == NULL) {
		weston_log("failed to initialise XKB state\n");
		exit(1);
	}

	seat->xkb_state.leds = 0;

	wl_keyboard_init(&seat->keyboard);
	wl_seat_set_keyboard(&seat->seat, &seat->keyboard);

	seat->has_keyboard = 1;
}

WL_EXPORT void
weston_seat_init_pointer(struct weston_seat *seat)
{
	if (seat->has_pointer)
		return;

	wl_pointer_init(&seat->pointer);
	wl_seat_set_pointer(&seat->seat, &seat->pointer);

	seat->has_pointer = 1;
}

WL_EXPORT void
weston_seat_init_touch(struct weston_seat *seat)
{
	if (seat->has_touch)
		return;

	wl_touch_init(&seat->touch);
	wl_seat_set_touch(&seat->seat, &seat->touch);

	seat->has_touch = 1;
}

WL_EXPORT void
weston_seat_init(struct weston_seat *seat, struct weston_compositor *ec)
{
	wl_seat_init(&seat->seat);
	seat->has_pointer = 0;
	seat->has_keyboard = 0;
	seat->has_touch = 0;

	wl_display_add_global(ec->wl_display, &wl_seat_interface, seat,
			      bind_seat);

	seat->sprite = NULL;
	seat->sprite_destroy_listener.notify = pointer_handle_sprite_destroy;

	seat->compositor = ec;
	seat->hotspot_x = 16;
	seat->hotspot_y = 16;
	seat->modifier_state = 0;
	seat->num_tp = 0;

	seat->drag_surface_destroy_listener.notify =
		handle_drag_surface_destroy;

	wl_list_insert(ec->seat_list.prev, &seat->link);

	seat->new_drag_icon_listener.notify = device_handle_new_drag_icon;
	wl_signal_add(&seat->seat.drag_icon_signal,
		      &seat->new_drag_icon_listener);

	clipboard_create(seat);
	input_method_create(ec, seat);
}

WL_EXPORT void
weston_seat_release(struct weston_seat *seat)
{
	wl_list_remove(&seat->link);
	/* The global object is destroyed at wl_display_destroy() time. */

	if (seat->sprite)
		pointer_unmap_sprite(seat);

	if (seat->xkb_state.state != NULL)
		xkb_state_unref(seat->xkb_state.state);
	xkb_info_destroy(&seat->xkb_info);

	wl_seat_release(&seat->seat);
}

static void
drag_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
	weston_surface_configure(es,
				 es->geometry.x + sx, es->geometry.y + sy,
				 es->buffer->width, es->buffer->height);
}

static int
device_setup_new_drag_surface(struct weston_seat *ws,
			      struct weston_surface *surface)
{
	struct wl_seat *seat = &ws->seat;

	if (surface->configure) {
		wl_resource_post_error(&surface->surface.resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface->configure already set");
		return 0;
	}

	ws->drag_surface = surface;

	weston_surface_set_position(ws->drag_surface,
				    wl_fixed_to_double(seat->pointer->x),
				    wl_fixed_to_double(seat->pointer->y));

	surface->configure = drag_surface_configure;

	wl_signal_add(&surface->surface.resource.destroy_signal,
		       &ws->drag_surface_destroy_listener);

	return 1;
}

static void
device_release_drag_surface(struct weston_seat *seat)
{
	seat->drag_surface->configure = NULL;
	undef_region(&seat->drag_surface->input);
	wl_list_remove(&seat->drag_surface_destroy_listener.link);
	seat->drag_surface = NULL;
}

static void
device_map_drag_surface(struct weston_seat *seat)
{
	struct wl_list *list;

	if (weston_surface_is_mapped(seat->drag_surface) ||
	    !seat->drag_surface->buffer)
		return;

	if (seat->sprite && weston_surface_is_mapped(seat->sprite))
		list = &seat->sprite->layer_link;
	else
		list = &seat->compositor->cursor_layer.surface_list;

	wl_list_insert(list, &seat->drag_surface->layer_link);
	weston_surface_assign_output(seat->drag_surface);
	empty_region(&seat->drag_surface->input);
}

static  void
weston_seat_update_drag_surface(struct weston_seat *seat,
				int dx, int dy)
{
	int surface_changed = 0;

	if (!seat->drag_surface && !seat->seat.drag_surface)
		return;

	if (seat->drag_surface && seat->seat.drag_surface &&
	    (&seat->drag_surface->surface.resource !=
	     &seat->seat.drag_surface->resource))
		/* between calls to this funcion we got a new drag_surface */
		surface_changed = 1;

	if (!seat->seat.drag_surface || surface_changed) {
		device_release_drag_surface(seat);
		if (!surface_changed)
			return;
	}

	if (!seat->drag_surface || surface_changed) {
		struct weston_surface *surface = (struct weston_surface *)
			seat->seat.drag_surface;
		if (!device_setup_new_drag_surface(seat, surface))
			return;
	}

	/* the client may not have attached a buffer to the drag surface
	 * when we setup it up, so check if map is needed on every update */
	device_map_drag_surface(seat);

	/* the client may have attached a buffer with a different size to
	 * the drag surface, causing the input region to be reset */
	if (region_is_undefined(&seat->drag_surface->input))
		empty_region(&seat->drag_surface->input);

	if (!dx && !dy)
		return;

	weston_surface_set_position(seat->drag_surface,
				    seat->drag_surface->geometry.x + wl_fixed_to_double(dx),
				    seat->drag_surface->geometry.y + wl_fixed_to_double(dy));
}

WL_EXPORT void
weston_compositor_update_drag_surfaces(struct weston_compositor *compositor)
{
	struct weston_seat *seat;

	wl_list_for_each(seat, &compositor->seat_list, link)
		weston_seat_update_drag_surface(seat, 0, 0);
}

static void
bind_output(struct wl_client *client,
	    void *data, uint32_t version, uint32_t id)
{
	struct weston_output *output = data;
	struct weston_mode *mode;
	struct wl_resource *resource;

	resource = wl_client_add_object(client,
					&wl_output_interface, NULL, id, data);

	wl_list_insert(&output->resource_list, &resource->link);
	resource->destroy = unbind_resource;

	wl_output_send_geometry(resource,
				output->x,
				output->y,
				output->mm_width,
				output->mm_height,
				output->subpixel,
				output->make, output->model,
				output->transform);

	wl_list_for_each (mode, &output->mode_list, link) {
		wl_output_send_mode(resource,
				    mode->flags,
				    mode->width,
				    mode->height,
				    mode->refresh);
	}
}

static const char vertex_shader[] =
	"uniform mat4 proj;\n"
	"attribute vec2 position;\n"
	"attribute vec2 texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = proj * vec4(position, 0.0, 1.0);\n"
	"   v_texcoord = texcoord;\n"
	"}\n";

/* Declare common fragment shader uniforms */
#define FRAGMENT_CONVERT_YUV						\
	"  y *= alpha;\n"						\
	"  u *= alpha;\n"						\
	"  v *= alpha;\n"						\
	"  gl_FragColor.r = y + 1.59602678 * v;\n"			\
	"  gl_FragColor.g = y - 0.39176229 * u - 0.81296764 * v;\n"	\
	"  gl_FragColor.b = y + 2.01723214 * u;\n"			\
	"  gl_FragColor.a = alpha;\n"

static const char texture_fragment_shader_rgba[] =
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D tex;\n"
	"uniform float alpha;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = alpha * texture2D(tex, v_texcoord)\n;"
	"}\n";

static const char texture_fragment_shader_rgbx[] =
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D tex;\n"
	"uniform float alpha;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor.rgb = alpha * texture2D(tex, v_texcoord).rgb\n;"
	"   gl_FragColor.a = alpha;\n"
	"}\n";

static const char texture_fragment_shader_egl_external[] =
	"#extension GL_OES_EGL_image_external : require\n"
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform samplerExternalOES tex;\n"
	"uniform float alpha;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = alpha * texture2D(tex, v_texcoord)\n;"
	"}\n";

static const char texture_fragment_shader_y_uv[] =
	"precision mediump float;\n"
	"uniform sampler2D tex;\n"
	"uniform sampler2D tex1;\n"
	"varying vec2 v_texcoord;\n"
	"uniform float alpha;\n"
	"void main() {\n"
	"  float y = 1.16438356 * (texture2D(tex, v_texcoord).x - 0.0625);\n"
	"  float u = texture2D(tex1, v_texcoord).r - 0.5;\n"
	"  float v = texture2D(tex1, v_texcoord).g - 0.5;\n"
	FRAGMENT_CONVERT_YUV
	"}\n";

static const char texture_fragment_shader_y_u_v[] =
	"precision mediump float;\n"
	"uniform sampler2D tex;\n"
	"uniform sampler2D tex1;\n"
	"uniform sampler2D tex2;\n"
	"varying vec2 v_texcoord;\n"
	"uniform float alpha;\n"
	"void main() {\n"
	"  float y = 1.16438356 * (texture2D(tex, v_texcoord).x - 0.0625);\n"
	"  float u = texture2D(tex1, v_texcoord).x - 0.5;\n"
	"  float v = texture2D(tex2, v_texcoord).x - 0.5;\n"
	FRAGMENT_CONVERT_YUV
	"}\n";

static const char texture_fragment_shader_y_xuxv[] =
	"precision mediump float;\n"
	"uniform sampler2D tex;\n"
	"uniform sampler2D tex1;\n"
	"varying vec2 v_texcoord;\n"
	"uniform float alpha;\n"
	"void main() {\n"
	"  float y = 1.16438356 * (texture2D(tex, v_texcoord).x - 0.0625);\n"
	"  float u = texture2D(tex1, v_texcoord).g - 0.5;\n"
	"  float v = texture2D(tex1, v_texcoord).a - 0.5;\n"
	FRAGMENT_CONVERT_YUV
	"}\n";

static const char solid_fragment_shader[] =
	"precision mediump float;\n"
	"uniform vec4 color;\n"
	"uniform float alpha;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = alpha * color\n;"
	"}\n";

static int
compile_shader(GLenum type, const char *source)
{
	GLuint s;
	char msg[512];
	GLint status;

	s = glCreateShader(type);
	glShaderSource(s, 1, &source, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(s, sizeof msg, NULL, msg);
		weston_log("shader info: %s\n", msg);
		return GL_NONE;
	}

	return s;
}

static int
weston_shader_init(struct weston_shader *shader,
		   const char *vertex_source, const char *fragment_source)
{
	char msg[512];
	GLint status;

	shader->vertex_shader =
		compile_shader(GL_VERTEX_SHADER, vertex_source);
	shader->fragment_shader =
		compile_shader(GL_FRAGMENT_SHADER, fragment_source);

	shader->program = glCreateProgram();
	glAttachShader(shader->program, shader->vertex_shader);
	glAttachShader(shader->program, shader->fragment_shader);
	glBindAttribLocation(shader->program, 0, "position");
	glBindAttribLocation(shader->program, 1, "texcoord");

	glLinkProgram(shader->program);
	glGetProgramiv(shader->program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(shader->program, sizeof msg, NULL, msg);
		weston_log("link info: %s\n", msg);
		return -1;
	}

	shader->proj_uniform = glGetUniformLocation(shader->program, "proj");
	shader->tex_uniforms[0] = glGetUniformLocation(shader->program, "tex");
	shader->tex_uniforms[1] = glGetUniformLocation(shader->program, "tex1");
	shader->tex_uniforms[2] = glGetUniformLocation(shader->program, "tex2");
	shader->alpha_uniform = glGetUniformLocation(shader->program, "alpha");
	shader->color_uniform = glGetUniformLocation(shader->program, "color");

	return 0;
}

WL_EXPORT void
weston_output_destroy(struct weston_output *output)
{
	struct weston_compositor *c = output->compositor;

	pixman_region32_fini(&output->region);
	pixman_region32_fini(&output->previous_damage);
	output->compositor->output_id_pool &= ~(1 << output->id);

	wl_display_remove_global(c->wl_display, output->global);
}

static void
weston_output_compute_transform(struct weston_output *output)
{
	struct weston_matrix transform;
	int flip;

	weston_matrix_init(&transform);

	switch(output->transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
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
				-(output->x + (output->border.right + output->width - output->border.left) / 2.0),
				-(output->y + (output->border.bottom + output->height - output->border.top) / 2.0), 0);

	weston_matrix_scale(&output->matrix,
			    2.0 / (output->width + output->border.left + output->border.right),
			    -2.0 / (output->height + output->border.top + output->border.bottom), 1);

	weston_output_compute_transform(output);

	if (output->zoom.active) {
		magnification = 1 / (1 - output->zoom.spring_z.current);
		weston_matrix_init(&camera);
		weston_matrix_init(&modelview);
		weston_output_update_zoom(output, output->zoom.type);
		weston_matrix_translate(&camera, output->zoom.trans_x,
					-output->zoom.trans_y, 0);
		weston_matrix_invert(&modelview, &camera);
		weston_matrix_scale(&modelview, magnification, magnification, 1.0);
		weston_matrix_multiply(&output->matrix, &modelview);
	}

	output->dirty = 0;
}

static void
weston_output_transform_init(struct weston_output *output, uint32_t transform)
{
	output->transform = transform;

	switch (transform) {
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		/* Swap width and height */
		output->width = output->current->height;
		output->height = output->current->width;
		break;
	case WL_OUTPUT_TRANSFORM_NORMAL:
	case WL_OUTPUT_TRANSFORM_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		output->width = output->current->width;
		output->height = output->current->height;
		break;
	default:
		break;
	}
}

WL_EXPORT void
weston_output_move(struct weston_output *output, int x, int y)
{
	output->x = x;
	output->y = y;

	pixman_region32_init(&output->previous_damage);
	pixman_region32_init_rect(&output->region, x, y,
				  output->width,
				  output->height);
}

WL_EXPORT void
weston_output_init(struct weston_output *output, struct weston_compositor *c,
		   int x, int y, int width, int height, uint32_t transform)
{
	output->compositor = c;
	output->x = x;
	output->y = y;
	output->border.top = 0;
	output->border.bottom = 0;
	output->border.left = 0;
	output->border.right = 0;
	output->mm_width = width;
	output->mm_height = height;
	output->dirty = 1;

	if (transform != WL_OUTPUT_TRANSFORM_NORMAL)
		output->disable_planes++;

	weston_output_transform_init(output, transform);
	weston_output_init_zoom(output);

	weston_output_move(output, x, y);
	weston_output_damage(output);

	wl_signal_init(&output->frame_signal);
	wl_list_init(&output->animation_list);
	wl_list_init(&output->resource_list);

	output->id = ffs(~output->compositor->output_id_pool) - 1;
	output->compositor->output_id_pool |= 1 << output->id;

	output->global =
		wl_display_add_global(c->wl_display, &wl_output_interface,
				      output, bind_output);
}

static void
compositor_bind(struct wl_client *client,
		void *data, uint32_t version, uint32_t id)
{
	struct weston_compositor *compositor = data;

	wl_client_add_object(client, &wl_compositor_interface,
			     &compositor_interface, id, compositor);
}

static void
log_uname(void)
{
	struct utsname usys;

	uname(&usys);

	weston_log("OS: %s, %s, %s, %s\n", usys.sysname, usys.release,
						usys.version, usys.machine);
}

static void
log_extensions(const char *name, const char *extensions)
{
	const char *p, *end;
	int l;
	int len;

	l = weston_log("%s:", name);
	p = extensions;
	while (*p) {
		end = strchrnul(p, ' ');
		len = end - p;
		if (l + len > 78)
			l = weston_log_continue("\n" STAMP_SPACE "%.*s",
						len, p);
		else
			l += weston_log_continue(" %.*s", len, p);
		for (p = end; isspace(*p); p++)
			;
	}
	weston_log_continue("\n");
}

static void
log_egl_gl_info(EGLDisplay egldpy)
{
	const char *str;

	str = eglQueryString(egldpy, EGL_VERSION);
	weston_log("EGL version: %s\n", str ? str : "(null)");

	str = eglQueryString(egldpy, EGL_VENDOR);
	weston_log("EGL vendor: %s\n", str ? str : "(null)");

	str = eglQueryString(egldpy, EGL_CLIENT_APIS);
	weston_log("EGL client APIs: %s\n", str ? str : "(null)");

	str = eglQueryString(egldpy, EGL_EXTENSIONS);
	log_extensions("EGL extensions", str ? str : "(null)");

	str = (char *)glGetString(GL_VERSION);
	weston_log("GL version: %s\n", str ? str : "(null)");

	str = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	weston_log("GLSL version: %s\n", str ? str : "(null)");

	str = (char *)glGetString(GL_VENDOR);
	weston_log("GL vendor: %s\n", str ? str : "(null)");

	str = (char *)glGetString(GL_RENDERER);
	weston_log("GL renderer: %s\n", str ? str : "(null)");

	str = (char *)glGetString(GL_EXTENSIONS);
	log_extensions("GL extensions", str ? str : "(null)");
}

WL_EXPORT int
weston_compositor_init(struct weston_compositor *ec,
		       struct wl_display *display,
		       int argc,
		       char *argv[],
		       const char *config_file)
{
	struct wl_event_loop *loop;
	struct xkb_rule_names xkb_names;
        const struct config_key keyboard_config_keys[] = {
		{ "keymap_rules", CONFIG_KEY_STRING, &xkb_names.rules },
		{ "keymap_model", CONFIG_KEY_STRING, &xkb_names.model },
		{ "keymap_layout", CONFIG_KEY_STRING, &xkb_names.layout },
		{ "keymap_variant", CONFIG_KEY_STRING, &xkb_names.variant },
		{ "keymap_options", CONFIG_KEY_STRING, &xkb_names.options },
        };
	const struct config_section cs[] = {
                { "keyboard",
                  keyboard_config_keys, ARRAY_LENGTH(keyboard_config_keys) },
	};

	memset(&xkb_names, 0, sizeof(xkb_names));
	parse_config_file(config_file, cs, ARRAY_LENGTH(cs), ec);

	ec->wl_display = display;
	wl_signal_init(&ec->destroy_signal);
	wl_signal_init(&ec->activate_signal);
	wl_signal_init(&ec->lock_signal);
	wl_signal_init(&ec->unlock_signal);
	wl_signal_init(&ec->show_input_panel_signal);
	wl_signal_init(&ec->hide_input_panel_signal);
	ec->launcher_sock = weston_environment_get_fd("WESTON_LAUNCHER_SOCK");

	ec->output_id_pool = 0;

	if (!wl_display_add_global(display, &wl_compositor_interface,
				   ec, compositor_bind))
		return -1;

	wl_list_init(&ec->surface_list);
	wl_list_init(&ec->layer_list);
	wl_list_init(&ec->seat_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->key_binding_list);
	wl_list_init(&ec->button_binding_list);
	wl_list_init(&ec->axis_binding_list);
	wl_list_init(&ec->fade.animation.link);

	weston_plane_init(&ec->primary_plane, 0, 0);

	weston_compositor_xkb_init(ec, &xkb_names);

	ec->ping_handler = NULL;

	screenshooter_create(ec);
	text_cursor_position_notifier_create(ec);

	wl_data_device_manager_init(ec->wl_display);

	wl_display_init_shm(display);

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->idle_source = wl_event_loop_add_timer(loop, idle_handler, ec);
	wl_event_source_timer_update(ec->idle_source, ec->idle_time * 1000);

	ec->input_loop = wl_event_loop_create();

	return 0;
}

WL_EXPORT int
weston_compositor_init_gl(struct weston_compositor *ec)
{
	const char *extensions;
	int has_egl_image_external = 0;

	log_egl_gl_info(ec->egl_display);

	ec->image_target_texture_2d =
		(void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	ec->image_target_renderbuffer_storage = (void *)
		eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	ec->create_image = (void *) eglGetProcAddress("eglCreateImageKHR");
	ec->destroy_image = (void *) eglGetProcAddress("eglDestroyImageKHR");
	ec->bind_display =
		(void *) eglGetProcAddress("eglBindWaylandDisplayWL");
	ec->unbind_display =
		(void *) eglGetProcAddress("eglUnbindWaylandDisplayWL");
	ec->query_buffer =
		(void *) eglGetProcAddress("eglQueryWaylandBufferWL");

	extensions = (const char *) glGetString(GL_EXTENSIONS);
	if (!extensions) {
		weston_log("Retrieving GL extension string failed.\n");
		return -1;
	}

	if (!strstr(extensions, "GL_EXT_texture_format_BGRA8888")) {
		weston_log("GL_EXT_texture_format_BGRA8888 not available\n");
		return -1;
	}

	if (strstr(extensions, "GL_EXT_read_format_bgra"))
		ec->read_format = GL_BGRA_EXT;
	else
		ec->read_format = GL_RGBA;

	if (strstr(extensions, "GL_EXT_unpack_subimage"))
		ec->has_unpack_subimage = 1;

	if (strstr(extensions, "GL_OES_EGL_image_external"))
		has_egl_image_external = 1;

	extensions =
		(const char *) eglQueryString(ec->egl_display, EGL_EXTENSIONS);
	if (!extensions) {
		weston_log("Retrieving EGL extension string failed.\n");
		return -1;
	}

	if (strstr(extensions, "EGL_WL_bind_wayland_display"))
		ec->has_bind_display = 1;
	if (ec->has_bind_display)
		ec->bind_display(ec->egl_display, ec->wl_display);

	weston_spring_init(&ec->fade.spring, 30.0, 1.0, 1.0);
	ec->fade.animation.frame = fade_frame;

	weston_layer_init(&ec->fade_layer, &ec->layer_list);
	weston_layer_init(&ec->cursor_layer, &ec->fade_layer.link);

	glActiveTexture(GL_TEXTURE0);

	if (weston_shader_init(&ec->texture_shader_rgba,
			     vertex_shader, texture_fragment_shader_rgba) < 0)
		return -1;
	if (weston_shader_init(&ec->texture_shader_rgbx,
			     vertex_shader, texture_fragment_shader_rgbx) < 0)
		return -1;
	if (has_egl_image_external &&
			weston_shader_init(&ec->texture_shader_egl_external,
				vertex_shader, texture_fragment_shader_egl_external) < 0)
		return -1;
	if (weston_shader_init(&ec->texture_shader_y_uv,
			       vertex_shader, texture_fragment_shader_y_uv) < 0)
		return -1;
	if (weston_shader_init(&ec->texture_shader_y_u_v,
			       vertex_shader, texture_fragment_shader_y_u_v) < 0)
		return -1;
	if (weston_shader_init(&ec->texture_shader_y_xuxv,
			       vertex_shader, texture_fragment_shader_y_xuxv) < 0)
		return -1;
	if (weston_shader_init(&ec->solid_shader,
			     vertex_shader, solid_fragment_shader) < 0)
		return -1;

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

	weston_binding_list_destroy_all(&ec->key_binding_list);
	weston_binding_list_destroy_all(&ec->button_binding_list);
	weston_binding_list_destroy_all(&ec->axis_binding_list);

	weston_plane_release(&ec->primary_plane);

	wl_array_release(&ec->vertices);
	wl_array_release(&ec->indices);
	wl_array_release(&ec->vtxcnt);

	wl_event_loop_destroy(ec->input_loop);
}

static int on_term_signal(int signal_number, void *data)
{
	struct wl_display *display = data;

	weston_log("caught signal %d\n", signal_number);
	wl_display_terminate(display);

	return 1;
}

static void
on_segv_signal(int s, siginfo_t *siginfo, void *context)
{
	void *buffer[32];
	int i, count;
	Dl_info info;

	/* This SIGSEGV handler will do a best-effort backtrace, and
	 * then call the backend restore function, which will switch
	 * back to the vt we launched from or ungrab X etc and then
	 * raise SIGTRAP.  If we run weston under gdb from X or a
	 * different vt, and tell gdb "handle SIGSEGV nostop", this
	 * will allow weston to switch back to gdb on crash and then
	 * gdb will catch the crash with SIGTRAP. */

	weston_log("caught segv\n");

	count = backtrace(buffer, ARRAY_LENGTH(buffer));
	for (i = 0; i < count; i++) {
		dladdr(buffer[i], &info);
		weston_log("  [%016lx]  %s  (%s)\n",
			(long) buffer[i],
			info.dli_sname ? info.dli_sname : "--",
			info.dli_fname);
	}

	segv_compositor->restore(segv_compositor);

	raise(SIGTRAP);
}


static void *
load_module(const char *name, const char *entrypoint, void **handle)
{
	char path[PATH_MAX];
	void *module, *init;

	if (name[0] != '/')
		snprintf(path, sizeof path, "%s/%s", MODULEDIR, name);
	else
		snprintf(path, sizeof path, "%s", name);

	weston_log("Loading module '%s'\n", path);
	module = dlopen(path, RTLD_NOW);
	if (!module) {
		weston_log("Failed to load module: %s\n", dlerror());
		return NULL;
	}

	init = dlsym(module, entrypoint);
	if (!init) {
		weston_log("Failed to lookup init function: %s\n", dlerror());
		return NULL;
	}

	return init;
}

static const char xdg_error_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR is not set.\n";

static const char xdg_wrong_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR\n"
	"is set to \"%s\", which is not a directory.\n";

static const char xdg_wrong_mode_message[] =
	"warning: XDG_RUNTIME_DIR \"%s\" is not configured\n"
	"correctly.  Unix access mode must be 0700 but is %o,\n"
	"and XDG_RUNTIME_DIR must be owned by the user, but is\n"
	"owned by UID %d.\n";

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
		"  -B, --backend=MODULE\tBackend module, one of drm-backend.so,\n"
		"\t\t\t\tx11-backend.so or wayland-backend.so\n"
		"  -S, --socket=NAME\tName of socket to listen on\n"
		"  -i, --idle-time=SECS\tIdle time in seconds\n"
		"  --xserver\t\tEnable X server integration\n"
		"  --module\t\tLoad the specified module\n"
		"  --log==FILE\t\tLog to the given file\n"
		"  -h, --help\t\tThis help message\n\n");

	fprintf(stderr,
		"Options for drm-backend.so:\n\n"
		"  --connector=ID\tBring up only this connector\n"
		"  --seat=SEAT\t\tThe seat that weston should run on\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --current-mode\tPrefer current KMS mode over EDID preferred mode\n\n");

	fprintf(stderr,
		"Options for x11-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of X window\n"
		"  --height=HEIGHT\tHeight of X window\n"
		"  --fullscreen\t\tRun in fullscreen mode\n"
		"  --output-count=COUNT\tCreate multiple outputs\n"
		"  --no-input\t\tDont create input devices\n\n");

	fprintf(stderr,
		"Options for wayland-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of Wayland surface\n"
		"  --height=HEIGHT\tHeight of Wayland surface\n"
		"  --display=DISPLAY\tWayland display to connect to\n\n");

	exit(error_code);
}

int main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;
	struct wl_display *display;
	struct weston_compositor *ec;
	struct wl_event_source *signals[4];
	struct wl_event_loop *loop;
	struct sigaction segv_action;
	void *shell_module, *backend_module, *xserver_module;
	int (*module_init)(struct weston_compositor *ec);
	struct weston_compositor
		*(*backend_init)(struct wl_display *display,
				 int argc, char *argv[], const char *config_file);
	int i;
	char *backend = NULL;
	char *shell = NULL;
	char *module = NULL;
	char *log = NULL;
	int32_t idle_time = 300;
	int32_t xserver = 0;
	int32_t help = 0;
	char *socket_name = NULL;
	char *config_file;

	const struct config_key shell_config_keys[] = {
		{ "type", CONFIG_KEY_STRING, &shell },
	};

	const struct config_section cs[] = {
		{ "shell",
		  shell_config_keys, ARRAY_LENGTH(shell_config_keys) },
	};

	const struct weston_option core_options[] = {
		{ WESTON_OPTION_STRING, "backend", 'B', &backend },
		{ WESTON_OPTION_STRING, "socket", 'S', &socket_name },
		{ WESTON_OPTION_INTEGER, "idle-time", 'i', &idle_time },
		{ WESTON_OPTION_BOOLEAN, "xserver", 0, &xserver },
		{ WESTON_OPTION_STRING, "module", 0, &module },
		{ WESTON_OPTION_STRING, "log", 0, &log },
		{ WESTON_OPTION_BOOLEAN, "help", 'h', &help },
	};

	argc = parse_options(core_options,
			     ARRAY_LENGTH(core_options), argc, argv);

	if (help)
		usage(EXIT_SUCCESS);

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

	if (!backend) {
		if (getenv("WAYLAND_DISPLAY"))
			backend = "wayland-backend.so";
		else if (getenv("DISPLAY"))
			backend = "x11-backend.so";
		else
			backend = "drm-backend.so";
	}

	config_file = config_file_path("weston.ini");
	parse_config_file(config_file, cs, ARRAY_LENGTH(cs), shell);

	backend_init = load_module(backend, "backend_init", &backend_module);
	if (!backend_init)
		exit(EXIT_FAILURE);

	ec = backend_init(display, argc, argv, config_file);
	if (ec == NULL) {
		weston_log("fatal: failed to create compositor\n");
		exit(EXIT_FAILURE);
	}

	segv_action.sa_flags = SA_SIGINFO | SA_RESETHAND;
	segv_action.sa_sigaction = on_segv_signal;
	sigemptyset(&segv_action.sa_mask);
	sigaction(SIGSEGV, &segv_action, NULL);
	segv_compositor = ec;

	for (i = 1; argv[i]; i++)
		weston_log("fatal: unhandled option: %s\n", argv[i]);
	if (argv[1]) {
		ret = EXIT_FAILURE;
		goto out;
	}

	free(config_file);

	ec->option_idle_time = idle_time;
	ec->idle_time = idle_time;

	module_init = NULL;
	if (xserver)
		module_init = load_module("xwayland.so",
					  "weston_xserver_init",
					  &xserver_module);
	if (module_init && module_init(ec) < 0) {
		ret = EXIT_FAILURE;
		goto out;
	}

	if (socket_name)
		setenv("WAYLAND_DISPLAY", socket_name, 1);

	if (!shell)
		shell = "desktop-shell.so";
	module_init = load_module(shell, "shell_init", &shell_module);
	if (!module_init || module_init(ec) < 0) {
		ret = EXIT_FAILURE;
		goto out;
	}


	module_init = NULL;
	if (module)
		module_init = load_module(module, "module_init", NULL);
	if (module_init && module_init(ec) < 0) {
		ret = EXIT_FAILURE;
		goto out;
	}

	if (wl_display_add_socket(display, socket_name)) {
		weston_log("fatal: failed to add socket: %m\n");
		ret = EXIT_FAILURE;
		goto out;
	}

	weston_compositor_dpms_on(ec);
	weston_compositor_wake(ec);

	wl_display_run(display);

 out:
	/* prevent further rendering while shutting down */
	ec->state = WESTON_COMPOSITOR_SLEEPING;

	wl_signal_emit(&ec->destroy_signal, ec);

	if (ec->has_bind_display)
		ec->unbind_display(ec->egl_display, display);

	for (i = ARRAY_LENGTH(signals); i;)
		wl_event_source_remove(signals[--i]);

	weston_compositor_xkb_destroy(ec);

	ec->destroy(ec);
	wl_display_destroy(display);

	weston_log_file_close();

	return ret;
}
