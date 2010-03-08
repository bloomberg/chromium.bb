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
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <time.h>

#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "wayland.h"
#include "wayland-protocol.h"
#include "cairo-util.h"
#include "wayland-system-compositor.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct wlsc_matrix {
	GLdouble d[16];
};

struct wl_visual {
	struct wl_object base;
};

struct wlsc_surface;

struct wlsc_listener {
	struct wl_list link;
	void (*func)(struct wlsc_listener *listener,
		     struct wlsc_surface *surface);
};

struct wlsc_output {
	struct wl_object base;
	struct wl_list link;
	struct wlsc_compositor *compositor;
	struct wlsc_surface *background;
	int32_t x, y, width, height;

	drmModeModeInfo mode;
	uint32_t crtc_id;
	uint32_t connector_id;

	GLuint rbo[2];
	uint32_t fb_id[2];
	EGLImageKHR image[2];
	uint32_t current;	
};

struct wlsc_input_device {
	struct wl_object base;
	int32_t x, y;
	struct wlsc_compositor *ec;
	struct wlsc_surface *sprite;
	struct wl_list link;

	int grab;
	struct wlsc_surface *grab_surface;
	struct wlsc_surface *pointer_focus;
	struct wlsc_surface *keyboard_focus;
	struct wl_array keys;

	struct wlsc_listener listener;
};

struct wlsc_compositor {
	struct wl_compositor base;
	struct wl_visual argb_visual, premultiplied_argb_visual, rgb_visual;

	EGLDisplay display;
	EGLContext context;
	int drm_fd;
	GLuint fbo;
	struct wl_display *wl_display;

	struct wl_list output_list;
	struct wl_list input_device_list;
	struct wl_list surface_list;

	struct wl_list surface_destroy_listener_list;

	struct wl_event_source *term_signal_source;

        /* tty handling state */
	int tty_fd;
	uint32_t vt_active : 1;

	struct termios terminal_attributes;
	struct wl_event_source *tty_input_source;
	struct wl_event_source *enter_vt_source;
	struct wl_event_source *leave_vt_source;

	struct udev *udev;

	/* Repaint state. */
	struct wl_event_source *timer_source;
	int repaint_needed;
	int repaint_on_timeout;
	struct timespec previous_swap;
	uint32_t current_frame;
	struct wl_event_source *drm_source;

	uint32_t modifier_state;
};

#define MODIFIER_CTRL	(1 << 8)
#define MODIFIER_ALT	(1 << 9)

struct wlsc_vector {
	GLdouble x, y, z;
};

struct wlsc_surface {
	struct wl_surface base;
	struct wlsc_compositor *compositor;
	struct wl_visual *visual;
	GLuint texture;
	EGLImageKHR image;
	struct wl_map map;
	int width, height;
	struct wl_list link;
	struct wlsc_matrix matrix;
};

static const char *option_background = "background.jpg";
static int option_connector = 0;

static const GOptionEntry option_entries[] = {
	{ "background", 'b', 0, G_OPTION_ARG_STRING,
	  &option_background, "Background image" },
	{ "connector", 'c', 0, G_OPTION_ARG_INT,
	  &option_connector, "KMS connector" },
	{ NULL }
};

struct screenshooter {
	struct wl_object base;
	struct wlsc_compositor *ec;
};

struct screenshooter_interface {
	void (*shoot)(struct wl_client *client, struct screenshooter *shooter);
};

static void
screenshooter_shoot(struct wl_client *client, struct screenshooter *shooter)
{
	struct wlsc_compositor *ec = shooter->ec;
	struct wlsc_output *output;
	char buffer[256];
	GdkPixbuf *pixbuf, *normal;
	GError *error = NULL;
	unsigned char *data;
	int i, j;

	i = 0;
	wl_list_for_each(output, &ec->output_list, link) {
		snprintf(buffer, sizeof buffer, "wayland-screenshot-%d.png", i++);
		data = malloc(output->width * output->height * 4);
		if (data == NULL) {
			fprintf(stderr, "couldn't allocate image buffer\n");
			continue;
		}

		glReadBuffer(GL_FRONT);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, output->width, output->height,
			     GL_RGBA, GL_UNSIGNED_BYTE, data);

		/* FIXME: We should just use a RGB visual for the frontbuffer. */
		for (j = 3; j < output->width * output->height * 4; j += 4)
			data[j] = 0xff;

		pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE,
						  8, output->width, output->height, output->width * 4,
						  NULL, NULL);
		normal = gdk_pixbuf_flip(pixbuf, FALSE);
		gdk_pixbuf_save(normal, buffer, "png", &error, NULL);
		gdk_pixbuf_unref(normal);
		gdk_pixbuf_unref(pixbuf);
		free(data);
	}
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
screenshooter_create(struct wlsc_compositor *ec)
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
				   
