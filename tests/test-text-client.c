/*
 * Copyright © 2012 Intel Corporation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include <wayland-client.h>
#include "../clients/text-client-protocol.h"

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;

	struct wl_surface *surface;
	struct wl_seat *seat;

	struct text_model_factory *factory;
	struct text_model *text_model;

	unsigned int activated;
	unsigned int deactivated;
};

static void 
text_model_commit_string(void *data,
			 struct text_model *text_model,
			 const char *text,
			 uint32_t index)
{
}

static void
text_model_preedit_string(void *data,
			  struct text_model *text_model,
			  const char *text,
			  uint32_t index)
{
}

static void
text_model_delete_surrounding_text(void *data,
				   struct text_model *text_model,
				   int32_t index,
				   uint32_t length)
{
}

static void
text_model_preedit_styling(void *data,
			   struct text_model *text_model)
{
}

static void 
text_model_key(void *data,
	       struct text_model *text_model,
	       uint32_t key,
	       uint32_t state)
{
}

static void
text_model_selection_replacement(void *data,
				 struct text_model *text_model)
{
}

static void
text_model_direction(void *data,
		     struct text_model *text_model)
{
}

static void
text_model_locale(void *data,
		  struct text_model *text_model)
{
}

static void
text_model_activated(void *data,
		     struct text_model *text_model)
{
	struct display *display = data;

	fprintf(stderr, "%s\n", __FUNCTION__);

	display->activated += 1;
}

static void
text_model_deactivated(void *data,
		       struct text_model *text_model)
{
	struct display *display = data;

	display->deactivated += 1;
}

static const struct text_model_listener text_model_listener = {
	text_model_commit_string,
	text_model_preedit_string,
	text_model_delete_surrounding_text,
	text_model_preedit_styling,
	text_model_key,
	text_model_selection_replacement,
	text_model_direction,
	text_model_locale,
	text_model_activated,
	text_model_deactivated
};

static void
handle_global(struct wl_display *_display, uint32_t id,
	      const char *interface, uint32_t version, void *data)
{
	struct display *display = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		display->compositor =
			wl_display_bind(display->display,
					id, &wl_compositor_interface);
	} else if (strcmp(interface, "wl_seat") == 0) {
		display->seat = wl_display_bind(display->display, id,
						&wl_seat_interface);
	} else if (strcmp(interface, "text_model_factory") == 0) {
		display->factory = wl_display_bind(display->display, id,
						   &text_model_factory_interface);
	}
}

static void
create_surface(int fd, struct display *display)
{
	char buf[64];
	int len;

	display->surface = wl_compositor_create_surface(display->compositor);
	wl_display_flush(display->display);

	len = snprintf(buf, sizeof buf, "surface %d\n",
		       wl_proxy_get_id((struct wl_proxy *) display->surface));
	assert(write(fd, buf, len) == len);
}

static void
create_text_model(int fd, struct display *display)
{
	char buf[64];
	int len;

	display->text_model = text_model_factory_create_text_model(display->factory);
	text_model_add_listener(display->text_model, &text_model_listener, display);
	wl_display_flush(display->display);

	len = snprintf(buf, sizeof buf, "text_model %d\n",
		       wl_proxy_get_id((struct wl_proxy *) display->text_model));
	assert(write(fd, buf, len) == len);
}

static void
write_state(int fd, struct display *display)
{
	char buf[64];
	int len;

	wl_display_flush(display->display);
	len = snprintf(buf, sizeof buf, "activated %u deactivated %u\n",
		       display->activated, display->deactivated);
	assert(write(fd, buf, len) == len);
	wl_display_roundtrip(display->display);
}

static void
activate_text_model(int fd, struct display *display)
{
	write_state(fd, display);

	text_model_activate(display->text_model, display->seat, display->surface);

	wl_display_flush(display->display);
	wl_display_roundtrip(display->display);
}

static void
deactivate_text_model(int fd, struct display *display)
{
	write_state(fd, display);

	text_model_deactivate(display->text_model, display->seat);

	wl_display_flush(display->display);
	wl_display_roundtrip(display->display);
}

int main(int argc, char *argv[])
{
	struct display *display;
	char buf[256], *p;
	int ret, fd;

	display = malloc(sizeof *display);
	assert(display);

	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->activated = 0;
	display->deactivated = 0;

	wl_display_add_global_listener(display->display,
				       handle_global, display);
	wl_display_iterate(display->display, WL_DISPLAY_READABLE);
	wl_display_roundtrip(display->display);

	fd = 0;
	p = getenv("TEST_SOCKET");
	if (p)
		fd = strtol(p, NULL, 0);

	while (1) {
		ret = read(fd, buf, sizeof buf);
		if (ret == -1) {
			fprintf(stderr, "read error: fd %d, %m\n", fd);
			return -1;
		}

		fprintf(stderr, "test-client: got %.*s\n", ret - 1, buf);

		if (strncmp(buf, "bye\n", ret) == 0) {
			return 0;
		} else if (strncmp(buf, "create-surface\n", ret) == 0) {
			create_surface(fd, display);
		} else if (strncmp(buf, "create-text-model\n", ret) == 0) {
			create_text_model(fd, display);
		} else if (strncmp(buf, "activate-text-model\n", ret) == 0) {
			activate_text_model(fd, display);
		} else if (strncmp(buf, "deactivate-text-model\n", ret) == 0) {
			deactivate_text_model(fd, display);
		} else if (strncmp(buf, "assert-state\n", ret) == 0) {
			write_state(fd, display);
		} else {
			fprintf(stderr, "unknown command %.*s\n", ret, buf);
			return -1;
		}
	}

	assert(0);
}
