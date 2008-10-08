#ifndef _WAYLAND_CLIENT_H
#define _WAYLAND_CLIENT_H

#include "connection.h"

struct wl_display;
struct wl_surface;

struct wl_display *wl_display_create(const char *address,
				     wl_connection_update_func_t update, void *data);
void wl_display_destroy(struct wl_display *display);
int wl_display_get_fd(struct wl_display *display);
void wl_display_iterate(struct wl_display *display, uint32_t mask);

struct wl_surface *
wl_display_create_surface(struct wl_display *display);

void wl_surface_destroy(struct wl_surface *surface);
void wl_surface_attach(struct wl_surface *surface,
		       uint32_t name, int width, int height, int stride);
void wl_surface_map(struct wl_surface *surface,
		    int32_t x, int32_t y, int32_t width, int32_t height);

#endif
