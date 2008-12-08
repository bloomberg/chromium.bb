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

#include "cairo-util.h"
#include "window.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

struct terminal {
	struct window *window;
	struct wl_display *display;
	int resize_scheduled;
};

static gboolean
resize_window(void *data)
{
	struct terminal *terminal = data;

	window_draw(terminal->window);
	wl_display_commit(terminal->display, 0);

	return FALSE;
}

static void
resize_handler(struct window *window, int32_t width, int32_t height, void *data)
{
	struct terminal *terminal = data;

	if (!terminal->resize_scheduled) {
		g_idle_add(resize_window, terminal);
		terminal->resize_scheduled = 1;
	}
}

static void
acknowledge_handler(struct window *window, uint32_t key, void *data)
{
	struct terminal *terminal = data;

	terminal->resize_scheduled = 0;
}

static struct terminal *
terminal_create(struct wl_display *display, int fd)
{
	struct terminal *terminal;

	terminal = malloc(sizeof *terminal);
	if (terminal == NULL)
		return terminal;

	terminal->window = window_create(display, fd, "Wayland Terminal",
					 500, 100, 300, 200);
	terminal->display = display;
	terminal->resize_scheduled = 1;

	window_set_resize_handler(terminal->window, resize_handler, terminal);
	window_set_acknowledge_handler(terminal->window, acknowledge_handler, terminal);

	return terminal;
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	int fd;
	GMainLoop *loop;
	GSource *source;
	struct terminal *terminal;

	fd = open(gem_device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "drm open failed: %m\n");
		return -1;
	}

	display = wl_display_create(socket_name, sizeof socket_name);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	loop = g_main_loop_new(NULL, FALSE);
	source = wl_glib_source_new(display);
	g_source_attach(source, NULL);

	terminal = terminal_create(display, fd);
	window_draw(terminal->window);
	wl_display_commit(display, 0);

	g_main_loop_run(loop);

	return 0;
}
