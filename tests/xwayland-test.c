/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Tiago Vignatti
 *
 * xwayland-test: the idea is to guarantee that XWayland infrastructure in
 * general works with Weston.
 */

#include "weston-test-runner.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/dri2.h>
#include <xf86drm.h>


static int
dri2_open(xcb_connection_t *c, xcb_screen_t *screen)
{
	xcb_dri2_connect_cookie_t cookie;
	xcb_dri2_connect_reply_t *reply;
	xcb_dri2_authenticate_cookie_t cookie_auth;
	xcb_dri2_authenticate_reply_t *reply_auth;
	char *driver, *device;
	int fd;
	drm_magic_t magic;

	cookie = xcb_dri2_connect(c, screen->root, XCB_DRI2_DRIVER_TYPE_DRI);
	reply = xcb_dri2_connect_reply(c, cookie, 0);
	assert(reply);

	driver = strndup(xcb_dri2_connect_driver_name (reply),
			 xcb_dri2_connect_driver_name_length (reply));
	device = strndup(xcb_dri2_connect_device_name (reply),
			 xcb_dri2_connect_device_name_length (reply));

	fd = open(device, O_RDWR);
	printf ("Trying connect to %s driver on %s\n", driver, device);
	free(driver);
	free(device);

	if (fd < 0)
		return -1;

	drmGetMagic(fd, &magic);

	cookie_auth = xcb_dri2_authenticate(c, screen->root, magic);
	reply_auth = xcb_dri2_authenticate_reply(c, cookie_auth, 0);
	assert(reply_auth);

	return fd;
}

static int
create_window(void)
{
	xcb_connection_t *c;
	xcb_screen_t *screen;
	xcb_window_t win;
	int fd;

	c = xcb_connect (NULL, NULL);
	if (c == NULL) {
		printf("failed to get X11 connection\n");
		return -1;
	}

	screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

	win = xcb_generate_id(c);
	xcb_create_window(c, XCB_COPY_FROM_PARENT, win, screen->root,
			0, 0, 150, 150, 1, XCB_WINDOW_CLASS_INPUT_OUTPUT,
			screen->root_visual, 0, NULL);

	xcb_change_property (c, XCB_PROP_MODE_REPLACE, win,
			XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
			5, "title");
	xcb_map_window(c, win);
	xcb_flush(c);

	fd = dri2_open(c, screen);
	if (fd < 0)
		return -1;

	xcb_destroy_window(c, win);
	xcb_disconnect(c);
	return 0;
}

/*
 * Ideally, the X Window Manager (XWM) and Weston Wayland compositor shouldn't
 * be in the same process because they are using two different protocol
 * streams in which one does not interface with the other. Probably the
 * biggest problem with such architecture are the potentials dead locks that
 * it may occur. So hypothetically, an X client might issue an X11 blocking
 * request via X (DRI2Authenticate) which in turn sends a Wayland blocking
 * request for Weston process it. X is blocked. At the same time, XWM might be
 * trying to process an XChangeProperty, so it requests a blocking X11 call to
 * the X server (xcb_get_property_reply -> xcb_wait_for_reply) which therefore
 * will blocks there. It's a deadlock situation and this test is trying to
 * catch that.
 */
static void
check_dri2_authenticate(void)
{
	int i, num_tests;

	/* TODO: explain why num_tests times */
	num_tests = 10;
	for (i = 0; i < num_tests; i++)
		assert(create_window() == 0);
}

TEST(xwayland_client_test)
{
	check_dri2_authenticate();
	exit(EXIT_SUCCESS);
}
