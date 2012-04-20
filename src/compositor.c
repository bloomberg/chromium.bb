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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <linux/input.h>
#include <dlfcn.h>
#include <signal.h>
#include <setjmp.h>
#include <execinfo.h>

#include <wayland-server.h>
#include "compositor.h"

static struct wl_list child_process_list;
static jmp_buf segv_jmp_buf;

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
		fprintf(stderr, "unknown child process exited\n");
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
		fprintf(stderr, "compositor: dup failed: %m\n");
		return;
	}

	snprintf(s, sizeof s, "%d", clientfd);
	setenv("WAYLAND_SOCKET", s, 1);

	if (execl(path, path, NULL) < 0)
		fprintf(stderr, "compositor: executing '%s' failed: %m\n",
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

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
		fprintf(stderr, "weston_client_launch: "
			"socketpair failed while launching '%s': %m\n",
			path);
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		close(sv[0]);
		close(sv[1]);
		fprintf(stderr,  "weston_client_launch: "
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
		fprintf(stderr, "weston_client_launch: "
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
surface_handle_buffer_destroy(struct wl_listener *listener, void *data)
{
	struct weston_surface *es =
		container_of(listener, struct weston_surface, 
			     buffer_destroy_listener);

	es->buffer = NULL;
}

static const pixman_region32_data_t undef_region_data;

static void
undef_region(pixman_region32_t *region)
{
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
	surface->image = EGL_NO_IMAGE_KHR;
	surface->alpha = 255;
	surface->brightness = 255;
	surface->saturation = 255;
	surface->pitch = 1;

	surface->buffer = NULL;
	surface->output = NULL;

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

static void
surface_to_global_float(struct weston_surface *surface,
			int32_t sx, int32_t sy, GLfloat *x, GLfloat *y)
{
	if (surface->transform.enabled) {
		struct weston_vector v = { { sx, sy, 0.0f, 1.0f } };

		weston_matrix_transform(&surface->transform.matrix, &v);

		if (fabsf(v.f[3]) < 1e-6) {
			fprintf(stderr, "warning: numerical instability in "
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
weston_surface_damage_below(struct weston_surface *surface)
{
	struct weston_compositor *compositor = surface->compositor;
	pixman_region32_t damage;

	pixman_region32_init(&damage);
	pixman_region32_subtract(&damage, &surface->transform.boundingbox,
				 &surface->clip);
	pixman_region32_union(&compositor->damage,
			      &compositor->damage, &damage);
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
		surface_to_global_float(surface, s[i][0], s[i][1], &x, &y);
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

	if (surface->alpha == 255) {
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
		fprintf(stderr, "error: weston_surface %p"
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

	/* weston_surface_damage() without update */
	pixman_region32_union(&surface->damage, &surface->damage,
			      &surface->transform.boundingbox);

	if (weston_surface_is_mapped(surface))
		weston_surface_assign_output(surface);

	weston_compositor_schedule_repaint(surface->compositor);
}

WL_EXPORT void
weston_surface_to_global_float(struct weston_surface *surface,
			       int32_t sx, int32_t sy, GLfloat *x, GLfloat *y)
{
	weston_surface_update_transform(surface);

	surface_to_global_float(surface, sx, sy, x, y);
}

WL_EXPORT void
weston_surface_to_global(struct weston_surface *surface,
			 int32_t sx, int32_t sy, int32_t *x, int32_t *y)
{
	GLfloat xf, yf;

	weston_surface_to_global_float(surface, sx, sy, &xf, &yf);
	*x = floorf(xf);
	*y = floorf(yf);
}

static void
surface_from_global_float(struct weston_surface *surface,
			  int32_t x, int32_t y, GLfloat *sx, GLfloat *sy)
{
	if (surface->transform.enabled) {
		struct weston_vector v = { { x, y, 0.0f, 1.0f } };

		weston_matrix_transform(&surface->transform.inverse, &v);

		if (fabsf(v.f[3]) < 1e-6) {
			fprintf(stderr, "warning: numerical instability in "
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
weston_surface_from_global(struct weston_surface *surface,
			   int32_t x, int32_t y, int32_t *sx, int32_t *sy)
{
	GLfloat sxf, syf;

	weston_surface_update_transform(surface);

	surface_from_global_float(surface, x, y, &sxf, &syf);
	*sx = floorf(sxf);
	*sy = floorf(syf);
}

static void
weston_surface_damage_rectangle(struct weston_surface *surface,
				int32_t sx, int32_t sy,
				int32_t width, int32_t height)
{
	weston_surface_update_transform(surface);

	if (surface->transform.enabled) {
		pixman_region32_t box;
		surface_compute_bbox(surface, sx, sy, width, height, &box);
		pixman_region32_union(&surface->damage, &surface->damage,
				      &box);
		pixman_region32_fini(&box);
	} else {
		pixman_region32_union_rect(&surface->damage, &surface->damage,
					   surface->geometry.x + sx,
					   surface->geometry.y + sy,
					   width, height);
	}

	weston_compositor_schedule_repaint(surface->compositor);
}

WL_EXPORT void
weston_surface_damage(struct weston_surface *surface)
{
	weston_surface_update_transform(surface);

	pixman_region32_union(&surface->damage, &surface->damage,
			      &surface->transform.boundingbox);

	weston_compositor_schedule_repaint(surface->compositor);
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
			       int32_t x, int32_t y, int32_t *sx, int32_t *sy)
{
	struct weston_surface *surface;

	wl_list_for_each(surface, &compositor->surface_list, link) {
		weston_surface_from_global(surface, x, y, sx, sy);
		if (pixman_region32_contains_point(&surface->input,
						   *sx, *sy, NULL))
			return surface;
	}

	return NULL;
}

static void
weston_device_repick(struct wl_input_device *device)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	const struct wl_pointer_grab_interface *interface;
	struct weston_surface *surface, *focus;

	surface = weston_compositor_pick_surface(wd->compositor,
						 device->x, device->y,
						 &device->current_x,
						 &device->current_y);

	if (&surface->surface != device->current) {
		interface = device->pointer_grab->interface;
		interface->focus(device->pointer_grab, &surface->surface,
				 device->current_x, device->current_y);
		device->current = &surface->surface;
	}

	focus = (struct weston_surface *) device->pointer_grab->focus;
	if (focus)
		weston_surface_from_global(focus, device->x, device->y,
					   &device->pointer_grab->x, &device->pointer_grab->y);
}

WL_EXPORT void
weston_compositor_repick(struct weston_compositor *compositor)
{
	struct weston_input_device *device;

	if (!compositor->focus)
		return;

	wl_list_for_each(device, &compositor->input_device_list, link)
		weston_device_repick(&device->input_device);
}

static void
weston_surface_unmap(struct weston_surface *surface)
{
	struct wl_input_device *device = surface->compositor->input_device;

	weston_surface_damage_below(surface);
	surface->output = NULL;
	wl_list_remove(&surface->link);
	wl_list_remove(&surface->layer_link);

	if (device->keyboard_focus == &surface->surface)
		wl_input_device_set_keyboard_focus(device, NULL);
	if (device->pointer_focus == &surface->surface)
		wl_input_device_set_pointer_focus(device, NULL, 0, 0);

	weston_compositor_schedule_repaint(surface->compositor);
}

static void
destroy_surface(struct wl_resource *resource)
{
	struct weston_surface *surface =
		container_of(resource,
			     struct weston_surface, surface.resource);
	struct weston_compositor *compositor = surface->compositor;

	if (weston_surface_is_mapped(surface))
		weston_surface_unmap(surface);

	if (surface->texture)
		glDeleteTextures(1, &surface->texture);

	if (surface->buffer)
		wl_list_remove(&surface->buffer_destroy_listener.link);

	if (surface->image != EGL_NO_IMAGE_KHR)
		compositor->destroy_image(compositor->display,
					  surface->image);

	pixman_region32_fini(&surface->transform.boundingbox);
	pixman_region32_fini(&surface->damage);
	pixman_region32_fini(&surface->opaque);
	pixman_region32_fini(&surface->clip);
	if (!region_is_undefined(&surface->input))
		pixman_region32_fini(&surface->input);

	free(surface);
}

WL_EXPORT void
weston_surface_destroy(struct weston_surface *surface)
{
	/* Not a valid way to destroy a client surface */
	assert(surface->surface.resource.client == NULL);

	destroy_surface(&surface->surface.resource);
}

static void
weston_surface_attach(struct wl_surface *surface, struct wl_buffer *buffer)
{
	struct weston_surface *es = (struct weston_surface *) surface;
	struct weston_compositor *ec = es->compositor;

	if (es->buffer) {
		weston_buffer_post_release(es->buffer);
		wl_list_remove(&es->buffer_destroy_listener.link);
	}

	es->buffer = buffer;

	if (!buffer) {
		if (weston_surface_is_mapped(es))
			weston_surface_unmap(es);
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

	if (!es->texture) {
		glGenTextures(1, &es->texture);
		glBindTexture(GL_TEXTURE_2D, es->texture);
		glTexParameteri(GL_TEXTURE_2D,
				GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D,
				GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		es->shader = &ec->texture_shader;
	} else {
		glBindTexture(GL_TEXTURE_2D, es->texture);
	}

	if (wl_buffer_is_shm(buffer)) {
		es->pitch = wl_shm_buffer_get_stride(buffer) / 4;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     es->pitch, es->buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
	} else {
		if (es->image != EGL_NO_IMAGE_KHR)
			ec->destroy_image(ec->display, es->image);
		es->image = ec->create_image(ec->display, NULL,
					     EGL_WAYLAND_BUFFER_WL,
					     buffer, NULL);

		ec->image_target_texture_2d(GL_TEXTURE_2D, es->image);

		es->pitch = buffer->width;
	}
}

static int
texture_region(struct weston_surface *es, pixman_region32_t *region)
{
	struct weston_compositor *ec = es->compositor;
	GLfloat *v, inv_width, inv_height;
	GLfloat sx, sy;
	pixman_box32_t *rectangles;
	unsigned int *p;
	int i, n;

	rectangles = pixman_region32_rectangles(region, &n);
	v = wl_array_add(&ec->vertices, n * 16 * sizeof *v);
	p = wl_array_add(&ec->indices, n * 6 * sizeof *p);
	inv_width = 1.0 / es->pitch;
	inv_height = 1.0 / es->geometry.height;

	for (i = 0; i < n; i++, v += 16, p += 6) {
		surface_from_global_float(es, rectangles[i].x1,
					  rectangles[i].y1, &sx, &sy);
		v[ 0] = rectangles[i].x1;
		v[ 1] = rectangles[i].y1;
		v[ 2] = sx * inv_width;
		v[ 3] = sy * inv_height;

		surface_from_global_float(es, rectangles[i].x1,
					  rectangles[i].y2, &sx, &sy);
		v[ 4] = rectangles[i].x1;
		v[ 5] = rectangles[i].y2;
		v[ 6] = sx * inv_width;
		v[ 7] = sy * inv_height;

		surface_from_global_float(es, rectangles[i].x2,
					  rectangles[i].y1, &sx, &sy);
		v[ 8] = rectangles[i].x2;
		v[ 9] = rectangles[i].y1;
		v[10] = sx * inv_width;
		v[11] = sy * inv_height;

		surface_from_global_float(es, rectangles[i].x2,
					  rectangles[i].y2, &sx, &sy);
		v[12] = rectangles[i].x2;
		v[13] = rectangles[i].y2;
		v[14] = sx * inv_width;
		v[15] = sy * inv_height;

		p[0] = i * 4 + 0;
		p[1] = i * 4 + 1;
		p[2] = i * 4 + 2;
		p[3] = i * 4 + 2;
		p[4] = i * 4 + 1;
		p[5] = i * 4 + 3;
	}

	return n;
}

WL_EXPORT void
weston_surface_draw(struct weston_surface *es, struct weston_output *output,
		    pixman_region32_t *damage)
{
	struct weston_compositor *ec = es->compositor;
	GLfloat *v;
	pixman_region32_t repaint;
	GLint filter;
	int n;

	pixman_region32_init(&repaint);
	pixman_region32_intersect(&repaint,
				  &es->transform.boundingbox, damage);
	pixman_region32_subtract(&repaint, &repaint, &es->clip);

	if (!pixman_region32_not_empty(&repaint))
		goto out;

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	if (ec->current_shader != es->shader) {
		glUseProgram(es->shader->program);
		ec->current_shader = es->shader;
	}

	glUniformMatrix4fv(es->shader->proj_uniform,
			   1, GL_FALSE, output->matrix.d);
	glUniform1i(es->shader->tex_uniform, 0);
	glUniform4fv(es->shader->color_uniform, 1, es->color);
	glUniform1f(es->shader->alpha_uniform, es->alpha / 255.0);
	glUniform1f(es->shader->brightness_uniform, es->brightness / 255.0);
	glUniform1f(es->shader->saturation_uniform, es->saturation / 255.0);
	glUniform1f(es->shader->texwidth_uniform,
		    (GLfloat)es->geometry.width / es->pitch);

	if (es->transform.enabled || output->zoom.active)
		filter = GL_LINEAR;
	else
		filter = GL_NEAREST;

	n = texture_region(es, &repaint);

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	v = ec->vertices.data;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[0]);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[2]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glDrawElements(GL_TRIANGLES, n * 6, GL_UNSIGNED_INT, ec->indices.data);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);

	ec->vertices.size = 0;
	ec->indices.size = 0;

out:
	pixman_region32_fini(&repaint);
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

	pixman_region32_union(&compositor->damage,
			      &compositor->damage, &output->region);
	weston_compositor_schedule_repaint(compositor);
}

static void
fade_frame(struct weston_animation *animation,
	   struct weston_output *output, uint32_t msecs)
{
	struct weston_compositor *compositor =
		container_of(animation,
			     struct weston_compositor, fade.animation);
	struct weston_surface *surface;

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

struct weston_frame_callback {
	struct wl_resource resource;
	struct wl_list link;
};

static void
weston_output_repaint(struct weston_output *output, int msecs)
{
	struct weston_compositor *ec = output->compositor;
	struct weston_surface *es;
	struct weston_layer *layer;
	struct weston_animation *animation, *next;
	struct weston_frame_callback *cb, *cnext;
	pixman_region32_t opaque, new_damage, output_damage;
	int32_t width, height;

	weston_compositor_update_drag_surfaces(ec);

	width = output->current->width +
		output->border.left + output->border.right;
	height = output->current->height +
		output->border.top + output->border.bottom;
	glViewport(0, 0, width, height);

	/* Rebuild the surface list and update surface transforms up front. */
	wl_list_init(&ec->surface_list);
	wl_list_for_each(layer, &ec->layer_list, link) {
		wl_list_for_each(es, &layer->surface_list, layer_link) {
			weston_surface_update_transform(es);
			wl_list_insert(ec->surface_list.prev, &es->link);
		}
	}

	if (output->assign_planes)
		/*
		 * This will queue flips for the fbs and sprites where
		 * applicable and clear the damage for those surfaces.
		 * The repaint loop below will repaint everything
		 * else.
		 */
		output->assign_planes(output);

	pixman_region32_init(&new_damage);
	pixman_region32_init(&opaque);

	wl_list_for_each(es, &ec->surface_list, link) {
		pixman_region32_subtract(&es->damage, &es->damage, &opaque);
		pixman_region32_union(&new_damage, &new_damage, &es->damage);
		empty_region(&es->damage);
		pixman_region32_copy(&es->clip, &opaque);
		pixman_region32_union(&opaque, &opaque, &es->transform.opaque);
	}

	pixman_region32_union(&ec->damage, &ec->damage, &new_damage);

	pixman_region32_init(&output_damage);
	pixman_region32_union(&output_damage,
			      &ec->damage, &output->previous_damage);
	pixman_region32_copy(&output->previous_damage, &ec->damage);
	pixman_region32_intersect(&output_damage,
				  &output_damage, &output->region);
	pixman_region32_subtract(&ec->damage, &ec->damage, &output->region);

	pixman_region32_fini(&opaque);
	pixman_region32_fini(&new_damage);

	if (output->dirty)
		weston_output_update_matrix(output);

	output->repaint(output, &output_damage);

	pixman_region32_fini(&output_damage);

	output->repaint_needed = 0;

	weston_compositor_repick(ec);
	wl_event_loop_dispatch(ec->input_loop, 0);

	wl_list_for_each_safe(cb, cnext, &output->frame_callback_list, link) {
		wl_callback_send_done(&cb->resource, msecs);
		wl_resource_destroy(&cb->resource);
	}

	wl_list_for_each_safe(animation, next, &ec->animation_list, link)
		animation->frame(animation, output, msecs);
}

static int
weston_compositor_read_input(int fd, uint32_t mask, void *data)
{
	struct weston_compositor *compositor = data;

	wl_event_loop_dispatch(compositor->input_loop, 0);

	return 1;
}

WL_EXPORT void
weston_output_finish_frame(struct weston_output *output, int msecs)
{
	struct weston_compositor *compositor = output->compositor;
	struct wl_event_loop *loop =
		wl_display_get_event_loop(compositor->wl_display);
	int fd;

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
	wl_list_insert(below, &layer->link);
}

WL_EXPORT void
weston_compositor_schedule_repaint(struct weston_compositor *compositor)
{
	struct weston_output *output;
	struct wl_event_loop *loop;

	if (compositor->state == WESTON_COMPOSITOR_SLEEPING)
		return;

	loop = wl_display_get_event_loop(compositor->wl_display);
	wl_list_for_each(output, &compositor->output_list, link) {
		output->repaint_needed = 1;
		if (output->repaint_scheduled)
			continue;

		wl_event_loop_add_idle(loop, idle_repaint, output);
		output->repaint_scheduled = 1;
	}

	if (compositor->input_loop_source) {
		wl_event_source_remove(compositor->input_loop_source);
		compositor->input_loop_source = NULL;
	}
}

WL_EXPORT void
weston_compositor_fade(struct weston_compositor *compositor, float tint)
{
	struct weston_surface *surface;
	int done;

	done = weston_spring_done(&compositor->fade.spring);
	compositor->fade.spring.target = tint;
	if (weston_spring_done(&compositor->fade.spring))
		return;

	if (done)
		compositor->fade.spring.timestamp =
			weston_compositor_get_time();

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
	if (wl_list_empty(&compositor->fade.animation.link))
		wl_list_insert(compositor->animation_list.prev,
			       &compositor->fade.animation.link);
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
	struct wl_resource *resource;
	struct wl_client *client = es->surface.resource.client;

	if (es->surface.resource.client == NULL)
		return;
	if (different == 0)
		return;

	es->output_mask = mask;
	wl_list_for_each(output, &es->compositor->output_list, link) {
		if (1 << output->id & different)
			resource =
				find_resource_for_client(&output->resource_list,
							 client);
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

	weston_surface_update_transform(es);

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

	if (!wl_list_empty(&es->frame_callback_list)) {
		wl_list_insert_list(new_output->frame_callback_list.prev,
				    &es->frame_callback_list);
		wl_list_init(&es->frame_callback_list);
	}
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
texture_set_subimage(struct weston_surface *surface,
		     int32_t x, int32_t y, int32_t width, int32_t height)
{
	glBindTexture(GL_TEXTURE_2D, surface->texture);

#ifdef GL_UNPACK_ROW_LENGTH
	/* Mesa does not define GL_EXT_unpack_subimage */

	if (surface->compositor->has_unpack_subimage) {
		glPixelStorei(GL_UNPACK_ROW_LENGTH, surface->pitch);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height,
				GL_BGRA_EXT, GL_UNSIGNED_BYTE,
				wl_shm_buffer_get_data(surface->buffer));
		return;
	}
#endif

	glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
		     surface->pitch, surface->buffer->height, 0,
		     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
		     wl_shm_buffer_get_data(surface->buffer));
}

static void
surface_damage(struct wl_client *client,
	       struct wl_resource *resource,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct weston_surface *es = resource->data;

	weston_surface_damage_rectangle(es, x, y, width, height);

	if (es->buffer && wl_buffer_is_shm(es->buffer))
		texture_set_subimage(es, x, y, width, height);
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

	if (es->output) {
		wl_list_insert(es->output->frame_callback_list.prev,
			       &cb->link);
	} else {
		wl_list_insert(es->frame_callback_list.prev, &cb->link);
	}
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

	weston_compositor_schedule_repaint(surface->compositor);
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

static  void
weston_input_update_drag_surface(struct wl_input_device *input_device,
				 int dx, int dy);

WL_EXPORT void
notify_motion(struct wl_input_device *device, uint32_t time, int x, int y)
{
	struct weston_output *output;
	const struct wl_pointer_grab_interface *interface;
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *ec = wd->compositor;
	int x_valid = 0, y_valid = 0;
	int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;

	weston_compositor_activity(ec);

	wl_list_for_each(output, &ec->output_list, link) {
		if (output->x <= x && x < output->x + output->current->width)
			x_valid = 1;

		if (output->y <= y && y < output->y + output->current->height)
			y_valid = 1;

		/* FIXME: calculate this only on output addition/deletion */
		if (output->x < min_x)
			min_x = output->x;
		if (output->y < min_y)
			min_y = output->y;

		if (output->x + output->current->width > max_x)
			max_x = output->x + output->current->width - 1;
		if (output->y + output->current->height > max_y)
			max_y = output->y + output->current->height - 1;
	}
	
	if (!x_valid) {
		if (x < min_x)
			x = min_x;
		else if (x >= max_x)
			x = max_x;
	}
	if (!y_valid) {
		if (y < min_y)
			y = min_y;
		else  if (y >= max_y)
			y = max_y;
	}

	weston_input_update_drag_surface(device,
					 x - device->x, y - device->y);

	device->x = x;
	device->y = y;

	wl_list_for_each(output, &ec->output_list, link)
		if (output->zoom.active &&
		    pixman_region32_contains_point(&output->region, x, y, NULL))
			weston_output_update_zoom(output, x, y);

	weston_device_repick(device);
	interface = device->pointer_grab->interface;
	interface->motion(device->pointer_grab, time,
			  device->pointer_grab->x, device->pointer_grab->y);

	if (wd->sprite) {
		weston_surface_set_position(wd->sprite,
					    device->x - wd->hotspot_x,
					    device->y - wd->hotspot_y);

		weston_compositor_schedule_repaint(ec);
	}
}

WL_EXPORT void
weston_surface_activate(struct weston_surface *surface,
			struct weston_input_device *device)
{
	struct weston_compositor *compositor = device->compositor;

	wl_input_device_set_keyboard_focus(&device->input_device,
					   &surface->surface);
	wl_data_device_set_keyboard_focus(&device->input_device);

	wl_signal_emit(&compositor->activate_signal, surface);
}

WL_EXPORT void
notify_button(struct wl_input_device *device,
	      uint32_t time, int32_t button, int32_t state)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *compositor = wd->compositor;
	struct weston_surface *focus = (struct weston_surface *) device->pointer_focus;
	uint32_t serial = wl_display_next_serial(compositor->wl_display);

	if (state) {
		if (compositor->ping_handler && focus)
			compositor->ping_handler(focus, serial);
		weston_compositor_idle_inhibit(compositor);
		if (device->button_count == 0) {
			device->grab_button = button;
			device->grab_time = time;
			device->grab_x = device->x;
			device->grab_y = device->y;
		}
		device->button_count++;
	} else {
		weston_compositor_idle_release(compositor);
		device->button_count--;
	}

	weston_compositor_run_binding(compositor, wd, time, 0, button, 0, state);

	device->pointer_grab->interface->button(device->pointer_grab, time, button, state);

	if (device->button_count == 1)
		device->grab_serial =
			wl_display_get_serial(compositor->wl_display);
}

WL_EXPORT void
notify_axis(struct wl_input_device *device,
	      uint32_t time, uint32_t axis, int32_t value)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *compositor = wd->compositor;
	struct weston_surface *focus = (struct weston_surface *) device->pointer_focus;
	uint32_t serial = wl_display_next_serial(compositor->wl_display);

	if (compositor->ping_handler && focus)
		compositor->ping_handler(focus, serial);

	weston_compositor_activity(compositor);

	if (value)
		weston_compositor_run_binding(compositor, wd,
						time, 0, 0, axis, value);
	else
		return;

	if (device->pointer_focus_resource)
		wl_resource_post_event(device->pointer_focus_resource,
				WL_INPUT_DEVICE_AXIS, time, axis, value);
}

static void
update_modifier_state(struct weston_input_device *device,
		      uint32_t key, uint32_t state)
{
	uint32_t modifier;

	switch (key) {
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		modifier = MODIFIER_CTRL;
		break;

	case KEY_LEFTALT:
	case KEY_RIGHTALT:
		modifier = MODIFIER_ALT;
		break;

	case KEY_LEFTMETA:
	case KEY_RIGHTMETA:
		modifier = MODIFIER_SUPER;
		break;

	default:
		modifier = 0;
		break;
	}

	if (state)
		device->modifier_state |= modifier;
	else
		device->modifier_state &= ~modifier;
}

WL_EXPORT void
notify_key(struct wl_input_device *device,
	   uint32_t time, uint32_t key, uint32_t state)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *compositor = wd->compositor;
	struct weston_surface *focus = (struct weston_surface *) device->pointer_focus;
	uint32_t serial = wl_display_next_serial(compositor->wl_display);
	uint32_t *k, *end;

	if (state) {
		if (compositor->ping_handler && focus)
			compositor->ping_handler(focus, serial);

		weston_compositor_idle_inhibit(compositor);
		device->grab_key = key;
		device->grab_time = time;
	} else {
		weston_compositor_idle_release(compositor);
	}

	update_modifier_state(wd, key, state);
	end = device->keys.data + device->keys.size;
	for (k = device->keys.data; k < end; k++) {
		if (*k == key)
			*k = *--end;
	}
	device->keys.size = (void *) end - device->keys.data;
	if (state) {
		k = wl_array_add(&device->keys, sizeof *k);
		*k = key;
	}

	if (device->keyboard_grab == &device->default_keyboard_grab)
		weston_compositor_run_binding(compositor, wd,
					      time, key, 0, 0, state);

	device->keyboard_grab->interface->key(device->keyboard_grab,
					      time, key, state);
}

WL_EXPORT void
notify_pointer_focus(struct wl_input_device *device,
		     struct weston_output *output, int32_t x, int32_t y)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *compositor = wd->compositor;

	if (output) {
		weston_input_update_drag_surface(device, x - device->x,
						 y - device->y);

		device->x = x;
		device->y = y;
		compositor->focus = 1;
		weston_compositor_repick(compositor);
	} else {
		compositor->focus = 0;
		weston_compositor_repick(compositor);
	}
}

static void
destroy_device_saved_kbd_focus(struct wl_listener *listener, void *data)
{
	struct weston_input_device *wd;

	wd = container_of(listener, struct weston_input_device,
			  saved_kbd_focus_listener);

	wd->saved_kbd_focus = NULL;
}

WL_EXPORT void
notify_keyboard_focus(struct wl_input_device *device, struct wl_array *keys)
{
	struct weston_input_device *wd =
		(struct weston_input_device *) device;
	struct weston_compositor *compositor = wd->compositor;
	struct wl_surface *surface;
	uint32_t *k;

	if (keys) {
		wl_array_copy(&wd->input_device.keys, keys);
		wd->modifier_state = 0;
		wl_array_for_each(k, &device->keys) {
			weston_compositor_idle_inhibit(compositor);
			update_modifier_state(wd, *k, 1);
		}

		surface = wd->saved_kbd_focus;

		if (surface) {
			wl_list_remove(&wd->saved_kbd_focus_listener.link);
			wl_input_device_set_keyboard_focus(&wd->input_device,
							   surface);
			wd->saved_kbd_focus = NULL;
		}
	} else {
		wl_array_for_each(k, &device->keys)
			weston_compositor_idle_release(compositor);

		wd->modifier_state = 0;

		surface = wd->input_device.keyboard_focus;

		if (surface) {
			wd->saved_kbd_focus = surface;
			wd->saved_kbd_focus_listener.notify =
				destroy_device_saved_kbd_focus;
			wl_signal_add(&surface->resource.destroy_signal,
				      &wd->saved_kbd_focus_listener);
		}

		wl_input_device_set_keyboard_focus(&wd->input_device, NULL);
		/* FIXME: We really need keyboard grab cancel here to
		 * let the grab shut down properly.  As it is we leak
		 * the grab data. */
		wl_input_device_end_keyboard_grab(&wd->input_device);
	}
}

static void
lose_touch_focus_resource(struct wl_listener *listener, void *data)
{
	struct weston_input_device *device =
		container_of(listener, struct weston_input_device,
			     touch_focus_resource_listener);

	device->touch_focus_resource = NULL;
}

static void
lose_touch_focus(struct wl_listener *listener, void *data)
{
	struct weston_input_device *device =
		container_of(listener, struct weston_input_device,
			     touch_focus_listener);

	device->touch_focus = NULL;
}

static void
touch_set_focus(struct weston_input_device *device,
		struct wl_surface *surface)
{
	struct wl_input_device *input_device = &device->input_device;
	struct wl_resource *resource;

	if (device->touch_focus == surface)
		return;

	if (surface) {
		resource =
			find_resource_for_client(&input_device->resource_list,
						 surface->resource.client);
		if (!resource) {
			fprintf(stderr, "couldn't find resource\n");
			return;
		}

		device->touch_focus_resource_listener.notify =
			lose_touch_focus_resource;
		wl_signal_add(&resource->destroy_signal,
			      &device->touch_focus_resource_listener);
		device->touch_focus_listener.notify = lose_touch_focus;
		wl_signal_add(&surface->resource.destroy_signal,
			       &device->touch_focus_listener);

		device->touch_focus = surface;
		device->touch_focus_resource = resource;
	} else {
		if (device->touch_focus)
			wl_list_remove(&device->touch_focus_listener.link);
		if (device->touch_focus_resource)
			wl_list_remove(&device->touch_focus_resource_listener.link);
		device->touch_focus = NULL;
		device->touch_focus_resource = NULL;
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
notify_touch(struct wl_input_device *device, uint32_t time, int touch_id,
             int x, int y, int touch_type)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *ec = wd->compositor;
	struct weston_surface *es;
	int32_t sx, sy;
	uint32_t serial = 0;

	switch (touch_type) {
	case WL_INPUT_DEVICE_TOUCH_DOWN:
		weston_compositor_idle_inhibit(ec);

		wd->num_tp++;

		/* the first finger down picks the surface, and all further go
		 * to that surface for the remainder of the touch session i.e.
		 * until all touch points are up again. */
		if (wd->num_tp == 1) {
			es = weston_compositor_pick_surface(ec, x, y, &sx, &sy);
			touch_set_focus(wd, &es->surface);
		} else if (wd->touch_focus) {
			es = (struct weston_surface *) wd->touch_focus;
			weston_surface_from_global(es, x, y, &sx, &sy);
		}

		if (wd->touch_focus_resource && wd->touch_focus)
			wl_input_device_send_touch_down(wd->touch_focus_resource,
							serial, time, &wd->touch_focus->resource,
							touch_id, sx, sy);
		break;
	case WL_INPUT_DEVICE_TOUCH_MOTION:
		es = (struct weston_surface *) wd->touch_focus;
		if (!es)
			break;

		weston_surface_from_global(es, x, y, &sx, &sy);
		if (wd->touch_focus_resource)
			wl_input_device_send_touch_motion(wd->touch_focus_resource,
							  time, touch_id, sx, sy);
		break;
	case WL_INPUT_DEVICE_TOUCH_UP:
		weston_compositor_idle_release(ec);
		wd->num_tp--;

		if (wd->touch_focus_resource)
			wl_input_device_send_touch_up(wd->touch_focus_resource,
						      serial, time, touch_id);
		if (wd->num_tp == 0)
			touch_set_focus(wd, NULL);
		break;
	}
}

static void
input_device_attach(struct wl_client *client,
		    struct wl_resource *resource,
		    uint32_t serial,
		    struct wl_resource *buffer_resource, int32_t x, int32_t y)
{
	struct weston_input_device *device = resource->data;
	struct weston_compositor *compositor = device->compositor;
	struct wl_buffer *buffer = NULL;

	if (serial < device->input_device.pointer_focus_serial)
		return;
	if (device->input_device.pointer_focus == NULL)
		return;
	if (device->input_device.pointer_focus->resource.client != client)
		return;

	if (buffer_resource)
		buffer = buffer_resource->data;

	weston_surface_attach(&device->sprite->surface, buffer);

	if (!buffer)
		return;

	if (!weston_surface_is_mapped(device->sprite)) {
		wl_list_insert(&compositor->cursor_layer.surface_list,
			       &device->sprite->layer_link);
		weston_surface_assign_output(device->sprite);
	}


	device->hotspot_x = x;
	device->hotspot_y = y;
	weston_surface_configure(device->sprite,
				 device->input_device.x - device->hotspot_x,
				 device->input_device.y - device->hotspot_y,
				 buffer->width, buffer->height);

	surface_damage(NULL, &device->sprite->surface.resource,
		       0, 0, buffer->width, buffer->height);
}

static const struct wl_input_device_interface input_device_interface = {
	input_device_attach,
};

static void
handle_drag_surface_destroy(struct wl_listener *listener, void *data)
{
	struct weston_input_device *device;

	device = container_of(listener, struct weston_input_device,
			      drag_surface_destroy_listener);

	device->drag_surface = NULL;
}

static void unbind_resource(struct wl_resource *resource)
{
	wl_list_remove(&resource->link);
	free(resource);
}

static void
bind_input_device(struct wl_client *client,
		  void *data, uint32_t version, uint32_t id)
{
	struct wl_input_device *device = data;
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &wl_input_device_interface,
					&input_device_interface, id, data);
	wl_list_insert(&device->resource_list, &resource->link);
	resource->destroy = unbind_resource;
}

static void
device_handle_new_drag_icon(struct wl_listener *listener, void *data)
{
	struct weston_input_device *device;

	device = container_of(listener, struct weston_input_device,
			      new_drag_icon_listener);

	weston_input_update_drag_surface(&device->input_device, 0, 0);
}

WL_EXPORT void
weston_input_device_init(struct weston_input_device *device,
			 struct weston_compositor *ec)
{
	wl_input_device_init(&device->input_device);

	wl_display_add_global(ec->wl_display, &wl_input_device_interface,
			      device, bind_input_device);

	device->sprite = weston_surface_create(ec);
	device->sprite->surface.resource.data = device->sprite;

	device->compositor = ec;
	device->hotspot_x = 16;
	device->hotspot_y = 16;
	device->modifier_state = 0;
	device->num_tp = 0;

	device->drag_surface_destroy_listener.notify =
		handle_drag_surface_destroy;

	wl_list_insert(ec->input_device_list.prev, &device->link);

	device->new_drag_icon_listener.notify = device_handle_new_drag_icon;
	wl_signal_add(&device->input_device.drag_icon_signal,
		      &device->new_drag_icon_listener);
}

WL_EXPORT void
weston_input_device_release(struct weston_input_device *device)
{
	wl_list_remove(&device->link);
	/* The global object is destroyed at wl_display_destroy() time. */

	if (device->sprite)
		destroy_surface(&device->sprite->surface.resource);

	wl_input_device_release(&device->input_device);
}

static void
drag_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
	weston_surface_configure(es,
				 es->geometry.x + sx, es->geometry.y + sy,
				 es->buffer->width, es->buffer->height);
}

static int
device_setup_new_drag_surface(struct weston_input_device *device,
			      struct weston_surface *surface)
{
	struct wl_input_device *input_device = &device->input_device;

	if (surface->configure) {
		wl_resource_post_error(&surface->surface.resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface->configure already set");
		return 0;
	}

	device->drag_surface = surface;

	weston_surface_set_position(device->drag_surface,
				    input_device->x, input_device->y);

	surface->configure = drag_surface_configure;

	wl_signal_add(&surface->surface.resource.destroy_signal,
		       &device->drag_surface_destroy_listener);

	return 1;
}

static void
device_release_drag_surface(struct weston_input_device *device)
{
	device->drag_surface->configure = NULL;
	undef_region(&device->drag_surface->input);
	wl_list_remove(&device->drag_surface_destroy_listener.link);
	device->drag_surface = NULL;
}

static void
device_map_drag_surface(struct weston_input_device *device)
{
	if (weston_surface_is_mapped(device->drag_surface) ||
	    !device->drag_surface->buffer)
		return;

	wl_list_insert(&device->sprite->layer_link,
		       &device->drag_surface->layer_link);
	weston_surface_assign_output(device->drag_surface);
	empty_region(&device->drag_surface->input);
}

static  void
weston_input_update_drag_surface(struct wl_input_device *input_device,
				 int dx, int dy)
{
	int surface_changed = 0;
	struct weston_input_device *device = (struct weston_input_device *)
		input_device;

	if (!device->drag_surface && !input_device->drag_surface)
		return;

	if (device->drag_surface && input_device->drag_surface &&
	    (&device->drag_surface->surface.resource !=
	     &input_device->drag_surface->resource))
		/* between calls to this funcion we got a new drag_surface */
		surface_changed = 1;

	if (!input_device->drag_surface || surface_changed) {
		device_release_drag_surface(device);
		if (!surface_changed)
			return;
	}

	if (!device->drag_surface || surface_changed) {
		struct weston_surface *surface = (struct weston_surface *)
			input_device->drag_surface;
		if (!device_setup_new_drag_surface(device, surface))
			return;
	}

	/* the client may not have attached a buffer to the drag surface
	 * when we setup it up, so check if map is needed on every update */
	device_map_drag_surface(device);

	/* the client may have attached a buffer with a different size to
	 * the drag surface, causing the input region to be reset */
	if (region_is_undefined(&device->drag_surface->input))
		empty_region(&device->drag_surface->input);

	if (!dx && !dy)
		return;

	weston_surface_set_position(device->drag_surface,
				    device->drag_surface->geometry.x + dx,
				    device->drag_surface->geometry.y + dy);
}

WL_EXPORT void
weston_compositor_update_drag_surfaces(struct weston_compositor *compositor)
{
	weston_input_update_drag_surface(compositor->input_device, 0, 0);
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
				output->make, output->model);

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

static const char texture_fragment_shader[] =
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D tex;\n"
	"uniform float alpha;\n"
	"uniform float bright;\n"
	"uniform float saturation;\n"
	"uniform float texwidth;\n"
	"void main()\n"
	"{\n"
	"   if (v_texcoord.x < 0.0 || v_texcoord.x > texwidth ||\n"
	"       v_texcoord.y < 0.0 || v_texcoord.y > 1.0)\n"
	"      discard;\n"
	"   gl_FragColor = texture2D(tex, v_texcoord)\n;"
	"   float gray = dot(gl_FragColor.rgb, vec3(0.299, 0.587, 0.114));\n"
	"   vec3 range = (gl_FragColor.rgb - vec3 (gray, gray, gray)) * saturation;\n"
	"   gl_FragColor = vec4(vec3(gray + range), gl_FragColor.a);\n"
	"   gl_FragColor = vec4(vec3(bright, bright, bright) * gl_FragColor.rgb, gl_FragColor.a);\n"
	"   gl_FragColor = alpha * gl_FragColor;\n"
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
		fprintf(stderr, "shader info: %s\n", msg);
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
		fprintf(stderr, "link info: %s\n", msg);
		return -1;
	}

	shader->proj_uniform = glGetUniformLocation(shader->program, "proj");
	shader->tex_uniform = glGetUniformLocation(shader->program, "tex");
	shader->alpha_uniform = glGetUniformLocation(shader->program, "alpha");
	shader->brightness_uniform = glGetUniformLocation(shader->program, "bright");
	shader->saturation_uniform = glGetUniformLocation(shader->program, "saturation");
	shader->color_uniform = glGetUniformLocation(shader->program, "color");
	shader->texwidth_uniform = glGetUniformLocation(shader->program,
							"texwidth");

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

WL_EXPORT void
weston_output_update_zoom(struct weston_output *output, int x, int y)
{
	float ratio;

	if (output->zoom.level <= 0)
		return;

	output->zoom.magnification = 1 / output->zoom.level;
	ratio = 1 - (1 / output->zoom.magnification);

	output->zoom.trans_x = (((float)(x - output->x) / output->current->width) * (ratio * 2)) - ratio;
	output->zoom.trans_y = (((float)(y - output->y) / output->current->height) * (ratio * 2)) - ratio;

	output->dirty = 1;
	weston_output_damage(output);
}

WL_EXPORT void
weston_output_update_matrix(struct weston_output *output)
{
	int flip;
	struct weston_matrix camera;
	struct weston_matrix modelview;

	weston_matrix_init(&output->matrix);
	weston_matrix_translate(&output->matrix,
				-(output->x + (output->border.right + output->current->width - output->border.left) / 2.0),
				-(output->y + (output->border.bottom + output->current->height - output->border.top) / 2.0), 0);

	flip = (output->flags & WL_OUTPUT_FLIPPED) ? -1 : 1;
	weston_matrix_scale(&output->matrix,
			    2.0 / (output->current->width + output->border.left + output->border.right),
			    flip * 2.0 / (output->current->height + output->border.top + output->border.bottom), 1);
	if (output->zoom.active) {
		weston_matrix_init(&camera);
		weston_matrix_init(&modelview);
		weston_matrix_translate(&camera, output->zoom.trans_x, flip * output->zoom.trans_y, 0);
		weston_matrix_invert(&modelview, &camera);
		weston_matrix_scale(&modelview, output->zoom.magnification, output->zoom.magnification, 1.0);
		weston_matrix_multiply(&output->matrix, &modelview);
	}

	output->dirty = 0;
}

WL_EXPORT void
weston_output_move(struct weston_output *output, int x, int y)
{
	output->x = x;
	output->y = y;

	pixman_region32_init(&output->previous_damage);
	pixman_region32_init_rect(&output->region, x, y,
				  output->current->width,
				  output->current->height);
}

WL_EXPORT void
weston_output_init(struct weston_output *output, struct weston_compositor *c,
		   int x, int y, int width, int height, uint32_t flags)
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
	wl_list_init(&output->read_pixels_list);

	output->zoom.active = 0;
	output->zoom.increment = 0.05;
	output->zoom.level = 1.0;
	output->zoom.magnification = 1.0;
	output->zoom.trans_x = 0.0;
	output->zoom.trans_y = 0.0;

	output->flags = flags;
	weston_output_move(output, x, y);
	weston_output_damage(output);

	wl_list_init(&output->frame_callback_list);
	wl_list_init(&output->resource_list);

	output->id = ffs(~output->compositor->output_id_pool) - 1;
	output->compositor->output_id_pool |= 1 << output->id;

	output->global =
		wl_display_add_global(c->wl_display, &wl_output_interface,
				      output, bind_output);
}

WL_EXPORT void
weston_output_do_read_pixels(struct weston_output *output)
{
	struct weston_read_pixels *r, *next;

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	wl_list_for_each_safe(r, next, &output->read_pixels_list, link) {
		glReadPixels(r->x, r->y, r->width, r->height,
			     output->compositor->read_format,
			     GL_UNSIGNED_BYTE, r->data);
		r->done(r, output);
	}
}

static void
compositor_bind(struct wl_client *client,
		void *data, uint32_t version, uint32_t id)
{
	struct weston_compositor *compositor = data;

	wl_client_add_object(client, &wl_compositor_interface,
			     &compositor_interface, id, compositor);
}

WL_EXPORT int
weston_compositor_init(struct weston_compositor *ec, struct wl_display *display)
{
	struct wl_event_loop *loop;
	const char *extensions;

	ec->wl_display = display;
	wl_signal_init(&ec->destroy_signal);
	wl_signal_init(&ec->activate_signal);
	wl_signal_init(&ec->lock_signal);
	wl_signal_init(&ec->unlock_signal);
	ec->launcher_sock = weston_environment_get_fd("WESTON_LAUNCHER_SOCK");

	ec->output_id_pool = 0;

	if (!wl_display_add_global(display, &wl_compositor_interface,
				   ec, compositor_bind))
		return -1;

	wl_display_init_shm(display);

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

	extensions = (const char *) glGetString(GL_EXTENSIONS);
	if (!extensions) {
		fprintf(stderr, "Retrieving GL extension string failed.\n");
		return -1;
	}

	if (!strstr(extensions, "GL_EXT_texture_format_BGRA8888")) {
		fprintf(stderr,
			"GL_EXT_texture_format_BGRA8888 not available\n");
		return -1;
	}

	if (strstr(extensions, "GL_EXT_read_format_bgra"))
		ec->read_format = GL_BGRA_EXT;
	else
		ec->read_format = GL_RGBA;

	if (strstr(extensions, "GL_EXT_unpack_subimage"))
		ec->has_unpack_subimage = 1;

	extensions =
		(const char *) eglQueryString(ec->display, EGL_EXTENSIONS);
	if (!extensions) {
		fprintf(stderr, "Retrieving EGL extension string failed.\n");
		return -1;
	}

	if (strstr(extensions, "EGL_WL_bind_wayland_display"))
		ec->has_bind_display = 1;
	if (ec->has_bind_display)
		ec->bind_display(ec->display, ec->wl_display);

	wl_list_init(&ec->surface_list);
	wl_list_init(&ec->layer_list);
	wl_list_init(&ec->input_device_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->binding_list);
	wl_list_init(&ec->animation_list);
	weston_spring_init(&ec->fade.spring, 30.0, 1.0, 1.0);
	ec->fade.animation.frame = fade_frame;
	wl_list_init(&ec->fade.animation.link);

	weston_layer_init(&ec->fade_layer, &ec->layer_list);
	weston_layer_init(&ec->cursor_layer, &ec->fade_layer.link);

	screenshooter_create(ec);

	ec->ping_handler = NULL;

	wl_data_device_manager_init(ec->wl_display);

	glActiveTexture(GL_TEXTURE0);

	if (weston_shader_init(&ec->texture_shader,
			     vertex_shader, texture_fragment_shader) < 0)
		return -1;
	if (weston_shader_init(&ec->solid_shader,
			     vertex_shader, solid_fragment_shader) < 0)
		return -1;

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->idle_source = wl_event_loop_add_timer(loop, idle_handler, ec);
	wl_event_source_timer_update(ec->idle_source, ec->idle_time * 1000);

	ec->input_loop = wl_event_loop_create();

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

	weston_binding_list_destroy_all(&ec->binding_list);

	wl_array_release(&ec->vertices);
	wl_array_release(&ec->indices);

	wl_event_loop_destroy(ec->input_loop);
}

static int on_term_signal(int signal_number, void *data)
{
	struct wl_display *display = data;

	fprintf(stderr, "caught signal %d\n", signal_number);
	wl_display_terminate(display);

	return 1;
}

static void
on_segv_signal(int s, siginfo_t *siginfo, void *context)
{
	void *buffer[32];
	int i, count;
	Dl_info info;

	fprintf(stderr, "caught segv\n");

	count = backtrace(buffer, ARRAY_LENGTH(buffer));
	for (i = 0; i < count; i++) {
		dladdr(buffer[i], &info);
		fprintf(stderr, "  [%016lx]  %s  (%s)\n",
			(long) buffer[i],
			info.dli_sname ? info.dli_sname : "--",
			info.dli_fname);
	}

	longjmp(segv_jmp_buf, 1);
}


static void *
load_module(const char *name, const char *entrypoint, void **handle)
{
	char path[PATH_MAX];
	void *module, *init;

	if (name[0] != '/')
		snprintf(path, sizeof path, MODULEDIR "/%s", name);
	else
		snprintf(path, sizeof path, "%s", name);

	module = dlopen(path, RTLD_LAZY);
	if (!module) {
		fprintf(stderr,
			"failed to load module: %s\n", dlerror());
		return NULL;
	}

	init = dlsym(module, entrypoint);
	if (!init) {
		fprintf(stderr,
			"failed to lookup init function: %s\n", dlerror());
		return NULL;
	}

	return init;
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct weston_compositor *ec;
	struct wl_event_source *signals[4];
	struct wl_event_loop *loop;
	struct sigaction segv_action;
	void *shell_module, *backend_module, *xserver_module;
	int (*shell_init)(struct weston_compositor *ec);
	int (*xserver_init)(struct weston_compositor *ec);
	struct weston_compositor
		*(*backend_init)(struct wl_display *display,
				 int argc, char *argv[]);
	int i;
	char *backend = NULL;
	char *shell = NULL;
	int32_t idle_time = 300;
	int32_t xserver;
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
	};

	argc = parse_options(core_options,
			     ARRAY_LENGTH(core_options), argc, argv);

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

	segv_action.sa_flags = SA_SIGINFO | SA_RESETHAND;
	segv_action.sa_sigaction = on_segv_signal;
	sigemptyset(&segv_action.sa_mask);
	sigaction(SIGSEGV, &segv_action, NULL);

	if (!backend) {
		if (getenv("WAYLAND_DISPLAY"))
			backend = "wayland-backend.so";
		else if (getenv("DISPLAY"))
			backend = "x11-backend.so";
		else if (getenv("OPENWFD"))
			backend = "openwfd-backend.so";
		else
			backend = "drm-backend.so";
	}

	config_file = config_file_path("weston.ini");
	parse_config_file(config_file, cs, ARRAY_LENGTH(cs), shell);
	free(config_file);

	if (!shell)
		shell = "desktop-shell.so";

	backend_init = load_module(backend, "backend_init", &backend_module);
	if (!backend_init)
		exit(EXIT_FAILURE);

	shell_init = load_module(shell, "shell_init", &shell_module);
	if (!shell_init)
		exit(EXIT_FAILURE);

	ec = backend_init(display, argc, argv);
	if (ec == NULL) {
		fprintf(stderr, "failed to create compositor\n");
		exit(EXIT_FAILURE);
	}

	for (i = 1; argv[i]; i++)
		fprintf(stderr, "unhandled option: %s\n", argv[i]);
	if (argv[1])
		exit(EXIT_FAILURE);

	ec->option_idle_time = idle_time;
	ec->idle_time = idle_time;

	xserver_init = NULL;
	if (xserver)
		xserver_init = load_module("xserver-launcher.so",
					   "weston_xserver_init",
					   &xserver_module);
	if (xserver_init)
		xserver_init(ec);

	if (shell_init(ec) < 0)
		exit(EXIT_FAILURE);

	if (wl_display_add_socket(display, socket_name)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	weston_compositor_dpms_on(ec);
	weston_compositor_wake(ec);
	if (setjmp(segv_jmp_buf) == 0)
		wl_display_run(display);

	/* prevent further rendering while shutting down */
	ec->state = WESTON_COMPOSITOR_SLEEPING;

	wl_signal_emit(&ec->destroy_signal, ec);

	if (ec->has_bind_display)
		ec->unbind_display(ec->display, display);

	for (i = ARRAY_LENGTH(signals); i;)
		wl_event_source_remove(signals[--i]);

	ec->destroy(ec);
	wl_display_destroy(display);

	return 0;
}
