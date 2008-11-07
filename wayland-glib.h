#ifndef _WAYLAND_GLIB_H_
#define _WAYLAND_GLIB_H_

#include <glib/gmain.h>

typedef struct _WaylandSource WaylandSource;

GSource *wayland_source_new(struct wl_display *display);

#endif
