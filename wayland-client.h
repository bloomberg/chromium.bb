#ifndef _WAYLAND_CLIENT_H
#define _WAYLAND_CLIENT_H

struct wl_connection;
struct wl_display;
struct wl_surface;

struct wl_connection *
wl_connection_create(const char *address);
void
wl_connection_destroy(struct wl_connection *connection);
int
wl_connection_get_fd(struct wl_connection *connection);
void
wl_connection_iterate(struct wl_connection *connection);
int
wl_connection_flush(struct wl_connection *connection);

struct wl_display *
wl_connection_get_display(struct wl_connection *connection);
struct wl_surface *
wl_display_create_surface(struct wl_display *display);

void wl_surface_destroy(struct wl_surface *surface);
void wl_surface_attach(struct wl_surface *surface,
		       uint32_t name, int width, int height, int stride);
void wl_surface_map(struct wl_surface *surface,
		    int32_t x, int32_t y, int32_t width, int32_t height);

#endif
