/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <termios.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <sys/poll.h>
#include <png.h>
#include <math.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <xf86drmMode.h>
#include <time.h>
#include <fnmatch.h>
#include <dirent.h>

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>

#include <GL/gl.h>
#include <eagle.h>

#include "wayland.h"
#include "wayland-protocol.h"
#include "cairo-util.h"
#include "wayland-system-compositor.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct wl_visual {
	struct wl_object base;
};

struct wl_output {
	struct wl_object base;
};

struct wlsc_input_device {
	struct wl_object base;
	int32_t x, y;
	struct egl_compositor *ec;
	struct egl_surface *pointer_surface;
	struct wl_list link;

	int grab;
	struct egl_surface *grab_surface;
	struct egl_surface *focus_surface;
};

struct egl_compositor {
	struct wl_compositor base;
	struct wl_output output;
	struct wl_visual argb_visual, premultiplied_argb_visual, rgb_visual;

	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGLConfig config;
	struct wl_display *wl_display;
	int width, height, stride;
	struct egl_surface *background;

	struct wl_list input_device_list;
	struct wl_list surface_list;

	struct wl_event_source *term_signal_source;

        /* tty handling state */
	int tty_fd;
	uint32_t vt_active : 1;

	struct termios terminal_attributes;
	struct wl_event_source *tty_input_source;
	struct wl_event_source *enter_vt_source;
	struct wl_event_source *leave_vt_source;

	/* Modesetting info. */
	struct drm_mode_modeinfo *mode;
	uint32_t fb_id;
	uint32_t crtc_id;
	uint32_t connector_id;

	struct udev *udev;

	/* Repaint state. */
	struct wl_event_source *timer_source;
	int repaint_needed;
	int repaint_on_timeout;
	struct timespec previous_swap;
	uint32_t current_frame;
};

struct egl_surface {
	struct wl_surface base;
	struct egl_compositor *compositor;
	struct wl_visual *visual;
	GLuint texture;
	struct wl_map map;
	EGLSurface surface;
	int width, height;
	struct wl_list link;
};

struct screenshooter {
	struct wl_object base;
	struct egl_compositor *ec;
};

struct screenshooter_interface {
	void (*shoot)(struct wl_client *client, struct screenshooter *shooter);
};

static void
screenshooter_shoot(struct wl_client *client, struct screenshooter *shooter)
{
	struct egl_compositor *ec = shooter->ec;
	GLuint stride;
	static const char filename[]  = "wayland-screenshot.png";
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	void *data;

	data = eglReadBuffer(ec->display, ec->surface, GL_FRONT_LEFT, &stride);
	pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE,
					  8, ec->width, ec->height, stride,
					  NULL, NULL);
	gdk_pixbuf_save(pixbuf, filename, "png", &error, NULL);
}

static const struct wl_message screenshooter_methods[] = {
	{ "shoot", "", NULL }
};

static const struct wl_interface screenshooter_interface = {
	"screenshooter", 1,
	ARRAY_LENGTH(screenshooter_methods),
	screenshooter_methods,
};

struct screenshooter_interface screenshooter_implementation = {
	screenshooter_shoot
};

static struct screenshooter *
screenshooter_create(struct egl_compositor *ec)
{
	struct screenshooter *shooter;

	shooter = malloc(sizeof *shooter);
	if (shooter == NULL)
		return NULL;

	shooter->base.interface = &screenshooter_interface;
	shooter->base.implementation = (void(**)(void)) &screenshooter_implementation;
	shooter->ec = ec;

	return shooter;
};

static struct egl_surface *
egl_surface_create_from_cairo_surface(struct egl_compositor *ec,
				      cairo_surface_t *surface,
				      int x, int y, int width, int height)
{
	struct egl_surface *es;
	int stride;
	void *data;

	stride = cairo_image_surface_get_stride(surface);
	data = cairo_image_surface_get_data(surface);

	es = malloc(sizeof *es);
	if (es == NULL)
		return NULL;

	glGenTextures(1, &es->texture);
	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		     GL_BGRA, GL_UNSIGNED_BYTE, data);

	es->compositor = ec;
	es->map.x = x;
	es->map.y = y;
	es->map.width = width;
	es->map.height = height;
	es->surface = EGL_NO_SURFACE;
	es->visual = &ec->premultiplied_argb_visual;

	return es;
}