static void
wlsc_matrix_init(struct wlsc_matrix *matrix)
{
	static const struct wlsc_matrix identity = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 }
	};

	memcpy(matrix, &identity, sizeof identity);
}

static void
wlsc_matrix_multiply(struct wlsc_matrix *m, const struct wlsc_matrix *n)
{
	struct wlsc_matrix tmp;
	const GLdouble *row, *column;
	div_t d;
	int i, j;

	for (i = 0; i < 16; i++) {
		tmp.d[i] = 0;
		d = div(i, 4);
		row = m->d + d.quot * 4;
		column = n->d + d.rem;
		for (j = 0; j < 4; j++)
			tmp.d[i] += row[j] * column[j * 4];
	}
	memcpy(m, &tmp, sizeof tmp);
}

static void
wlsc_matrix_translate(struct wlsc_matrix *matrix, GLdouble x, GLdouble y, GLdouble z)
{
	struct wlsc_matrix translate = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 }
	};

	wlsc_matrix_multiply(matrix, &translate);
}

static void
wlsc_matrix_scale(struct wlsc_matrix *matrix, GLdouble x, GLdouble y, GLdouble z)
{
	struct wlsc_matrix scale = {
		{ x, 0, 0, 0,  0, y, 0, 0,  0, 0, z, 0,  0, 0, 0, 1 }
	};

	wlsc_matrix_multiply(matrix, &scale);
}

static void
wlsc_matrix_rotate(struct wlsc_matrix *matrix,
		   GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	GLdouble c = cos(angle);
	GLdouble s = sin(angle);
	struct wlsc_matrix rotate = {
		{ x * x * (1 - c) + c,     y * x * (1 - c) + z * s, x * z * (1 - c) - y * s, 0,
		  x * y * (1 - c) - z * s, y * y * (1 - c) + c,     y * z * (1 - c) + x * s, 0, 
		  x * z * (1 - c) + y * s, y * z * (1 - c) - x * s, z * z * (1 - c) + c,     0,
		  0, 0, 0, 1 }
	};

	wlsc_matrix_multiply(matrix, &rotate);
}

static void
wlsc_surface_init(struct wlsc_surface *surface,
		  struct wlsc_compositor *compositor, struct wl_visual *visual,
		  int32_t x, int32_t y, int32_t width, int32_t height)
{
	glGenTextures(1, &surface->texture);
	surface->compositor = compositor;
	surface->map.x = x;
	surface->map.y = y;
	surface->map.width = width;
	surface->map.height = height;
	surface->visual = visual;
	wlsc_matrix_init(&surface->matrix);
}

static struct wlsc_surface *
wlsc_surface_create_from_cairo_surface(struct wlsc_compositor *ec,
				      cairo_surface_t *surface,
				      int x, int y, int width, int height)
{
	struct wlsc_surface *es;
	int stride;
	void *data;

	stride = cairo_image_surface_get_stride(surface);
	data = cairo_image_surface_get_data(surface);

	es = malloc(sizeof *es);
	if (es == NULL)
		return NULL;

	wlsc_surface_init(es, ec, &ec->premultiplied_argb_visual,
			  x, y, width, height);
	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		     GL_BGRA, GL_UNSIGNED_BYTE, data);

	return es;
}

static void
wlsc_surface_destroy(struct wlsc_surface *surface,
		     struct wlsc_compositor *compositor)
{
	struct wlsc_listener *l;

	wl_list_remove(&surface->link);
	glDeleteTextures(1, &surface->texture);
	wl_client_remove_surface(surface->base.client, &surface->base);

	wl_list_for_each(l, &compositor->surface_destroy_listener_list, link)
		l->func(l, surface);

