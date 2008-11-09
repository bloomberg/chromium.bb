#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>
#include <glib.h>

#include <GL/gl.h>
#include <eagle.h>

#include "wayland-client.h"
#include "wayland-glib.h"

#include "gears.h"
#include "cairo-util.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

static void die(const char *msg)
{
	fprintf(stderr, "%s", msg);
	exit(EXIT_FAILURE);
}

struct window {
	struct wl_surface *surface;
	int x, y, width, height;
	int drag_x, drag_y, last_x, last_y;
	int state;
	uint32_t name;
	int fd;
	int redraw_scheduled;
	cairo_pattern_t *background;

	struct buffer *buffer;
	struct buffer *egl_buffer;

	GLfloat gears_angle;
	struct gears *gears;
	EGLDisplay display;
	EGLContext context;
	EGLConfig config;
	EGLSurface egl_surface;
};

static void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius)
{
	cairo_move_to(cr, x0, y0 + radius);
	cairo_arc(cr, x0 + radius, y0 + radius, radius, M_PI, 3 * M_PI / 2);
	cairo_line_to(cr, x1 - radius, y0);
	cairo_arc(cr, x1 - radius, y0 + radius, radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to(cr, x1, y1 - radius);
	cairo_arc(cr, x1 - radius, y1 - radius, radius, 0, M_PI / 2);
	cairo_line_to(cr, x0 + radius, y1);
	cairo_arc(cr, x0 + radius, y1 - radius, radius, M_PI / 2, M_PI);
	cairo_close_path(cr);
}

static gboolean
draw_window(void *data)
{
	struct window *window = data;
	cairo_surface_t *surface;
	cairo_t *cr;
	int border = 2, radius = 5;
	cairo_text_extents_t extents;
	cairo_pattern_t *gradient, *outline, *bright, *dim;
	struct buffer *buffer;
	const static char title[] = "Wayland First Post";

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     window->width + 32, window->height + 32);

	outline = cairo_pattern_create_rgb(0.1, 0.1, 0.1);
	bright = cairo_pattern_create_rgb(0.6, 0.6, 0.6);
	dim = cairo_pattern_create_rgb(0.4, 0.4, 0.4);

	cr = cairo_create(surface);

	cairo_translate(cr, 16 + 5, 16 + 3);
	cairo_set_line_width (cr, border);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
	rounded_rect(cr, 1, 1, window->width - 1, window->height - 1, radius);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);
	blur_surface(surface);

	cairo_translate(cr, -5, -3);
	cairo_set_line_width (cr, border);
	rounded_rect(cr, 1, 1, window->width - 1, window->height - 1, radius);
	cairo_set_source(cr, outline);
	cairo_stroke(cr);
	rounded_rect(cr, 2, 2, window->width - 2, window->height - 2, radius);
	cairo_set_source(cr, bright);
	cairo_stroke(cr);
	rounded_rect(cr, 3, 3, window->width - 2, window->height - 2, radius);
	cairo_set_source(cr, dim);
	cairo_stroke(cr);

	rounded_rect(cr, 2, 2, window->width - 2, window->height - 2, radius);
	gradient = cairo_pattern_create_linear (0, 0, 0, window->height);
	cairo_pattern_add_color_stop_rgb(gradient, 0, 0.4, 0.4, 0.4);
	cairo_pattern_add_color_stop_rgb(gradient, 0.2, 0.7, 0.7, 0.7);
	cairo_set_source(cr, gradient);
	cairo_fill(cr);
	cairo_pattern_destroy(gradient);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_move_to(cr, 10, 50);
	cairo_line_to(cr, window->width - 10, 50);
	cairo_line_to(cr, window->width - 10, window->height - 10);
	cairo_line_to(cr, 10, window->height - 10);
	cairo_close_path(cr);
	cairo_set_source(cr, dim);
	cairo_stroke(cr);

	cairo_move_to(cr, 11, 51);
	cairo_line_to(cr, window->width - 10, 51);
	cairo_line_to(cr, window->width - 10, window->height - 10);
	cairo_line_to(cr, 11, window->height - 10);
	cairo_close_path(cr);
	cairo_set_source(cr, bright);
	cairo_stroke(cr);

	cairo_move_to(cr, 10, 50);
	cairo_line_to(cr, window->width - 10, 50);
	cairo_line_to(cr, window->width - 10, window->height - 10);
	cairo_line_to(cr, 10, window->height - 10);
	cairo_close_path(cr);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.9);
	cairo_fill(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_font_size(cr, 14);
	cairo_text_extents(cr, title, &extents);
	cairo_move_to(cr, (window->width - extents.width) / 2, 10 - extents.y_bearing);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 4);
	cairo_text_path(cr, title);
	cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);

	cairo_destroy(cr);

	if (window->buffer != NULL)
		buffer_destroy(window->buffer, window->fd);
	buffer = buffer_create_from_cairo_surface(window->fd, surface);
	window->buffer = buffer;

	cairo_surface_destroy(surface);

	wl_surface_attach(window->surface, buffer->name,
			  buffer->width, buffer->height, buffer->stride);
			  
	wl_surface_map(window->surface, 
		       window->x, window->y,
		       buffer->width, buffer->height);

	/* FIXME: Free window->buffer when we receive the ack event. */

	buffer = window->egl_buffer;
	gears_draw(window->gears, window->gears_angle);
	wl_surface_copy(window->surface,
			(window->width - 300) / 2,
			50 + (window->height - 50 - 300) / 2,
			buffer->name, buffer->stride,
			0, 0, buffer->width, buffer->height);

	window->redraw_scheduled = 0;

	return FALSE;
}