static void
egl_surface_destroy(struct egl_surface *es, struct egl_compositor *ec)
{
	glDeleteTextures(1, &es->texture);
	if (es->surface != EGL_NO_SURFACE)
		eglDestroySurface(ec->display, es->surface);
	free(es);
}

static void
pointer_path(cairo_t *cr, int x, int y)
{
	const int end = 3, tx = 4, ty = 12, dx = 5, dy = 10;
	const int width = 16, height = 16;

	cairo_move_to(cr, x, y);
	cairo_line_to(cr, x + tx, y + ty);
	cairo_line_to(cr, x + dx, y + dy);
	cairo_line_to(cr, x + width - end, y + height);
	cairo_line_to(cr, x + width, y + height - end);
	cairo_line_to(cr, x + dy, y + dx);
	cairo_line_to(cr, x + ty, y + tx);
	cairo_close_path(cr);
}

static struct egl_surface *
pointer_create(struct egl_compositor *ec, int x, int y, int width, int height)
{
	struct egl_surface *es;
	const int hotspot_x = 16, hotspot_y = 16;
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					     width, height);

	cr = cairo_create(surface);
	pointer_path(cr, hotspot_x + 5, hotspot_y + 4);
	cairo_set_line_width(cr, 2);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);
	blur_surface(surface, width);

	pointer_path(cr, hotspot_x, hotspot_y);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
	cairo_destroy(cr);

	es = egl_surface_create_from_cairo_surface(ec,
						   surface,
						   x - hotspot_x,
						   y - hotspot_y,
						   width, height);
	
	cairo_surface_destroy(surface);

	return es;
}

static struct egl_surface *
background_create(struct egl_compositor *ec,
		  const char *filename, int width, int height)
{
	struct egl_surface *background;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	void *data;
	GLenum format;

	background = malloc(sizeof *background);
	if (background == NULL)
		return NULL;
	
	g_type_init();

	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
						   width, height,
						   FALSE, &error);
	if (error != NULL) {
		free(background);
		return NULL;
	}

	data = gdk_pixbuf_get_pixels(pixbuf);

	glGenTextures(1, &background->texture);
	glBindTexture(GL_TEXTURE_2D, background->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        if (gdk_pixbuf_get_has_alpha(pixbuf))
		format = GL_RGBA;
	else
		format = GL_RGB;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
		     format, GL_UNSIGNED_BYTE, data);

	background->compositor = ec;
	background->map.x = 0;
	background->map.y = 0;
	background->map.width = width;
	background->map.height = height;
	background->surface = EGL_NO_SURFACE;
	background->visual = &ec->rgb_visual;

	return background;
}

static void
draw_surface(struct egl_surface *es)
{
	struct egl_compositor *ec = es->compositor;
	GLint vertices[12];
	GLint tex_coords[12] = { 0, 0,  0, 1,  1, 0,  1, 1 };
	GLuint indices[4] = { 0, 1, 2, 3 };

	vertices[0] = es->map.x;
	vertices[1] = es->map.y;
	vertices[2] = 0;

	vertices[3] = es->map.x;
	vertices[4] = es->map.y + es->map.height;
	vertices[5] = 0;

	vertices[6] = es->map.x + es->map.width;
	vertices[7] = es->map.y;
	vertices[8] = 0;

	vertices[9] = es->map.x + es->map.width;
	vertices[10] = es->map.y + es->map.height;
	vertices[11] = 0;

	if (es->visual == &ec->argb_visual) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else if (es->visual == &ec->premultiplied_argb_visual) {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_INT, 0, vertices);
	glTexCoordPointer(2, GL_INT, 0, tex_coords);
	glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, indices);
}

static void
schedule_repaint(struct egl_compositor *ec);