	free(surface);
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

static struct wlsc_surface *
pointer_create(struct wlsc_compositor *ec, int x, int y, int width, int height)
{
	struct wlsc_surface *es;
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

	es = wlsc_surface_create_from_cairo_surface(ec,
						   surface,
						   x - hotspot_x,
						   y - hotspot_y,
						   width, height);
	
	cairo_surface_destroy(surface);

	return es;
}

static struct wlsc_surface *
background_create(struct wlsc_output *output, const char *filename)
{
	struct wlsc_surface *background;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	void *data;
	GLenum format;

	background = malloc(sizeof *background);
	if (background == NULL)
		return NULL;
	
	g_type_init();

	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
						   output->width,
						   output->height,
						   FALSE, &error);
	if (error != NULL) {
		free(background);
		return NULL;
	}

	data = gdk_pixbuf_get_pixels(pixbuf);

	wlsc_surface_init(background, output->compositor,
			  &output->compositor->rgb_visual,
			  output->x, output->y, output->width, output->height);

	glBindTexture(GL_TEXTURE_2D, background->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (gdk_pixbuf_get_has_alpha(pixbuf))
		format = GL_RGBA;
	else
		format = GL_RGB;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
		     output->width, output->height, 0,
		     format, GL_UNSIGNED_BYTE, data);

	return background;
}

static void
wlsc_surface_draw(struct wlsc_surface *es)
{
	struct wlsc_compositor *ec = es->compositor;
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

	glPushMatrix();
	//glMultMatrixd(es->matrix.d);
	glBindTexture(GL_TEXTURE_2D, es->texture);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_INT, 0, vertices);
	glTexCoordPointer(2, GL_INT, 0, tex_coords);
	glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, indices);
	glPopMatrix();
}

static void
wlsc_surface_raise(struct wlsc_surface *surface)
{
	struct wlsc_compositor *compositor = surface->compositor;

	wl_list_remove(&surface->link);
	wl_list_insert(compositor->surface_list.prev, &surface->link);
}

static void
wlsc_surface_lower(struct wlsc_surface *surface)
{
	struct wlsc_compositor *compositor = surface->compositor;

	wl_list_remove(&surface->link);
	wl_list_insert(&compositor->surface_list, &surface->link);
}

static void
wlsc_vector_add(struct wlsc_vector *v1, struct wlsc_vector *v2)
{
	v1->x += v2->x;
	v1->y += v2->y;
	v1->z += v2->z;
}

static void
wlsc_vector_subtract(struct wlsc_vector *v1, struct wlsc_vector *v2)
{
	v1->x -= v2->x;
	v1->y -= v2->y;
	v1->z -= v2->z;
}

static void
wlsc_vector_scalar(struct wlsc_vector *v1, GLdouble s)
{
	v1->x *= s;
	v1->y *= s;
	v1->z *= s;
}

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct wlsc_output *output = data;
	struct wlsc_compositor *compositor = output->compositor;
	uint32_t msecs;

	msecs = sec * 1000 + usec / 1000;
	wl_display_post_frame(compositor->wl_display,
			      &compositor->base,
			      compositor->current_frame, msecs);

	wl_event_source_timer_update(compositor->timer_source, 5);
	compositor->repaint_on_timeout = 1;

	compositor->current_frame++;
}

static void
repaint_output(struct wlsc_output *output)
{
	struct wlsc_compositor *ec = output->compositor;
	struct wlsc_surface *es;
	struct wlsc_input_device *eid;
	double s = 3000;

	glViewport(0, 0, output->width, output->height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-output->width / s, output->width / s,
		  -output->height / s, output->height / s, 1, 2 * s);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClearColor(0, 0, 0, 1);

	glTranslatef(-output->width / 2, -output->height / 2, -s / 2);

	if (output->background)
		wlsc_surface_draw(output->background);
	else
		glClear(GL_COLOR_BUFFER_BIT);

	wl_list_for_each(es, &ec->surface_list, link)
		wlsc_surface_draw(es);

	wl_list_for_each(eid, &ec->input_device_list, link)
		wlsc_surface_draw(eid->sprite);

	output->current ^= 1;

	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER_EXT,
				  GL_COLOR_ATTACHMENT0_EXT,
				  GL_RENDERBUFFER_EXT,
				  output->rbo[output->current]);
	drmModePageFlip(ec->drm_fd, output->crtc_id,
			output->fb_id[output->current ^ 1],
			DRM_MODE_PAGE_FLIP_EVENT, output);
}

