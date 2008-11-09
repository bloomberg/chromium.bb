#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <cairo.h>
#include <glib.h>

#include "wayland-client.h"
#include "wayland-glib.h"

#include "cairo-util.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

static void
pointer_path(cairo_t *cr, int x, int y)
{
	const int end = 4, tx = 2, ty = 7, dx = 3, dy = 5;
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

static void *
draw_pointer(int width, int height)
{
	const int hotspot_x = 16, hotspot_y = 16;
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     width, height);

	cr = cairo_create(surface);
	pointer_path(cr, hotspot_x + 3, hotspot_y + 2);
	cairo_set_line_width (cr, 2);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);
	blur_surface(surface);

	pointer_path(cr, hotspot_x, hotspot_y);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
	cairo_destroy(cr);

	return surface;
}

struct pointer {
	int width, height;
	struct wl_surface *surface;
};

static void
event_handler(struct wl_display *display,
	      uint32_t opcode,
	      uint32_t arg1, uint32_t arg2, void *data)
{
	struct pointer *pointer = data;

	if (opcode == 0)
		wl_surface_map(pointer->surface, arg1, arg2, pointer->width, pointer->height);
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct pointer pointer;
	int fd;
	cairo_surface_t *s;
	GMainLoop *loop;
	GSource *source;
	struct buffer *buffer;

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

	pointer.width = 48;
	pointer.height = 48;
	pointer.surface = wl_display_create_surface(display);

	s = draw_pointer(pointer.width, pointer.height);
	buffer = buffer_create_from_cairo_surface(fd, s);

	wl_surface_attach(pointer.surface, buffer->name,
			  buffer->width, buffer->height, buffer->stride);
	wl_surface_map(pointer.surface, 512, 384, pointer.width, pointer.height);

	wl_display_set_event_handler(display, event_handler, &pointer);

	g_main_loop_run(loop);

	return 0;
}