static void
repaint(void *data)
{
	struct egl_compositor *ec = data;
	struct egl_surface *es;
	struct wlsc_input_device *eid;
	struct timespec ts;
	uint32_t msecs;

	if (!ec->repaint_needed) {
		ec->repaint_on_timeout = 0;
		return;
	}

	if (ec->background)
		draw_surface(ec->background);
	else
		glClear(GL_COLOR_BUFFER_BIT);

	es = container_of(ec->surface_list.next,
			  struct egl_surface, link);
	while (&es->link != &ec->surface_list) {
		draw_surface(es);

		es = container_of(es->link.next,
				   struct egl_surface, link);
	}

	eid = container_of(ec->input_device_list.next,
			   struct wlsc_input_device, link);
	while (&eid->link != &ec->input_device_list) {
		draw_surface(eid->pointer_surface);

		eid = container_of(eid->link.next,
				   struct wlsc_input_device, link);
	}

	eglSwapBuffers(ec->display, ec->surface);
	ec->repaint_needed = 0;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	msecs = ts.tv_sec * 1000 + ts.tv_nsec / (1000 * 1000);
	wl_display_post_frame(ec->wl_display, &ec->base,
			      ec->current_frame, msecs);
	ec->current_frame++;

	wl_event_source_timer_update(ec->timer_source, 10);
	ec->repaint_on_timeout = 1;
}

static void
schedule_repaint(struct egl_compositor *ec)
{
	struct wl_event_loop *loop;

	ec->repaint_needed = 1;
	if (!ec->repaint_on_timeout) {
		loop = wl_display_get_event_loop(ec->wl_display);
		wl_event_loop_add_idle(loop, repaint, ec);
	}
}
				   
static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	struct egl_surface *es = (struct egl_surface *) surface;
	struct egl_compositor *ec = es->compositor;

	wl_list_remove(&es->link);
	egl_surface_destroy(es, ec);

	schedule_repaint(ec);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, uint32_t name, 
	       uint32_t width, uint32_t height, uint32_t stride,
	       struct wl_object *visual)
{
	struct egl_surface *es = (struct egl_surface *) surface;
	struct egl_compositor *ec = es->compositor;

	if (es->surface != EGL_NO_SURFACE)
		eglDestroySurface(ec->display, es->surface);

	es->width = width;
	es->height = height;
	es->surface = eglCreateSurfaceForName(ec->display, ec->config,
					      name, width, height, stride, NULL);
	if (visual == &ec->argb_visual.base)
		es->visual = &ec->argb_visual;
	else if (visual == &ec->premultiplied_argb_visual.base)
		es->visual = &ec->premultiplied_argb_visual;
	else if (visual == &ec->rgb_visual.base)
		es->visual = &ec->rgb_visual;
	else
		/* FIXME: Smack client with an exception event */;

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	eglBindTexImage(ec->display, es->surface, GL_TEXTURE_2D);
}

static void
surface_map(struct wl_client *client,
	    struct wl_surface *surface,
	    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct egl_surface *es = (struct egl_surface *) surface;

	es->map.x = x;
	es->map.y = y;
	es->map.width = width;
	es->map.height = height;
}

static void
surface_copy(struct wl_client *client,
	     struct wl_surface *surface,
	     int32_t dst_x, int32_t dst_y,
	     uint32_t name, uint32_t stride,
	     int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct egl_surface *es = (struct egl_surface *) surface;
	struct egl_compositor *ec = es->compositor;
	EGLSurface src;

	/* FIXME: glCopyPixels should work, but then we'll have to
	 * call eglMakeCurrent to set up the src and dest surfaces
	 * first.  This seems cheaper, but maybe there's a better way
	 * to accomplish this. */

	src = eglCreateSurfaceForName(ec->display, ec->config,
				      name, x + width, y + height, stride, NULL);

	eglCopyNativeBuffers(ec->display, es->surface, GL_FRONT_LEFT, dst_x, dst_y,
			     src, GL_FRONT_LEFT, x, y, width, height);
	eglDestroySurface(ec->display, src);
}

static void
surface_damage(struct wl_client *client,
	       struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	/* FIXME: This need to take a damage region, of course. */
}