static void
repaint(void *data)
{
	struct wlsc_compositor *ec = data;
	struct wlsc_output *output;

	if (!ec->repaint_needed) {
		ec->repaint_on_timeout = 0;
		return;
	}

	wl_list_for_each(output, &ec->output_list, link)
		repaint_output(output);

	ec->repaint_needed = 0;
}

static void
wlsc_compositor_schedule_repaint(struct wlsc_compositor *compositor)
{
	struct wlsc_output *output;

	compositor->repaint_needed = 1;
	if (compositor->repaint_on_timeout)
		return;

	wl_list_for_each(output, &compositor->output_list, link)
		drmModePageFlip(compositor->drm_fd, output->crtc_id,
				output->fb_id[output->current ^ 1],
				DRM_MODE_PAGE_FLIP_EVENT, output);
}

static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;

	wlsc_surface_destroy(es, ec);

	wlsc_compositor_schedule_repaint(ec);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, uint32_t name, 
	       uint32_t width, uint32_t height, uint32_t stride,
	       struct wl_object *visual)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;
	EGLint attribs[] = {
		EGL_IMAGE_WIDTH_INTEL,	0,
		EGL_IMAGE_HEIGHT_INTEL,	0,
		EGL_IMAGE_NAME_INTEL,	0,
		EGL_IMAGE_STRIDE_INTEL,	0,
		EGL_IMAGE_FORMAT_INTEL,	EGL_FORMAT_RGBA_8888_KHR,
		EGL_NONE
	};

	es->width = width;
	es->height = height;

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

	if (es->image)
		eglDestroyImageKHR(ec->display, es->image);

	attribs[1] = width;
	attribs[3] = height;
	attribs[5] = name;
	attribs[7] = stride / 4;

	es->image = eglCreateImageKHR(ec->display, ec->context,
				       EGL_SYSTEM_IMAGE_INTEL,
				       NULL, attribs);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, es->image);
	
}

static void
surface_map(struct wl_client *client,
	    struct wl_surface *surface,
	    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

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
	struct wlsc_compositor *ec = (struct wlsc_compositor *) compositor;
	struct wlsc_surface *surface;

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		/* FIXME: Send OOM event. */
		return;

	wlsc_surface_init(surface, ec, NULL, 0, 0, 0, 0);

	wl_list_insert(ec->surface_list.prev, &surface->link);
	wl_client_add_surface(client, &surface->base,
			      &surface_interface, id);
}

static void
compositor_commit(struct wl_client *client,
		  struct wl_compositor *compositor, uint32_t key)
{
	struct wlsc_compositor *ec = (struct wlsc_compositor *) compositor;

	wlsc_compositor_schedule_repaint(ec);
	wl_client_send_acknowledge(client, compositor, key, ec->current_frame);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
	compositor_commit
};

static void
wlsc_surface_transform(struct wlsc_surface *surface,
		       int32_t x, int32_t y, int32_t *sx, int32_t *sy)
{
	/* Transform to surface coordinates. */
	*sx = (x - surface->map.x) * surface->width / surface->map.width;
	*sy = (y - surface->map.y) * surface->height / surface->map.height;
}

static void
wlsc_input_device_set_keyboard_focus(struct wlsc_input_device *device,
				     struct wlsc_surface *surface)
{
	if (device->keyboard_focus == surface)
		return;

	if (device->keyboard_focus &&
	    (!surface || device->keyboard_focus->base.client != surface->base.client))
		wl_surface_post_event(&device->keyboard_focus->base,
				      &device->base,
				      WL_INPUT_KEYBOARD_FOCUS, NULL, &device->keys);

	if (surface)
		wl_surface_post_event(&surface->base,
				      &device->base,
				      WL_INPUT_KEYBOARD_FOCUS,
				      &surface->base, &device->keys);

	device->keyboard_focus = surface;
}

