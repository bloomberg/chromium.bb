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
#include <getopt.h>
#include <signal.h>
#include <setjmp.h>
#include <execinfo.h>

#include <wayland-server.h>
#include "compositor.h"

static const char *option_socket_name = NULL;

static struct wl_list child_process_list;
static jmp_buf segv_jmp_buf;

static int
sigchld_handler(int signal_number, void *data)
{
	struct weston_process *p;
	int status;
	pid_t pid;

	pid = wait(&status);
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
surface_handle_buffer_destroy(struct wl_listener *listener,
			      struct wl_resource *resource, uint32_t time)
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

	wl_list_init(&surface->surface.resource.destroy_listener_list);

	wl_list_init(&surface->link);
	wl_list_init(&surface->buffer_link);

	surface->surface.resource.client = NULL;

	surface->compositor = compositor;
	surface->image = EGL_NO_IMAGE_KHR;
	surface->alpha = 255;

	surface->buffer = NULL;
	surface->output = NULL;
	surface->force_configure = 0;

	pixman_region32_init(&surface->damage);
	pixman_region32_init(&surface->opaque);
	pixman_region32_init(&surface->clip);
	undef_region(&surface->input);
	pixman_region32_init(&surface->transform.opaque);
	wl_list_init(&surface->frame_callback_list);

	surface->buffer_destroy_listener.func = surface_handle_buffer_destroy;

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
	struct weston_surface *below;

	if (surface->output == NULL)
		return;

	if (surface->link.next == &surface->compositor->surface_list)
		return;

	below = container_of(surface->link.next, struct weston_surface, link);
	pixman_region32_union(&below->damage, &below->damage,
			      &surface->transform.boundingbox);
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

	if (surface->output)
		weston_surface_assign_output(surface);

	weston_compositor_schedule_repaint(surface->compositor);
}