const static struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_map,
	surface_copy,
	surface_damage
};

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct egl_surface *es;

	es = malloc(sizeof *es);
	if (es == NULL)
		/* FIXME: Send OOM event. */
		return;

	es->compositor = ec;
	es->surface = EGL_NO_SURFACE;
	wl_list_insert(ec->surface_list.prev, &es->link);
	glGenTextures(1, &es->texture);
	wl_client_add_surface(client, &es->base,
			      &surface_interface, id);
}

static void
compositor_commit(struct wl_client *client,
		  struct wl_compositor *compositor, uint32_t key)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;

	schedule_repaint(ec);
	wl_client_send_acknowledge(client, compositor, key, ec->current_frame);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
	compositor_commit
};

static struct egl_surface *
pick_surface(struct wlsc_input_device *device, int32_t *sx, int32_t *sy)
{
	struct egl_compositor *ec = device->ec;
	struct egl_surface *es;

	if (device->grab > 0)
		return device->grab_surface;

	es = container_of(ec->surface_list.prev,
			  struct egl_surface, link);
	while (&es->link != &ec->surface_list) {
		if (es->map.x <= device->x &&
		    device->x < es->map.x + es->map.width &&
		    es->map.y <= device->y &&
		    device->y < es->map.y + es->map.height)
			return es;

		es = container_of(es->link.prev,
				  struct egl_surface, link);

		/* Transform to surface coordinates. */
		*sx = (device->x - es->map.x) * es->width / es->map.width;
		*sy = (device->y - es->map.y) * es->height / es->map.height;
	}

	return NULL;
}

void
notify_motion(struct wlsc_input_device *device, int x, int y)
{
	struct egl_surface *es;
	struct egl_compositor *ec = device->ec;
	const int hotspot_x = 16, hotspot_y = 16;
	int32_t sx, sy;

	if (!ec->vt_active)
		return;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x >= ec->width)
		x = ec->width - 1;
	if (y >= ec->height)
		y = ec->height - 1;

	device->x = x;
	device->y = y;
	es = pick_surface(device, &sx, &sy);
	if (es)
		wl_surface_post_event(&es->base, &device->base,
				      WL_INPUT_MOTION, x, y, sx, sy);

	device->pointer_surface->map.x = x - hotspot_x;
	device->pointer_surface->map.y = y - hotspot_y;

	schedule_repaint(device->ec);
}

void
notify_button(struct wlsc_input_device *device,
	      int32_t button, int32_t state)
{
	struct egl_surface *es;
	struct egl_compositor *ec = device->ec;
	int32_t sx, sy;

	if (!ec->vt_active)
		return;

	es = pick_surface(device, &sx, &sy);
	if (es) {
		wl_list_remove(&es->link);
		wl_list_insert(device->ec->surface_list.prev, &es->link);

		if (state) {
			/* FIXME: We need callbacks when the surfaces
			 * we reference here go away. */
			device->grab++;
			device->grab_surface = es;
			device->focus_surface = es;
		} else {
			device->grab--;
		}

		/* FIXME: Swallow click on raise? */
		wl_surface_post_event(&es->base, &device->base,
				      WL_INPUT_BUTTON, button, state,
				      device->x, device->y, sx, sy);

		schedule_repaint(device->ec);
	}
}

void
notify_key(struct wlsc_input_device *device,
	   uint32_t key, uint32_t state)
{
	struct egl_compositor *ec = device->ec;

	if (!ec->vt_active)
		return;

	if (device->focus_surface != NULL)
		wl_surface_post_event(&device->focus_surface->base,
				      &device->base, 
				      WL_INPUT_KEY, key, state);
}

struct evdev_input_device *
evdev_input_device_create(struct wlsc_input_device *device,
			  struct wl_display *display, const char *path);

static struct wlsc_input_device *
create_input_device(struct egl_compositor *ec)
{
	struct wlsc_input_device *device;

	device = malloc(sizeof *device);
	if (device == NULL)
		return NULL;

	memset(device, 0, sizeof *device);
	device->base.interface = &wl_input_device_interface;
	device->base.implementation = NULL;
	wl_display_add_object(ec->wl_display, &device->base);
	wl_display_add_global(ec->wl_display, &device->base, NULL);
	device->x = 100;
	device->y = 100;
	device->pointer_surface =
		pointer_create(ec, device->x, device->y, 64, 64);
	device->ec = ec;

	wl_list_insert(ec->input_device_list.prev, &device->link);

	return device;
}