static void
wlsc_input_device_set_pointer_focus(struct wlsc_input_device *device,
				    struct wlsc_surface *surface)
{
	if (device->pointer_focus == surface)
		return;

	if (device->pointer_focus &&
	    (!surface || device->pointer_focus->base.client != surface->base.client))
		wl_surface_post_event(&device->pointer_focus->base,
				      &device->base,
				      WL_INPUT_POINTER_FOCUS, NULL);
	if (surface)
		wl_surface_post_event(&surface->base,
				      &device->base,
				      WL_INPUT_POINTER_FOCUS, &surface->base);

	device->pointer_focus = surface;
}

static struct wlsc_surface *
pick_surface(struct wlsc_input_device *device, int32_t *sx, int32_t *sy)
{
	struct wlsc_compositor *ec = device->ec;
	struct wlsc_surface *es;

	if (device->grab > 0) {
		wlsc_surface_transform(device->grab_surface,
				       device->x, device->y, sx, sy);
		return device->grab_surface;
	}

	wl_list_for_each(es, &ec->surface_list, link)
		if (es->map.x <= device->x &&
		    device->x < es->map.x + es->map.width &&
		    es->map.y <= device->y &&
		    device->y < es->map.y + es->map.height) {
			wlsc_surface_transform(es, device->x, device->y, sx, sy);
			return es;
		}

	return NULL;
}

void
notify_motion(struct wlsc_input_device *device, int x, int y)
{
	struct wlsc_surface *es;
	struct wlsc_compositor *ec = device->ec;
	struct wlsc_output *output;
	const int hotspot_x = 16, hotspot_y = 16;
	int32_t sx, sy;

	if (!ec->vt_active)
		return;

	/* FIXME: We need some multi head love here. */
	output = container_of(ec->output_list.next, struct wlsc_output, link);
	if (x < output->x)
		x = 0;
	if (y < output->y)
		y = 0;
	if (x >= output->x + output->width)
		x = output->x + output->width - 1;
	if (y >= output->y + output->height)
		y = output->y + output->height - 1;

	device->x = x;
	device->y = y;
	es = pick_surface(device, &sx, &sy);

	wlsc_input_device_set_pointer_focus(device, es);
		
	if (es)
		wl_surface_post_event(&es->base, &device->base,
				      WL_INPUT_MOTION, x, y, sx, sy);

	device->sprite->map.x = x - hotspot_x;
	device->sprite->map.y = y - hotspot_y;

	wlsc_compositor_schedule_repaint(device->ec);
}

void
notify_button(struct wlsc_input_device *device,
	      int32_t button, int32_t state)
{
	struct wlsc_surface *surface;
	struct wlsc_compositor *compositor = device->ec;
	int32_t sx, sy;

	if (!compositor->vt_active)
		return;

	surface = pick_surface(device, &sx, &sy);
	if (surface) {
		if (state) {
			wlsc_surface_raise(surface);
			device->grab++;
			device->grab_surface = surface;
			wlsc_input_device_set_keyboard_focus(device,
							     surface);
		} else {
			device->grab--;
		}

		/* FIXME: Swallow click on raise? */
		wl_surface_post_event(&surface->base, &device->base,
				      WL_INPUT_BUTTON, button, state,
				      device->x, device->y, sx, sy);

		wlsc_compositor_schedule_repaint(compositor);
	}
}

static void on_term_signal(int signal_number, void *data);

void
notify_key(struct wlsc_input_device *device,
	   uint32_t key, uint32_t state)
{
	struct wlsc_compositor *compositor = device->ec;
	uint32_t *k, *end;
	uint32_t modifier;

	if (!compositor->vt_active)
		return;

	switch (key | compositor->modifier_state) {
	case KEY_BACKSPACE | MODIFIER_CTRL | MODIFIER_ALT:
		on_term_signal(SIGTERM, compositor);
		return;
	}

	switch (key) {
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		modifier = MODIFIER_CTRL;
		break;

	case KEY_LEFTALT:
	case KEY_RIGHTALT:
		modifier = MODIFIER_ALT;
		break;

	default:
		modifier = 0;
		break;
	}

	if (state)
		compositor->modifier_state |= modifier;
	else
		compositor->modifier_state &= ~modifier;

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

	if (device->keyboard_focus != NULL)
		wl_surface_post_event(&device->keyboard_focus->base,
				      &device->base, 
				      WL_INPUT_KEY, key, state);
}

