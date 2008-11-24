#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>

#include "wayland-client.h"
#include "wayland-glib.h"

/* The screenshooter is a good example of a custom object exposed by
 * the compositor and serves as a test bed for implementing client
 * side marshalling outside libwayland.so */

static const char socket_name[] = "\0wayland";

struct screenshooter {
	uint32_t id;
	struct wl_display *display;
};

static struct screenshooter *
screenshooter_create(struct wl_display *display)
{
	struct screenshooter *screenshooter;
	uint32_t id;

	id = wl_display_get_object_id(display, "screenshooter");
	if (id == 0) {
		fprintf(stderr, "server doesn't support screenshooter interface\n");
		return NULL;
	}

	screenshooter = malloc(sizeof screenshooter);
	if (screenshooter == NULL)
		return NULL;

	screenshooter->id = id;
	screenshooter->display = display;

	return screenshooter;
}

#define SCREENSHOOTER_SHOOT 0

static void
screenshooter_shoot(struct screenshooter *screenshooter)
{
	uint32_t request[2];

	request[0] = screenshooter->id;
	request[1] = SCREENSHOOTER_SHOOT | ((sizeof request) << 16);

	wl_display_write(screenshooter->display,
			 request, sizeof request);
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	GMainLoop *loop;
	GSource *source;
	struct screenshooter *s;

	display = wl_display_create(socket_name);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	loop = g_main_loop_new(NULL, FALSE);
	source = wayland_source_new(display);
	g_source_attach(source, NULL);

	s = screenshooter_create(display);
	if (s == NULL)
		exit(-1);

	screenshooter_shoot(s);

	g_main_loop_run(loop);

	return 0;
}