void
wlsc_device_get_position(struct wlsc_input_device *device, int32_t *x, int32_t *y)
{
	*x = device->x;
	*y = device->y;
}

static uint32_t
create_frontbuffer(struct egl_compositor *ec)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	drmModeEncoder *encoder;
	struct drm_mode_modeinfo *mode;
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	int i, ret, fd;

	fd = eglGetDisplayFD(ec->display);
	resources = drmModeGetResources(fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return 0;
	}

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(fd, resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
		    connector->count_modes > 0)
			break;

		drmModeFreeConnector(connector);
	}

	if (i == resources->count_connectors) {
		fprintf(stderr, "No currently active connector found.\n");
		return -1;
	}

	mode = &connector->modes[0];

	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(fd, resources->encoders[i]);

		if (encoder == NULL)
			continue;

		if (encoder->encoder_id == connector->encoder_id)
			break;

		drmModeFreeEncoder(encoder);
	}

	/* Mode size at 32 bpp */
	create.size = mode->hdisplay * mode->vdisplay * 4;
	if (ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
		fprintf(stderr, "gem create failed: %m\n");
		return 0;
	}

	ret = drmModeAddFB(fd, mode->hdisplay, mode->vdisplay,
			   32, 32, mode->hdisplay * 4, create.handle, &ec->fb_id);
	if (ret) {
		fprintf(stderr, "failed to add fb: %m\n");
		return 0;
	}

	ret = drmModeSetCrtc(fd, encoder->crtc_id, ec->fb_id, 0, 0,
			     &connector->connector_id, 1, mode);
	if (ret) {
		fprintf(stderr, "failed to set mode: %m\n");
		return 0;
	}

	flink.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
		fprintf(stderr, "gem flink failed: %m\n");
		return 0;
	}

	ec->crtc_id = encoder->crtc_id;
	ec->connector_id = connector->connector_id;
	ec->mode = mode;
	ec->width = mode->hdisplay;
	ec->height = mode->vdisplay;
	ec->stride = mode->hdisplay * 4;

	return flink.name;
}

static const struct wl_interface visual_interface = {
	"visual", 1,
};

static void
add_visuals(struct egl_compositor *ec)
{
	ec->argb_visual.base.interface = &visual_interface;
	ec->argb_visual.base.implementation = NULL;
	wl_display_add_object(ec->wl_display, &ec->argb_visual.base);
	wl_display_add_global(ec->wl_display, &ec->argb_visual.base, NULL);

	ec->premultiplied_argb_visual.base.interface = &visual_interface;
	ec->premultiplied_argb_visual.base.implementation = NULL;
	wl_display_add_object(ec->wl_display,
			      &ec->premultiplied_argb_visual.base);
	wl_display_add_global(ec->wl_display,
			      &ec->premultiplied_argb_visual.base, NULL);

	ec->rgb_visual.base.interface = &visual_interface;
	ec->rgb_visual.base.implementation = NULL;
	wl_display_add_object(ec->wl_display, &ec->rgb_visual.base);
	wl_display_add_global(ec->wl_display, &ec->rgb_visual.base, NULL);
}

static void
post_output_geometry(struct wl_client *client, struct wl_object *global)
{
	struct egl_compositor *ec =
		container_of(global, struct egl_compositor, output.base);

	wl_client_post_event(client, global,
			     WL_OUTPUT_GEOMETRY, ec->width, ec->height);
}

static const char gem_device[] = "/dev/dri/card0";

static const char *option_background = "background.jpg";

static const GOptionEntry option_entries[] = {
	{ "background", 'b', 0, G_OPTION_ARG_STRING,
	  &option_background, "Background image" },
	{ NULL }
};