struct evdev_input_device *
evdev_input_device_create(struct wlsc_input_device *device,
			  struct wl_display *display, const char *path);

static void
handle_surface_destroy(struct wlsc_listener *listener,
		       struct wlsc_surface *surface)
{
	struct wlsc_input_device *device =
		container_of(listener, struct wlsc_input_device, listener);
	struct wlsc_surface *focus;
	int32_t sx, sy;

	if (device->grab_surface == surface) {
		device->grab_surface = NULL;
		device->grab = 0;
	}
	if (device->keyboard_focus == surface)
		wlsc_input_device_set_keyboard_focus(device, NULL);
	if (device->pointer_focus == surface) {
		focus = pick_surface(device, &sx, &sy);
		wlsc_input_device_set_pointer_focus(device, focus);
		fprintf(stderr, "lost pointer focus surface, reverting to %p\n", focus);
	}
}

static struct wlsc_input_device *
create_input_device(struct wlsc_compositor *ec)
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
	device->ec = ec;

	device->listener.func = handle_surface_destroy;
	wl_list_insert(ec->surface_destroy_listener_list.prev,
		       &device->listener.link);
	wl_list_insert(ec->input_device_list.prev, &device->link);

	return device;
}

void
wlsc_device_get_position(struct wlsc_input_device *device, int32_t *x, int32_t *y)
{
	*x = device->x;
	*y = device->y;
}

static void
post_output_geometry(struct wl_client *client, struct wl_object *global)
{
	struct wlsc_output *output =
		container_of(global, struct wlsc_output, base);

	wl_client_post_event(client, global,
			     WL_OUTPUT_GEOMETRY,
			     output->width, output->height);
}

static void
on_drm_input(int fd, uint32_t mask, void *data)
{
	drmEventContext evctx;

	memset(&evctx, 0, sizeof evctx);
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.page_flip_handler = page_flip_handler;
	drmHandleEvent(fd, &evctx);
}

