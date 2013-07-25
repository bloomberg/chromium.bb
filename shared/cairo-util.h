/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef _CAIRO_UTIL_H
#define _CAIRO_UTIL_H

#include <cairo.h>

void
surface_flush_device(cairo_surface_t *surface);

void
tile_mask(cairo_t *cr, cairo_surface_t *surface,
	  int x, int y, int width, int height, int margin, int top_margin);

void
tile_source(cairo_t *cr, cairo_surface_t *surface,
	    int x, int y, int width, int height, int margin, int top_margin);

void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius);

cairo_surface_t *
load_cairo_surface(const char *filename);

struct theme {
	cairo_surface_t *active_frame;
	cairo_surface_t *inactive_frame;
	cairo_surface_t *shadow;
	int frame_radius;
	int margin;
	int width;
	int titlebar_height;
};

struct theme *
theme_create(void);
void
theme_destroy(struct theme *t);

enum {
	THEME_FRAME_ACTIVE = 1,
	THEME_FRAME_MAXIMIZED,
};

void
theme_render_frame(struct theme *t, 
		   cairo_t *cr, int width, int height,
		   const char *title, uint32_t flags);

enum theme_location {
	THEME_LOCATION_INTERIOR = 0,
	THEME_LOCATION_RESIZING_TOP = 1,
	THEME_LOCATION_RESIZING_BOTTOM = 2,
	THEME_LOCATION_RESIZING_LEFT = 4,
	THEME_LOCATION_RESIZING_TOP_LEFT = 5,
	THEME_LOCATION_RESIZING_BOTTOM_LEFT = 6,
	THEME_LOCATION_RESIZING_RIGHT = 8,
	THEME_LOCATION_RESIZING_TOP_RIGHT = 9,
	THEME_LOCATION_RESIZING_BOTTOM_RIGHT = 10,
	THEME_LOCATION_RESIZING_MASK = 15,
	THEME_LOCATION_EXTERIOR = 16,
	THEME_LOCATION_TITLEBAR = 17,
	THEME_LOCATION_CLIENT_AREA = 18,
};

enum theme_location
theme_get_location(struct theme *t, int x, int y, int width, int height, int flags);

#endif