enum window_state {
	WINDOW_STABLE,
	WINDOW_MOVING,
	WINDOW_RESIZING_UPPER_LEFT,
	WINDOW_RESIZING_UPPER_RIGHT,
	WINDOW_RESIZING_LOWER_LEFT,
	WINDOW_RESIZING_LOWER_RIGHT
};

enum location {
	LOCATION_INTERIOR,
	LOCATION_UPPER_LEFT,
	LOCATION_UPPER_RIGHT,
	LOCATION_LOWER_LEFT,
	LOCATION_LOWER_RIGHT,
	LOCATION_OUTSIDE
};

static void
event_handler(struct wl_display *display,
	      uint32_t opcode, uint32_t arg1, uint32_t arg2, void *data)
{
	struct window *window = data;
	int location, border = 4;
	int grip_size = 16;

	if (opcode == 0) {
		window->last_x = arg1;
		window->last_y = arg2;
		switch (window->state) {
		case WINDOW_MOVING:
			window->x = window->drag_x + arg1;
			window->y = window->drag_y + arg2;
			wl_surface_map(window->surface, window->x, window->y,
				       window->buffer->width, window->buffer->height);
			break;
		case WINDOW_RESIZING_LOWER_RIGHT:
			window->width = window->drag_x + arg1;
			window->height = window->drag_y + arg2;
			if (window->width < 400)
				window->width = 400;
			if (window->height < 400)
				window->height = 400;
			if (!window->redraw_scheduled) {
				window->redraw_scheduled = 1;
				g_idle_add(draw_window, window);
			}
			break;
		}
	}

	if (window->x + window->width - grip_size <= window->last_x &&
		   window->last_x < window->x + window->width &&
		   window->y + window->height - grip_size <= window->last_y &&
		   window->last_y < window->y + window->height) {
		location = LOCATION_LOWER_RIGHT;
	} else if (window->x + border <= window->last_x &&
		   window->last_x < window->x + window->width - border &&
		   window->y + border <= window->last_y &&
		   window->last_y < window->y + window->height - border) {
		location = LOCATION_INTERIOR;
	} else {
		location = LOCATION_OUTSIDE;
	}

	if (opcode == 1 && arg1 == 0 && arg2 == 1) {
		switch (location) {
		case LOCATION_INTERIOR:
			window->drag_x = window->x - window->last_x;
			window->drag_y = window->y - window->last_y;
			window->state = WINDOW_MOVING;
			break;
		case LOCATION_LOWER_RIGHT:
			window->drag_x = window->width - window->last_x;
			window->drag_y = window->height - window->last_y;
			window->state = WINDOW_RESIZING_LOWER_RIGHT;
			break;
		default:
			window->state = WINDOW_STABLE;
			break;
		}
	} else if (opcode == 1 && arg1 == 0 && arg2 == 0) {
		window->state = WINDOW_STABLE;
	}
}

static struct window *
window_create(struct wl_display *display, int fd)
{
	EGLint major, minor, count;
	EGLConfig configs[64];
	struct window *window;
	struct buffer *buffer;
	const GLfloat red = 0, green = 0, blue = 0, alpha = 0.9;

	window = malloc(sizeof *window);
	if (window == NULL)
		return NULL;

	memset(window, 0, sizeof *window);
	window->surface = wl_display_create_surface(display);
	window->x = 200;
	window->y = 200;
	window->width = 450;
	window->height = 500;
	window->state = WINDOW_STABLE;
	window->fd = fd;
	window->background = cairo_pattern_create_rgba (red, green, blue, alpha);

	window->display = eglCreateDisplayNative("/dev/dri/card0", "i965");
	if (window->display == NULL)
		die("failed to create display\n");

	if (!eglInitialize(window->display, &major, &minor))
		die("failed to initialize display\n");

	if (!eglGetConfigs(window->display, configs, 64, &count))
		die("failed to get configs\n");

	window->config = configs[24];
	window->context = eglCreateContext(window->display, window->config, NULL, NULL);
	if (window->context == NULL)
		die("failed to create context\n");

	/* FIXME: We need to get the stride right here in a chipset
	 * independent way.  Maybe do it in name_cairo_surface(). */
	buffer = buffer_create(window->fd, 300, 300, (300 * 4 + 15) & ~15);
	window->egl_buffer = buffer;
	window->egl_surface = eglCreateSurfaceForName(window->display,
						      window->config, buffer->name,
						      buffer->width, buffer->height,
						      buffer->stride, NULL);

	if (window->egl_surface == NULL)
		die("failed to create egl surface\n");

	if (!eglMakeCurrent(window->display,
			    window->egl_surface, window->egl_surface, window->context))
		die("failed to make context current\n");

	glViewport(0, 0, 300, 300);

	window->gears = gears_create(red, green, blue, alpha);
	window->gears_angle = 0.0;

	draw_window(window);

	return window;
}

static gboolean
draw(gpointer data)
{
	struct window *window = data;
	struct buffer *buffer;

	
	if (!window->redraw_scheduled) {
		gears_draw(window->gears, window->gears_angle);

		buffer = window->egl_buffer;
		wl_surface_copy(window->surface,
				(window->width - 300) / 2,
				50 + (window->height - 50 - 300) / 2,
				buffer->name, buffer->stride,
				0, 0, buffer->width, buffer->height);
	}

	window->gears_angle += 1;

	return TRUE;
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	int fd;
	struct window *window;
	GMainLoop *loop;
	GSource *source;

	fd = open(gem_device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "drm open failed: %m\n");
		return -1;
	}

	display = wl_display_create(socket_name);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	loop = g_main_loop_new(NULL, FALSE);
	source = wayland_source_new(display);
	g_source_attach(source, NULL);

	window = window_create(display, fd);

	draw_window(window);

	wl_display_set_event_handler(display, event_handler, window);

	g_timeout_add(20, draw, window);

	g_main_loop_run(loop);

	return 0;
}