static int
init_egl(struct wlsc_compositor *ec, struct udev_device *device)
{
	struct wl_event_loop *loop;
	EGLDisplayTypeDRMMESA display;
	EGLint major, minor;

	display.type = EGL_DISPLAY_TYPE_DRM_MESA;
	display.device = udev_device_get_devnode(device);
	ec->display = eglGetDisplay((EGLNativeDisplayType) &display);
	if (ec->display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return -1;
	}

	if (!eglInitialize(ec->display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return -1;
	}

	ec->context = eglCreateContext(ec->display, NULL, NULL, NULL);
	if (ec->context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return -1;
	}

	if (!eglMakeCurrent(ec->display, EGL_NO_SURFACE, EGL_NO_SURFACE, ec->context)) {
		fprintf(stderr, "failed to make context current\n");
		return -1;
	}

	glGenFramebuffers(1, &ec->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, ec->fbo);

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->drm_fd = display.fd;
	ec->drm_source =
		wl_event_loop_add_fd(loop, ec->drm_fd,
				     WL_EVENT_READABLE, on_drm_input, ec);

	return 0;
}

static drmModeModeInfo builtin_1024x768 = {
	63500,			/* clock */
	1024, 1072, 1176, 1328, 0,
	768, 771, 775, 798, 0,
	59920,
	DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
	0,
	"1024x768"
};

static int
create_output_for_connector(struct wlsc_compositor *ec,
			    drmModeRes *resources,
			    drmModeConnector *connector)
{
	struct wlsc_output *output;
	drmModeEncoder *encoder;
	drmModeModeInfo *mode;
	int i, ret;
	EGLint name, handle, stride, attribs[] = {
		EGL_IMAGE_WIDTH_INTEL,	0,
		EGL_IMAGE_HEIGHT_INTEL,	0,
		EGL_IMAGE_FORMAT_INTEL,	EGL_FORMAT_RGBA_8888_KHR,
		EGL_IMAGE_USE_INTEL,	EGL_IMAGE_USE_SHARE_INTEL |
					EGL_IMAGE_USE_SCANOUT_INTEL,
		EGL_NONE
	};

	output = malloc(sizeof *output);
	if (output == NULL)
		return -1;

	if (connector->count_modes > 0) 
		mode = &connector->modes[0];
	else
		mode = &builtin_1024x768;

	encoder = drmModeGetEncoder(ec->drm_fd, connector->encoders[0]);
	if (encoder == NULL) {
		fprintf(stderr, "No encoder for connector.\n");
		return -1;
	}

	for (i = 0; i < resources->count_crtcs; i++) {
		if (encoder->possible_crtcs & (1 << i))
			break;
	}
	if (i == resources->count_crtcs) {
		fprintf(stderr, "No usable crtc for encoder.\n");
		return -1;
	}

	output->compositor = ec;
	output->crtc_id = resources->crtcs[i];
	output->connector_id = connector->connector_id;
	output->mode = *mode;
	output->x = 0;
	output->y = 0;
	output->width = mode->hdisplay;
	output->height = mode->vdisplay;

	printf("using crtc %d, connector %d and encoder %d, mode %s\n",
	       output->crtc_id,
	       output->connector_id,
	       encoder->encoder_id,
	       mode->name);

	drmModeFreeEncoder(encoder);

	glGenRenderbuffers(2, output->rbo);
	for (i = 0; i < 2; i++) {
		glBindRenderbuffer(GL_RENDERBUFFER_EXT, output->rbo[i]);

		attribs[1] = output->width;
		attribs[3] = output->height;
		output->image[i] = eglCreateImageKHR(ec->display, ec->context,
						     EGL_SYSTEM_IMAGE_INTEL,
						     NULL, attribs);
		glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, output->image[i]);
		eglShareImageINTEL(ec->display, ec->context,
				   output->image[i], 0, &name, &handle, &stride);

		ret = drmModeAddFB(ec->drm_fd, output->width, output->height,
				   32, 32, stride * 4, handle, &output->fb_id[i]);
		if (ret) {
			fprintf(stderr, "failed to add fb %d: %m\n", i);
			return -1;
		}
	}

	output->current = 0;
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER_EXT,
				  GL_COLOR_ATTACHMENT0_EXT,
				  GL_RENDERBUFFER_EXT,
				  output->rbo[output->current]);
	ret = drmModeSetCrtc(ec->drm_fd, output->crtc_id,
			     output->fb_id[output->current ^ 1], 0, 0,
			     &output->connector_id, 1, &output->mode);
	if (ret) {
		fprintf(stderr, "failed to set mode: %m\n");
		return -1;
	}

	output->base.interface = &wl_output_interface;
	wl_display_add_object(ec->wl_display, &output->base);
	wl_display_add_global(ec->wl_display, &output->base, post_output_geometry);
	wl_list_insert(ec->output_list.prev, &output->link);

	output->background = background_create(output, option_background);

	return 0;
}

static int
create_outputs(struct wlsc_compositor *ec)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	int i;

	resources = drmModeGetResources(ec->drm_fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return -1;
	}

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(ec->drm_fd, resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
		    (option_connector == 0 ||
		     connector->connector_id == option_connector))
			if (create_output_for_connector(ec, resources, connector) < 0)
				return -1;

		drmModeFreeConnector(connector);
	}

	if (wl_list_empty(&ec->output_list)) {
		fprintf(stderr, "No currently active connector found.\n");
		return -1;
	}

	drmModeFreeResources(resources);

	return 0;
}

static const struct wl_interface visual_interface = {
	"visual", 1,
};

static void
add_visuals(struct wlsc_compositor *ec)
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

static void on_enter_vt(int signal_number, void *data)
{
	struct wlsc_compositor *ec = data;
	struct wlsc_output *output;
	int ret;

	ret = drmSetMaster(ec->drm_fd);
	if (ret) {
		fprintf(stderr, "failed to set drm master\n");
		return;
	}

	ioctl(ec->tty_fd, VT_RELDISP, VT_ACKACQ);
	ec->vt_active = TRUE;

	wl_list_for_each(output, &ec->output_list, link) {
		ret = drmModeSetCrtc(ec->drm_fd, output->crtc_id,
				     output->fb_id[output->current ^ 1], 0, 0,
				     &output->connector_id, 1, &output->mode);
		if (ret)
			fprintf(stderr, "failed to set mode for connector %d: %m\n",
				output->connector_id);
	}
}

