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
	struct wl_display *display;
	struct wl_surface *surface;
	int x, y, width, height;
	int margin;
	int drag_x, drag_y, last_x, last_y;
	int state;
	uint32_t name;
	int fd;
	int resized;
	cairo_pattern_t *background;

	struct buffer *buffer;
	struct buffer *egl_buffer;

	GLfloat gears_angle;
	struct gears *gears;
	EGLDisplay egl_display;
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
	const static char title[] = "Wayland First Post";
	struct buffer *buffer;
	int width, height;

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     window->width + window->margin * 2,
					     window->height + window->margin * 2);

	outline = cairo_pattern_create_rgb(0.1, 0.1, 0.1);
	bright = cairo_pattern_create_rgb(0.8, 0.8, 0.8);
	dim = cairo_pattern_create_rgb(0.4, 0.4, 0.4);

	cr = cairo_create(surface);

	cairo_translate(cr, window->margin + 7, window->margin + 5);
	cairo_set_line_width (cr, border);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
	rounded_rect(cr, 0, 0, window->width, window->height, radius);
	cairo_fill(cr);
	blur_surface(surface, 24 + radius);

	cairo_translate(cr, -7, -5);
	cairo_set_line_width (cr, border);
	rounded_rect(cr, 1, 1, window->width - 1, window->height - 1, radius);
	cairo_set_source(cr, outline);
	cairo_stroke(cr);
	rounded_rect(cr, 2, 2, window->width - 2, window->height - 2, radius - 1);
	cairo_set_source(cr, bright);
	cairo_stroke(cr);
	rounded_rect(cr, 3, 3, window->width - 2, window->height - 2, radius - 1);
	cairo_set_source(cr, dim);
	cairo_stroke(cr);

	rounded_rect(cr, 2, 2, window->width - 2, window->height - 2, radius - 1);
	gradient = cairo_pattern_create_linear (0, 0, 0, 100);
	cairo_pattern_add_color_stop_rgb(gradient, 0, 0.6, 0.6, 0.4);
	cairo_pattern_add_color_stop_rgb(gradient, 1, 0.8, 0.8, 0.7);
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

	window->buffer = buffer_create_from_cairo_surface(window->fd, surface);
	cairo_surface_destroy(surface);

	wl_surface_attach(window->surface,
			  window->buffer->name,
			  window->buffer->width,
			  window->buffer->height,
			  window->buffer->stride);

	wl_surface_map(window->surface,
		       window->x - window->margin,
		       window->y - window->margin,
		       window->width + 2 * window->margin,
		       window->height + 2 * window->margin);

	width = window->width - 20;		
	height = window->height - 60;
	buffer = buffer_create(window->fd, width, height, (width * 4 + 15) & ~15);
	window->egl_buffer = buffer;
	window->egl_surface = eglCreateSurfaceForName(window->egl_display,
						      window->config, buffer->name,
						      buffer->width, buffer->height,
						      buffer->stride, NULL);
	if (!eglMakeCurrent(window->egl_display,
			    window->egl_surface, window->egl_surface, window->context))
		die("failed to make context current\n");

	glViewport(0, 0, width, height);
	window->resized = 0;

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

static int
update_gears(void *data)
{
	struct window *window = data;

	if (window->resized)
		draw_window(window);
	gears_draw(window->gears, window->gears_angle);

	return FALSE;
}

