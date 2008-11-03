#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>

#include "wayland-client.h"

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

struct window {
	struct wl_surface *surface;
	int x, y, width, height, stride;
	int drag_x, drag_y, last_x, last_y;
	int state;
	uint32_t name;
	int fd;
};

static void *
draw_window(struct window *window)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	int border = 4;
	int half = (border + 1) / 2;

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     window->width,
					     window->height);

	cr = cairo_create(surface);
	cairo_set_line_width (cr, border);
	cairo_move_to(cr, half, half);
	cairo_line_to(cr, window->width - half, half);
	cairo_line_to(cr, window->width - half,
		      window->height - half);
	cairo_line_to(cr, half, window->height - half);
	cairo_close_path(cr);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.85);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_stroke(cr);
	cairo_destroy(cr);

	window->stride = cairo_image_surface_get_stride(surface);

	window->name = name_cairo_surface(window->fd, surface);
	cairo_surface_destroy(surface);

	wl_surface_attach(window->surface, window->name,
			  window->width, window->height, window->stride);
			  
	wl_surface_map(window->surface, 
		       window->x, window->y,
		       window->width, window->height);

	return surface;
}

static int
connection_update(struct wl_connection *connection,
		  uint32_t mask, void *data)
{
	struct pollfd *p = data;

	p->events = 0;
	if (mask & WL_CONNECTION_READABLE)
		p->events |= POLLIN;
	if (mask & WL_CONNECTION_WRITABLE)
		p->events |= POLLOUT;

	return 0;
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

void event_handler(struct wl_display *display,
		   uint32_t opcode,
		   uint32_t arg1, uint32_t arg2, void *data)
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
				       window->width, window->height);
			break;
		case WINDOW_RESIZING_LOWER_RIGHT:
			window->width = window->drag_x + arg1;
			window->height = window->drag_y + arg2;
			draw_window(window);
			break;
		}
	}

	if (window->x + border <= window->last_x &&
	    window->last_x < window->x + window->width - border &&
	    window->y + border <= window->last_y &&
	    window->last_y < window->y + window->height - border) {
		location = LOCATION_INTERIOR;
	} else if (window->x + window->width - grip_size <= window->last_x &&
		   window->last_x < window->x + window->width &&
		   window->y + window->height - grip_size <= window->last_y &&
		   window->last_y < window->y + window->height) {
		location = LOCATION_LOWER_RIGHT;
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

int main(int argc, char *argv[])
{
	struct wl_display *display;
	int fd, ret;
	uint32_t mask;
	cairo_surface_t *s;
	struct pollfd p[1];
	struct window window;

	fd = open(gem_device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "drm open failed: %m\n");
		return -1;
	}

	display = wl_display_create(socket_name,
				    connection_update, &p[0]);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}
	p[0].fd = wl_display_get_fd(display);

	window.surface = wl_display_create_surface(display);
	window.x = 200;
	window.y = 200;
	window.width = 350;
	window.height = 200;
	window.state = WINDOW_STABLE;
	window.fd = fd;

	s = draw_window(&window);

	wl_display_set_event_handler(display, event_handler, &window);

	while (ret = poll(p, 1, -1), ret >= 0) {
		mask = 0;
		if (p[0].revents & POLLIN)
			mask |= WL_CONNECTION_READABLE;
		if (p[0].revents & POLLOUT)
			mask |= WL_CONNECTION_WRITABLE;
		if (mask)
			wl_display_iterate(display, mask);
	}

	return 0;
}