static void on_leave_vt(int signal_number, void *data)
{
	struct wlsc_compositor *ec = data;
	int ret;

	ret = drmDropMaster(ec->drm_fd);
	if (ret) {
		fprintf(stderr, "failed to drop drm master\n");
		return;
	}

	ioctl (ec->tty_fd, VT_RELDISP, 1);
	ec->vt_active = FALSE;
}

static void
on_tty_input(int fd, uint32_t mask, void *data)
{
	struct wlsc_compositor *ec = data;

	/* Ignore input to tty.  We get keyboard events from evdev
	 */
	tcflush(ec->tty_fd, TCIFLUSH);
}

static void on_term_signal(int signal_number, void *data)
{
	struct wlsc_compositor *ec = data;

	if (tcsetattr(ec->tty_fd, TCSANOW, &ec->terminal_attributes) < 0)
		fprintf(stderr, "could not restore terminal to canonical mode\n");

	exit(0);
}

static int setup_tty(struct wlsc_compositor *ec, struct wl_event_loop *loop)
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

static int
init_libudev(struct wlsc_compositor *ec)
{
	struct udev_enumerate *e;
        struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path;
	struct wlsc_input_device *input_device;

	ec->udev = udev_new();
	if (ec->udev == NULL) {
		fprintf(stderr, "failed to initialize udev context\n");
		return -1;
	}

	input_device = create_input_device(ec);

	e = udev_enumerate_new(ec->udev);
	udev_enumerate_add_match_subsystem(e, "input");
	udev_enumerate_add_match_property(e, "WAYLAND_SEAT", "1");
        udev_enumerate_scan_devices(e);
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(ec->udev, path);
		evdev_input_device_create(input_device, ec->wl_display,
					  udev_device_get_devnode(device));
	}
        udev_enumerate_unref(e);

	e = udev_enumerate_new(ec->udev);
	udev_enumerate_add_match_subsystem(e, "drm");
	udev_enumerate_add_match_property(e, "WAYLAND_SEAT", "1");
        udev_enumerate_scan_devices(e);
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(ec->udev, path);
		if (init_egl(ec, device) < 0) {
			fprintf(stderr, "failed to initialize egl\n");
			return -1;
		}
		if (create_outputs(ec) < 0) {
			fprintf(stderr, "failed to create output for %s\n", path);
			return -1;
		}
	}
        udev_enumerate_unref(e);

	/* Create the pointer surface now that we have a current EGL context. */
	input_device->sprite =
		pointer_create(ec, input_device->x, input_device->y, 64, 64);

	return 0;
}

static struct wlsc_compositor *
wlsc_compositor_create(struct wl_display *display)
{
	struct wlsc_compositor *ec;
	struct screenshooter *shooter;
	struct wl_event_loop *loop;

	ec = malloc(sizeof *ec);
	if (ec == NULL)
		return NULL;

	memset(ec, 0, sizeof *ec);
	ec->wl_display = display;

	wl_display_set_compositor(display, &ec->base, &compositor_interface); 

	add_visuals(ec);

	wl_list_init(&ec->surface_list);
	wl_list_init(&ec->input_device_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->surface_destroy_listener_list);
	if (init_libudev(ec) < 0) {
		fprintf(stderr, "failed to initialize devices\n");
		return NULL;
	}

	shooter = screenshooter_create(ec);
	wl_display_add_object(display, &shooter->base);
	wl_display_add_global(display, &shooter->base, NULL);

	loop = wl_display_get_event_loop(ec->wl_display);

	setup_tty(ec, loop);

	ec->timer_source = wl_event_loop_add_timer(loop, repaint, ec);
	wlsc_compositor_schedule_repaint(ec);

	return ec;
}

/* The plan here is to generate a random anonymous socket name and
 * advertise that through a service on the session dbus.
 */
static const char socket_name[] = "\0wayland";

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wlsc_compositor *ec;
	GError *error = NULL;
	GOptionContext *context;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, option_entries, "Wayland");
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		exit(EXIT_FAILURE);
	}

	display = wl_display_create();

	ec = wlsc_compositor_create(display);
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