static void
event_handler(struct wl_display *display,
	      uint32_t object, uint32_t opcode,
	      uint32_t size, uint32_t *p, void *data)
{
	struct window *window = data;
	struct buffer *buffer;
	int location;
	int grip_size = 16;

	/* FIXME: Object ID 1 is the display, for anything else we
	 * assume it's an input device. */
	if (object == 1 && opcode == 3) {
		uint32_t key = p[0];

		/* Ignore acknowledge events for window move requests. */
		if (key != 0)
			return;

		/* The acknowledge event means that the server
		 * processed our last commit request and we can now
		 * safely free the old window buffer if we resized and
		 * render the next frame into our back buffer.. */

		if (window->buffer != NULL) {
			buffer_destroy(window->buffer, window->fd);
			window->buffer = NULL;
		}
		g_idle_add(update_gears, window);
	} else if (object == 1 && opcode == 4) {
		/* The frame event means that the previous frame was
		 * composited, and we can now send the request to copy
		 * the frame we've rendered in the mean time into the
		 * servers surface buffer. */
		buffer = window->egl_buffer;
		wl_surface_copy(window->surface,
				10 + window->margin, 50 + window->margin,
				buffer->name, buffer->stride,
				0, 0, buffer->width, buffer->height);
		wl_display_commit(window->display, 0);
	window->gears_angle += 1;

	} else if (object == 1) {
		fprintf(stderr, "unexpected event from display: %d\n",
			opcode);
		exit(-1);
	} else if (opcode == 0) {
		int x = p[0], y = p[1];

		window->last_x = x;
		window->last_y = y;
		switch (window->state) {
		case WINDOW_MOVING:
			window->x = window->drag_x + x;
			window->y = window->drag_y + y;
			wl_surface_map(window->surface,
				       window->x - window->margin,
				       window->y - window->margin,
				       window->width + 2 * window->margin,
				       window->height + 2 * window->margin);
			wl_display_commit(window->display, 1);
			break;
		case WINDOW_RESIZING_LOWER_RIGHT:
			window->width = window->drag_x + x;
			window->height = window->drag_y + y;
			if (window->width < 400)
				window->width = 400;
			if (window->height < 400)
				window->height = 400;

			/* Right now, resizing the window from the
			 * per-frame callback is fine, since the
			 * window drawing code is so slow that we
			 * can't draw more than one window per frame
			 * anyway.  However, once we implement faster
			 * resizing, this will show lag between
			 * pointer motion and window size even if
			 * resizing is fast.  We need to keep
			 * processing motion events and posting new
			 * frames as fast as possible so when the
			 * server composites the next frame it will
			 * have the most recent size possible, like
			 * what we do for window moves. */

			window->resized = 1;
			break;
		}
	} else if (opcode == 1) {
		int button = p[0], state = p[1];

		if (window->x + window->width - grip_size <= window->last_x &&
		    window->last_x < window->x + window->width &&
		    window->y + window->height - grip_size <= window->last_y &&
		    window->last_y < window->y + window->height) {
			location = LOCATION_LOWER_RIGHT;
		} else if (window->x <= window->last_x &&
			   window->last_x < window->x + window->width &&
			   window->y <= window->last_y &&
			   window->last_y < window->y + window->height) {
			location = LOCATION_INTERIOR;
		} else {
			location = LOCATION_OUTSIDE;
		}

		if (button == 0 && state == 1) {
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
		} else if (button == 0 && state == 0) {
			window->state = WINDOW_STABLE;
		}
	}
}

static struct window *
window_create(struct wl_display *display, int fd)
{
	EGLint major, minor, count;
	EGLConfig configs[64];
	struct window *window;
	const GLfloat red = 0, green = 0, blue = 0, alpha = 0.92;

	window = malloc(sizeof *window);
	if (window == NULL)
		return NULL;

	memset(window, 0, sizeof *window);
	window->display = display;
	window->surface = wl_display_create_surface(display);
	window->x = 200;
	window->y = 200;
	window->width = 450;
	window->height = 500;
	window->margin = 16;
	window->state = WINDOW_STABLE;
	window->fd = fd;
	window->background = cairo_pattern_create_rgba (red, green, blue, alpha);

	window->egl_display = eglCreateDisplayNative("/dev/dri/card0", "i965");
	if (window->egl_display == NULL)
		die("failed to create egl display\n");

	if (!eglInitialize(window->egl_display, &major, &minor))
		die("failed to initialize display\n");

	if (!eglGetConfigs(window->egl_display, configs, 64, &count))
		die("failed to get configs\n");

	window->config = configs[24];
	window->context = eglCreateContext(window->egl_display, window->config, NULL, NULL);
	if (window->context == NULL)
		die("failed to create context\n");

	draw_window(window);
	window->gears = gears_create(0, 0, 0, 0.92);
	gears_draw(window->gears, window->gears_angle);
	window->gears_angle += 1;
	wl_display_commit(window->display, 0);

	return window;
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

	wl_display_set_event_handler(display, event_handler, window);

	g_main_loop_run(loop);

	return 0;
}