static void on_enter_vt(int signal_number, void *data)
{
	struct egl_compositor *ec = data;
	int ret, fd;

	ioctl(ec->tty_fd, VT_RELDISP, VT_ACKACQ);
	ec->vt_active = TRUE;

	fd = eglGetDisplayFD(ec->display);
	ret = drmModeSetCrtc(fd, ec->crtc_id, ec->fb_id, 0, 0,
			     &ec->connector_id, 1, ec->mode);
	if (ret) {
		fprintf(stderr, "failed to set mode: %m\n");
		return;
	}
}

static void on_leave_vt(int signal_number, void *data)
{
	struct egl_compositor *ec = data;

	ioctl (ec->tty_fd, VT_RELDISP, 1);
	ec->vt_active = FALSE;
}

static void
on_tty_input(int fd, uint32_t mask, void *data)
{
	struct egl_compositor *ec = data;

	/* Ignore input to tty.  We get keyboard events from evdev
	 */
	tcflush(ec->tty_fd, TCIFLUSH);
}

static void on_term_signal(int signal_number, void *data)
{
	struct egl_compositor *ec = data;

	if (tcsetattr(ec->tty_fd, TCSANOW, &ec->terminal_attributes) < 0)
		fprintf(stderr, "could not restore terminal to canonical mode\n");

	exit(0);
}

static int setup_tty(struct egl_compositor *ec, struct wl_event_loop *loop)
{
	struct termios raw_attributes;
	struct vt_mode mode = { 0 };

	ec->tty_fd = open("/dev/tty0", O_RDWR | O_NOCTTY);
	if (ec->tty_fd <= 0) {
		fprintf(stderr, "failed to open active tty: %m\n");
		return -1;
	}

	if (tcgetattr(ec->tty_fd, &ec->terminal_attributes) < 0) {
		fprintf(stderr, "could not get terminal attributes: %m\n");
		return -1;
	}

	/* Ignore control characters and disable echo */
	raw_attributes = ec->terminal_attributes;
	cfmakeraw(&raw_attributes);

	/* Fix up line endings to be normal (cfmakeraw hoses them) */
	raw_attributes.c_oflag |= OPOST | OCRNL;

	if (tcsetattr(ec->tty_fd, TCSANOW, &raw_attributes) < 0)
		fprintf(stderr, "could not put terminal into raw mode: %m\n");

	ec->term_signal_source =
		wl_event_loop_add_signal(loop, SIGTERM, on_term_signal, ec);

	ec->tty_input_source =
		wl_event_loop_add_fd(loop, ec->tty_fd,
				     WL_EVENT_READABLE, on_tty_input, ec);

	ec->vt_active = TRUE;
	mode.mode = VT_PROCESS;
	mode.relsig = SIGUSR1;
	mode.acqsig = SIGUSR2;
	if (!ioctl(ec->tty_fd, VT_SETMODE, &mode) < 0) {
		fprintf(stderr, "failed to take control of vt handling\n");
	}

	ec->leave_vt_source =
		wl_event_loop_add_signal(loop, SIGUSR1, on_leave_vt, ec);
	ec->enter_vt_source =
		wl_event_loop_add_signal(loop, SIGUSR2, on_enter_vt, ec);

	return 0;
}

static const char *
get_udev_property(struct udev_device *device, const char *name)
{
        struct udev_list_entry *entry;

	udev_list_entry_foreach(entry, udev_device_get_properties_list_entry(device))
		if (strcmp(udev_list_entry_get_name(entry), name) == 0)
			return udev_list_entry_get_value(entry);

	return NULL;
}