WL_EXPORT void
weston_surface_to_global(struct weston_surface *surface,
			 int32_t sx, int32_t sy, int32_t *x, int32_t *y)
{
	GLfloat xf, yf;

	weston_surface_update_transform(surface);

	surface_to_global_float(surface, sx, sy, &xf, &yf);
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

static void
weston_surface_flush_damage(struct weston_surface *surface)
{
	struct weston_surface *below;

	if (surface->output &&
	    surface->link.next != &surface->compositor->surface_list) {
		below = container_of(surface->link.next,
				     struct weston_surface, link);

		pixman_region32_union(&below->damage,
				      &below->damage, &surface->damage);
	}
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
weston_device_repick(struct wl_input_device *device, uint32_t time)
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
		interface->focus(device->pointer_grab, time, &surface->surface,
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
	uint32_t time;

	if (!compositor->focus)
		return;

	time = weston_compositor_get_time();
	wl_list_for_each(device, &compositor->input_device_list, link)
		weston_device_repick(&device->input_device, time);
}

static void
weston_surface_unmap(struct weston_surface *surface)
{
	weston_surface_damage_below(surface);
	weston_surface_flush_damage(surface);
	surface->output = NULL;
	wl_list_remove(&surface->link);
	weston_compositor_repick(surface->compositor);
	weston_compositor_schedule_repaint(surface->compositor);
}

static void
destroy_surface(struct wl_resource *resource)
{
	struct weston_surface *surface =
		container_of(resource,
			     struct weston_surface, surface.resource);
	struct weston_compositor *compositor = surface->compositor;

	if (surface->output)
		weston_surface_unmap(surface);

	if (surface->texture)
		glDeleteTextures(1, &surface->texture);

	if (surface->buffer)
		wl_list_remove(&surface->buffer_destroy_listener.link);

	if (surface->image != EGL_NO_IMAGE_KHR)
		compositor->destroy_image(compositor->display,
					  surface->image);

	wl_list_remove(&surface->buffer_link);

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
weston_buffer_attach(struct wl_buffer *buffer, struct wl_surface *surface)
{
	struct weston_surface *es = (struct weston_surface *) surface;
	struct weston_compositor *ec = es->compositor;
	struct wl_list *surfaces_attached_to;

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
			     es->pitch, buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			     wl_shm_buffer_get_data(buffer));

		surfaces_attached_to = buffer->user_data;

		wl_list_remove(&es->buffer_link);
		wl_list_insert(surfaces_attached_to, &es->buffer_link);
	} else {
		if (es->image != EGL_NO_IMAGE_KHR)
			ec->destroy_image(ec->display, es->image);
		es->image = ec->create_image(ec->display, NULL,
					     EGL_WAYLAND_BUFFER_WL,
					     buffer, NULL);
		
		ec->image_target_texture_2d(GL_TEXTURE_2D, es->image);

		es->pitch = es->geometry.width;
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

WL_EXPORT struct wl_list *
weston_compositor_top(struct weston_compositor *compositor)
{
	struct weston_input_device *input_device;
	struct wl_list *list;

	input_device = (struct weston_input_device *) compositor->input_device;

	/* Insert below pointer */
	list = &compositor->surface_list;
	if (compositor->fade.surface &&
	    list->next == &compositor->fade.surface->link)
		list = list->next;
	if (list->next == &input_device->sprite->link)
		list = list->next;
	if (input_device->drag_surface &&
	    list->next == &input_device->drag_surface->link)
		list = list->next;

	return list;
}

static void
weston_surface_raise(struct weston_surface *surface)
{
	struct weston_compositor *compositor = surface->compositor;
	struct wl_list *list = weston_compositor_top(compositor);

	wl_list_remove(&surface->link);
	wl_list_insert(list, &surface->link);
	weston_compositor_repick(compositor);
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
	struct weston_surface *es;

	if (wl_list_empty(&compositor->surface_list))
		return;

	es = container_of(compositor->surface_list.next,
			  struct weston_surface, link);
	pixman_region32_union(&es->damage, &es->damage, &output->region);
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
			compositor->shell->lock(compositor->shell);
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

	wl_list_for_each(es, &ec->surface_list, link)
		/* Update surface transform now to avoid calling it ever
		 * again from the repaint sub-functions. */
		weston_surface_update_transform(es);

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

	wl_list_for_each_safe(cb, cnext, &output->frame_callback_list, link) {
		wl_resource_post_event(&cb->resource, WL_CALLBACK_DONE, msecs);
		wl_resource_destroy(&cb->resource, 0);
	}

	wl_list_for_each_safe(animation, next, &ec->animation_list, link)
		animation->frame(animation, output, msecs);
}

static void
idle_repaint(void *data)
{
	struct weston_output *output = data;

	/* An idle repaint may have been cancelled by vt switching away. */
	if (output->repaint_needed)
		weston_output_repaint(output, weston_compositor_get_time());
	else
		output->repaint_scheduled = 0;
}

WL_EXPORT void
weston_output_finish_frame(struct weston_output *output, int msecs)
{
	if (output->repaint_needed)
		weston_output_repaint(output, msecs);
	else
		output->repaint_scheduled = 0;
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
		wl_list_insert(&compositor->surface_list, &surface->link);
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
	wl_resource_destroy(resource, weston_compositor_get_time());
}

WL_EXPORT void
weston_surface_assign_output(struct weston_surface *es)
{
	struct weston_compositor *ec = es->compositor;
	struct weston_output *output, *new_output;
	pixman_region32_t region;
	uint32_t max, area;
	pixman_box32_t *e;

	weston_surface_update_transform(es);

	new_output = NULL;
	max = 0;
	pixman_region32_init(&region);
	wl_list_for_each(output, &ec->output_list, link) {
		pixman_region32_intersect(&region, &es->transform.boundingbox,
					  &output->region);

		e = pixman_region32_extents(&region);
		area = (e->x2 - e->x1) * (e->y2 - e->y1);

		if (area >= max) {
			new_output = output;
			max = area;
		}
	}
	pixman_region32_fini(&region);

	es->output = new_output;
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
	struct weston_shell *shell = es->compositor->shell;
	struct wl_buffer *buffer;

	if (!buffer_resource && !es->output)
		return;

	if (es->buffer) {
		weston_buffer_post_release(es->buffer);
		wl_list_remove(&es->buffer_destroy_listener.link);
	}

	if (!buffer_resource && es->output) {
		weston_surface_unmap(es);
		es->buffer = NULL;
		return;
	}

	buffer = buffer_resource->data;
	buffer->busy_count++;
	es->buffer = buffer;
	wl_list_insert(es->buffer->resource.destroy_listener_list.prev,
		       &es->buffer_destroy_listener.link);

	if (es->geometry.width != buffer->width ||
	    es->geometry.height != buffer->height) {
		undef_region(&es->input);
		pixman_region32_fini(&es->opaque);
		pixman_region32_init(&es->opaque);
	}

	if (es->output == NULL) {
		shell->map(shell, es, buffer->width, buffer->height, sx, sy);
	} else if (es->force_configure || sx != 0 || sy != 0 ||
		   es->geometry.width != buffer->width ||
		   es->geometry.height != buffer->height) {
		GLfloat from_x, from_y;
		GLfloat to_x, to_y;

		surface_to_global_float(es, 0, 0, &from_x, &from_y);
		surface_to_global_float(es, sx, sy, &to_x, &to_y);
		shell->configure(shell, es,
				 es->geometry.x + to_x - from_x,
				 es->geometry.y + to_y - from_y,
				 buffer->width, buffer->height);
		es->force_configure = 0;
	}

	weston_buffer_attach(buffer, &es->surface);
}

static void
surface_damage(struct wl_client *client,
	       struct wl_resource *resource,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct weston_surface *es = resource->data;

	weston_surface_damage_rectangle(es, x, y, width, height);
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

	weston_compositor_repick(surface->compositor);
}

const static struct wl_surface_interface surface_interface = {
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
	wl_resource_destroy(resource, weston_compositor_get_time());
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

const static struct wl_compositor_interface compositor_interface = {
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
		compositor->shell->unlock(compositor->shell);
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

	weston_device_repick(device, time);
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
			struct weston_input_device *device, uint32_t time)
{
	weston_surface_raise(surface);
	wl_input_device_set_keyboard_focus(&device->input_device,
					   &surface->surface, time);
	wl_data_device_set_keyboard_focus(&device->input_device);
}

WL_EXPORT void
notify_button(struct wl_input_device *device,
	      uint32_t time, int32_t button, int32_t state)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *compositor = wd->compositor;

	if (state) {
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

	weston_compositor_run_binding(compositor, wd, time, 0, button, state);

	device->pointer_grab->interface->button(device->pointer_grab, time, button, state);

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
	uint32_t *k, *end;

	if (state) {
		weston_compositor_idle_inhibit(compositor);
		device->grab_key = key;
		device->grab_time = time;
	} else {
		weston_compositor_idle_release(compositor);
	}

	if (device->keyboard_grab == &device->default_keyboard_grab)
		weston_compositor_run_binding(compositor, wd,
					      time, key, 0, state);

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

	device->keyboard_grab->interface->key(device->keyboard_grab,
					      time, key, state);
}

WL_EXPORT void
notify_pointer_focus(struct wl_input_device *device,
		     uint32_t time, struct weston_output *output,
		     int32_t x, int32_t y)
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

WL_EXPORT void
notify_keyboard_focus(struct wl_input_device *device,
		      uint32_t time, struct weston_output *output,
		      struct wl_array *keys)
{
	struct weston_input_device *wd =
		(struct weston_input_device *) device;
	struct weston_compositor *compositor = wd->compositor;
	struct weston_surface *es;
	uint32_t *k, *end;

	if (!wl_list_empty(&compositor->surface_list))
		es = container_of(compositor->surface_list.next,
				  struct weston_surface, link);
	else
		es = NULL;

	if (output) {
		wl_array_copy(&wd->input_device.keys, keys);
		wd->modifier_state = 0;
		end = device->keys.data + device->keys.size;
		for (k = device->keys.data; k < end; k++) {
			weston_compositor_idle_inhibit(compositor);
			update_modifier_state(wd, *k, 1);
		}

		if (es && es->surface.resource.client)
			wl_input_device_set_keyboard_focus(&wd->input_device,
							   &es->surface, time);
	} else {
		end = device->keys.data + device->keys.size;
		for (k = device->keys.data; k < end; k++)
			weston_compositor_idle_release(compositor);

		wd->modifier_state = 0;
		wl_input_device_set_keyboard_focus(&wd->input_device,
						   NULL, time);
	}
}

/* TODO: share this function with wayland-server.c */
static struct wl_resource *
find_resource_for_surface(struct wl_list *list, struct wl_surface *surface)
{
        struct wl_resource *r;

        if (!surface)
                return NULL;

        wl_list_for_each(r, list, link) {
                if (r->client == surface->resource.client)
                        return r;
        }

        return NULL;
}

static void
lose_touch_focus_resource(struct wl_listener *listener,
			  struct wl_resource *resource, uint32_t time)
{
	struct weston_input_device *device =
		container_of(listener, struct weston_input_device,
			     touch_focus_resource_listener);

	device->touch_focus_resource = NULL;
}

static void
lose_touch_focus(struct wl_listener *listener,
		 struct wl_resource *resource, uint32_t time)
{
	struct weston_input_device *device =
		container_of(listener, struct weston_input_device,
			     touch_focus_listener);

	device->touch_focus = NULL;
}

static void
touch_set_focus(struct weston_input_device *device,
		struct wl_surface *surface, uint32_t time)
{
	struct wl_input_device *input_device = &device->input_device;
	struct wl_resource *resource;

	if (device->touch_focus == surface)
		return;

	if (surface) {
		resource =
			find_resource_for_surface(&input_device->resource_list,
						  surface);
		if (!resource) {
			fprintf(stderr, "couldn't find resource\n");
			return;
		}

		device->touch_focus_resource_listener.func =
			lose_touch_focus_resource;
		wl_list_insert(resource->destroy_listener_list.prev,
			       &device->touch_focus_resource_listener.link);
		device->touch_focus_listener.func = lose_touch_focus;
		wl_list_insert(surface->resource.destroy_listener_list.prev,
			       &device->touch_focus_listener.link);

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

	switch (touch_type) {
	case WL_INPUT_DEVICE_TOUCH_DOWN:
		weston_compositor_idle_inhibit(ec);

		wd->num_tp++;

		/* the first finger down picks the surface, and all further go
		 * to that surface for the remainder of the touch session i.e.
		 * until all touch points are up again. */
		if (wd->num_tp == 1) {
			es = weston_compositor_pick_surface(ec, x, y, &sx, &sy);
			touch_set_focus(wd, &es->surface, time);
		} else if (wd->touch_focus) {
			es = (struct weston_surface *) wd->touch_focus;
			weston_surface_from_global(es, x, y, &sx, &sy);
		}

		if (wd->touch_focus_resource && wd->touch_focus)
			wl_resource_post_event(wd->touch_focus_resource,
					       touch_type, time,
					       wd->touch_focus,
					       touch_id, sx, sy);
		break;
	case WL_INPUT_DEVICE_TOUCH_MOTION:
		es = (struct weston_surface *) wd->touch_focus;
		if (!es)
			break;

		weston_surface_from_global(es, x, y, &sx, &sy);
		if (wd->touch_focus_resource)
			wl_resource_post_event(wd->touch_focus_resource,
					       touch_type, time,
					       touch_id, sx, sy);
		break;
	case WL_INPUT_DEVICE_TOUCH_UP:
		weston_compositor_idle_release(ec);
		wd->num_tp--;

		if (wd->touch_focus_resource)
			wl_resource_post_event(wd->touch_focus_resource,
					       touch_type, time, touch_id);
		if (wd->num_tp == 0)
			touch_set_focus(wd, NULL, time);
		break;
	}
}

static void
input_device_attach(struct wl_client *client,
		    struct wl_resource *resource,
		    uint32_t time,
		    struct wl_resource *buffer_resource, int32_t x, int32_t y)
{
	struct weston_input_device *device = resource->data;
	struct weston_compositor *compositor = device->compositor;
	struct wl_buffer *buffer;

	if (time < device->input_device.pointer_focus_time)
		return;
	if (device->input_device.pointer_focus == NULL)
		return;
	if (device->input_device.pointer_focus->resource.client != client)
		return;

	if (!buffer_resource && device->sprite->output) {
		wl_list_remove(&device->sprite->link);
		device->sprite->output = NULL;
		return;
	}

	if (!device->sprite->output) {
		wl_list_insert(&compositor->surface_list,
			       &device->sprite->link);
		weston_surface_assign_output(device->sprite);
	}

	buffer = buffer_resource->data;
	device->hotspot_x = x;
	device->hotspot_y = y;
	weston_surface_configure(device->sprite,
				 device->input_device.x - device->hotspot_x,
				 device->input_device.y - device->hotspot_y,
				 buffer->width, buffer->height);

	weston_buffer_attach(buffer, &device->sprite->surface);
}

const static struct wl_input_device_interface input_device_interface = {
	input_device_attach,
};

static void unbind_input_device(struct wl_resource *resource)
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
	resource->destroy = unbind_input_device;
}

WL_EXPORT void
weston_input_device_init(struct weston_input_device *device,
			 struct weston_compositor *ec)
{
	wl_input_device_init(&device->input_device);

	wl_display_add_global(ec->wl_display, &wl_input_device_interface,
			      device, bind_input_device);

	device->sprite = weston_surface_create(ec);

	device->compositor = ec;
	device->hotspot_x = 16;
	device->hotspot_y = 16;
	device->modifier_state = 0;
	device->num_tp = 0;

	wl_list_insert(ec->input_device_list.prev, &device->link);
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
		undef_region(&device->drag_surface->input);
		device->drag_surface = NULL;
		if (!surface_changed)
			return;
	}

	if (!device->drag_surface || surface_changed) {
		device->drag_surface = (struct weston_surface *)
			input_device->drag_surface;

		weston_surface_set_position(device->drag_surface,
					    input_device->x, input_device->y);
	}

	if (device->drag_surface->output == NULL &&
	    device->drag_surface->buffer) {
		wl_list_insert(&device->sprite->link,
			       &device->drag_surface->link);
		weston_surface_assign_output(device->drag_surface);
		empty_region(&device->drag_surface->input);
	}

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

	wl_resource_post_event(resource,
			       WL_OUTPUT_GEOMETRY,
			       output->x,
			       output->y,
			       output->mm_width,
			       output->mm_height,
			       output->subpixel,
			       output->make, output->model);

	wl_list_for_each (mode, &output->mode_list, link) {
		wl_resource_post_event(resource,
				       WL_OUTPUT_MODE,
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
	"uniform float texwidth;\n"
	"void main()\n"
	"{\n"
	"   if (v_texcoord.x < 0.0 || v_texcoord.x > texwidth ||\n"
	"       v_texcoord.y < 0.0 || v_texcoord.y > 1.0)\n"
	"      discard;\n"
	"   gl_FragColor = texture2D(tex, v_texcoord)\n;"
	"   gl_FragColor = alpha * gl_FragColor;\n"
	"}\n";

static const char solid_fragment_shader[] =
	"precision mediump float;\n"
	"uniform vec4 color;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = color\n;"
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
	shader->color_uniform = glGetUniformLocation(shader->program, "color");
	shader->texwidth_uniform = glGetUniformLocation(shader->program,
							"texwidth");

	return 0;
}

WL_EXPORT void
weston_output_destroy(struct weston_output *output)
{
	pixman_region32_fini(&output->region);
	pixman_region32_fini(&output->previous_damage);
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

	output->zoom.active = 0;
	output->zoom.increment = 0.05;
	output->zoom.level = 1.0;
	output->zoom.magnification = 1.0;
	output->zoom.trans_x = 0.0;
	output->zoom.trans_y = 0.0;

	output->flags = flags;
	weston_output_move(output, x, y);

	wl_list_init(&output->frame_callback_list);

	wl_display_add_global(c->wl_display,
			      &wl_output_interface, output, bind_output);
}

static void
shm_buffer_created(struct wl_buffer *buffer)
{
	struct wl_list *surfaces_attached_to;

	surfaces_attached_to = malloc(sizeof *surfaces_attached_to);
	if (!surfaces_attached_to) {
		buffer->user_data = NULL;
		return;
	}

	wl_list_init(surfaces_attached_to);

	buffer->user_data = surfaces_attached_to;
}

static void
shm_buffer_damaged(struct wl_buffer *buffer,
		   int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wl_list *surfaces_attached_to = buffer->user_data;
	struct weston_surface *es;
	GLsizei tex_width = wl_shm_buffer_get_stride(buffer) / 4;

	wl_list_for_each(es, surfaces_attached_to, buffer_link) {
		glBindTexture(GL_TEXTURE_2D, es->texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     tex_width, buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			     wl_shm_buffer_get_data(buffer));
		/* Hmm, should use glTexSubImage2D() here but GLES2 doesn't
		 * support any unpack attributes except GL_UNPACK_ALIGNMENT. */
	}
}

static void
shm_buffer_destroyed(struct wl_buffer *buffer)
{
	struct wl_list *surfaces_attached_to = buffer->user_data;
	struct weston_surface *es, *next;

	wl_list_for_each_safe(es, next, surfaces_attached_to, buffer_link) {
		wl_list_remove(&es->buffer_link);
		wl_list_init(&es->buffer_link);
	}

	free(surfaces_attached_to);
}

const static struct wl_shm_callbacks shm_callbacks = {
	shm_buffer_created,
	shm_buffer_damaged,
	shm_buffer_destroyed
};

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

	if (!wl_display_add_global(display, &wl_compositor_interface,
				   ec, compositor_bind))
		return -1;

	ec->shm = wl_shm_init(display, &shm_callbacks);

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
	if (!strstr(extensions, "GL_EXT_texture_format_BGRA8888")) {
		fprintf(stderr,
			"GL_EXT_texture_format_BGRA8888 not available\n");
		return -1;
	}

	extensions =
		(const char *) eglQueryString(ec->display, EGL_EXTENSIONS);
	if (strstr(extensions, "EGL_WL_bind_wayland_display"))
		ec->has_bind_display = 1;
	if (ec->has_bind_display)
		ec->bind_display(ec->display, ec->wl_display);

	wl_list_init(&ec->surface_list);
	wl_list_init(&ec->input_device_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->binding_list);
	wl_list_init(&ec->animation_list);
	weston_spring_init(&ec->fade.spring, 30.0, 1.0, 1.0);
	ec->fade.animation.frame = fade_frame;
	wl_list_init(&ec->fade.animation.link);

	ec->screenshooter = screenshooter_create(ec);

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

	weston_compositor_schedule_repaint(ec);

	return 0;
}

WL_EXPORT void
weston_compositor_shutdown(struct weston_compositor *ec)
{
	struct weston_output *output, *next;

	wl_event_source_remove(ec->idle_source);

	if (ec->screenshooter)
		screenshooter_destroy(ec->screenshooter);

	/* Destroy all outputs associated with this compositor */
	wl_list_for_each_safe(output, next, &ec->output_list, link)
		output->destroy(output);

	weston_binding_list_destroy_all(&ec->binding_list);

	wl_shm_finish(ec->shm);

	wl_array_release(&ec->vertices);
	wl_array_release(&ec->indices);
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
	int o, xserver = 0;
	void *shell_module, *backend_module;
	int (*shell_init)(struct weston_compositor *ec);
	struct weston_compositor
		*(*backend_init)(struct wl_display *display, char *options);
	char *backend = NULL;
	char *backend_options = "";
	char *shell = NULL;
	char *p;
	int option_idle_time = 300;
	int i;

	static const char opts[] = "B:b:o:S:i:s:x";
	static const struct option longopts[ ] = {
		{ "backend", 1, NULL, 'B' },
		{ "backend-options", 1, NULL, 'o' },
		{ "socket", 1, NULL, 'S' },
		{ "idle-time", 1, NULL, 'i' },
		{ "shell", 1, NULL, 's' },
		{ "xserver", 0, NULL, 'x' },
		{ NULL, }
	};

	while (o = getopt_long(argc, argv, opts, longopts, &o), o > 0) {
		switch (o) {
		case 'B':
			backend = optarg;
			break;
		case 'o':
			backend_options = optarg;
			break;
		case 'S':
			option_socket_name = optarg;
			break;
		case 'i':
			option_idle_time = strtol(optarg, &p, 0);
			if (*p != '\0') {
				fprintf(stderr,
					"invalid idle time option: %s\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 's':
			shell = optarg;
			break;
		case 'x':
			xserver = 1;
			break;
		}
	}

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

	if (!shell)
		shell = "desktop-shell.so";

	backend_init = load_module(backend, "backend_init", &backend_module);
	if (!backend_init)
		exit(EXIT_FAILURE);

	shell_init = load_module(shell, "shell_init", &shell_module);
	if (!shell_init)
		exit(EXIT_FAILURE);

	ec = backend_init(display, backend_options);
	if (ec == NULL) {
		fprintf(stderr, "failed to create compositor\n");
		exit(EXIT_FAILURE);
	}

	ec->option_idle_time = option_idle_time;
	ec->idle_time = option_idle_time;

	if (xserver)
		weston_xserver_init(ec);

	if (shell_init(ec) < 0)
		exit(EXIT_FAILURE);

	if (wl_display_add_socket(display, option_socket_name)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	weston_compositor_dpms_on(ec);
	weston_compositor_wake(ec);
	if (setjmp(segv_jmp_buf) == 0)
		wl_display_run(display);

	/* prevent further rendering while shutting down */
	ec->state = WESTON_COMPOSITOR_SLEEPING;

	if (xserver)
		weston_xserver_destroy(ec);

	ec->shell->destroy(ec->shell);

	if (ec->has_bind_display)
		ec->unbind_display(ec->display, display);

	for (i = ARRAY_LENGTH(signals); i;)
		wl_event_source_remove(signals[--i]);

	ec->destroy(ec);
	wl_display_destroy(display);

	return 0;
}
