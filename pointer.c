#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <cairo.h>
#include <glib.h>

#include "wayland-client.h"
#include "wayland-glib.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

static uint32_t name_cairo_surface(int fd, cairo_surface_t *surface)
{
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	struct drm_i915_gem_pwrite pwrite;
	int32_t width, height, stride;
	void *data;

	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);
	stride = cairo_image_surface_get_stride(surface);
	data = cairo_image_surface_get_data(surface);

	memset(&create, 0, sizeof(create));
	create.size = height * stride;

	if (ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
		fprintf(stderr, "gem create failed: %m\n");
		return 0;
	}

	pwrite.handle = create.handle;
	pwrite.offset = 0;
	pwrite.size = height * stride;
	pwrite.data_ptr = (uint64_t) (uintptr_t) data;
	if (ioctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite) < 0) {
		fprintf(stderr, "gem pwrite failed: %m\n");
		return 0;
	}

	flink.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
		fprintf(stderr, "gem flink failed: %m\n");
		return 0;
	}

#if 0
	/* We need to hold on to the handle until the server has received
	 * the attach request... we probably need a confirmation event.
	 * I guess the breadcrumb idea will suffice. */
	struct drm_gem_close close;
	close.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close) < 0) {
		fprintf(stderr, "gem close failed: %m\n");
		return 0;
	}
#endif

	return flink.name;
}

static void *
draw_pointer(int width, int height)
{
	const int d = 2, end = 6, tx = 2, ty = 7, dx = 3, dy = 5;
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     width, height);

	cr = cairo_create(surface);
	cairo_set_line_width (cr, d);
	cairo_move_to(cr, d, d);
	cairo_line_to(cr, d + tx, d + ty);
	cairo_line_to(cr, d + dx, d + dy);
	cairo_line_to(cr, width - end, height - d);
	cairo_line_to(cr, width - d, height - end);
	cairo_line_to(cr, d + dy, d + dx);
	cairo_line_to(cr, d + ty, d + tx);
	cairo_close_path(cr);
	cairo_set_source_rgb(cr, 0, 0, 0);
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
	uint32_t name;
	cairo_surface_t *s;
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

	pointer.width = 16;
	pointer.height = 16;
	pointer.surface = wl_display_create_surface(display);

	s = draw_pointer(pointer.width, pointer.height);
	name = name_cairo_surface(fd, s);

	wl_surface_attach(pointer.surface, name,
			  pointer.width, pointer.height,
			  cairo_image_surface_get_stride(s));
	wl_surface_map(pointer.surface, 512, 384, pointer.width, pointer.height);

	wl_display_set_event_handler(display, event_handler, &pointer);

	g_main_loop_run(loop);

	return 0;
}