static void
init_libudev(struct egl_compositor *ec)
{
	struct udev_enumerate *e;
        struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path, *seat;
	struct wlsc_input_device *input_device;

	/* FIXME: Newer (version 135+) udev has two new features that
	 * make all this much easier: 1) we can enumerate by a
	 * specific property.  This lets us directly iterate through
	 * the devices we care about. 2) We can attach properties to
	 * sysfs nodes without a device file, which lets us configure
	 * which connectors belong to a seat instead of tagging the
	 * overall drm node.  I don't want to update my system udev,
	 * so I'm going to stick with this until the new version is in
	 * rawhide. */

	ec->udev = udev_new();
	if (ec->udev == NULL) {
		fprintf(stderr, "failed to initialize udev context\n");
		return;
	}

	input_device = create_input_device(ec);

	e = udev_enumerate_new(ec->udev);
        udev_enumerate_scan_devices(e);
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(ec->udev, path);

		/* FIXME: Should the property namespace be CK for console kit? */
		seat = get_udev_property(device, "WAYLAND_SEAT");
		if (!seat || strcmp(seat, "1") != 0)
			continue;
		if (strcmp(udev_device_get_subsystem(device), "input") == 0) {
			evdev_input_device_create(input_device, ec->wl_display,
						  udev_device_get_devnode(device));
			continue;
		}
	}
        udev_enumerate_unref(e);
}

static struct egl_compositor *
egl_compositor_create(struct wl_display *display)
{
	static const EGLint config_attribs[] = {
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_CONFIG_CAVEAT, EGL_NONE,
		EGL_NONE		
	};

	const static EGLint attribs[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
		EGL_NONE
	};

	EGLint major, minor;
	struct egl_compositor *ec;
	struct screenshooter *shooter;
	uint32_t fb_name;
	struct wl_event_loop *loop;

	ec = malloc(sizeof *ec);
	if (ec == NULL)
		return NULL;

	ec->wl_display = display;

	ec->display = eglCreateDisplayNative(gem_device, "i965");
	if (ec->display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return NULL;
	}

	if (!eglInitialize(ec->display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return NULL;
	}

	if (!eglChooseConfig(ec->display, config_attribs, &ec->config, 1, NULL))
		return NULL;
 
	fb_name = create_frontbuffer(ec);
	ec->surface = eglCreateSurfaceForName(ec->display, ec->config,
					      fb_name, ec->width, ec->height, ec->stride, attribs);
	if (ec->surface == NULL) {
		fprintf(stderr, "failed to create surface\n");
		return NULL;
	}

	ec->context = eglCreateContext(ec->display, ec->config, NULL, NULL);
	if (ec->context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return NULL;
	}

	if (!eglMakeCurrent(ec->display, ec->surface, ec->surface, ec->context)) {
		fprintf(stderr, "failed to make context current\n");
		return NULL;
	}

	glViewport(0, 0, ec->width, ec->height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, ec->width, ec->height, 0, 0, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glClearColor(0, 0, 0.2, 1);

	wl_display_set_compositor(display, &ec->base, &compositor_interface); 

	/* FIXME: This needs to be much more expressive... something like randr 1.2. */
	ec->output.base.interface = &wl_output_interface;
	wl_display_add_object(display, &ec->output.base);
	wl_display_add_global(display, &ec->output.base, post_output_geometry);

	add_visuals(ec);

	wl_list_init(&ec->input_device_list);
	init_libudev(ec);

	wl_list_init(&ec->surface_list);
	ec->background = background_create(ec, option_background,
					   ec->width, ec->height);

	shooter = screenshooter_create(ec);
	wl_display_add_object(display, &shooter->base);
	wl_display_add_global(display, &shooter->base, NULL);

	loop = wl_display_get_event_loop(ec->wl_display);

	setup_tty(ec, loop);

	ec->timer_source = wl_event_loop_add_timer(loop, repaint, ec);
	ec->repaint_needed = 0;
	ec->repaint_on_timeout = 0;

	schedule_repaint(ec);

	return ec;
}

/* The plan here is to generate a random anonymous socket name and
 * advertise that through a service on the session dbus.
 */
static const char socket_name[] = "\0wayland";

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct egl_compositor *ec;
	GError *error = NULL;
	GOptionContext *context;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, option_entries, "Wayland");
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		exit(EXIT_FAILURE);
	}

	display = wl_display_create();

	ec = egl_compositor_create(display);
	if (ec == NULL) {
		fprintf(stderr, "failed to create compositor\n");
		exit(EXIT_FAILURE);
	}
		
	if (wl_display_add_socket(display, socket_name, sizeof socket_name)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	wl_display_run(display);

	return 0;
}
