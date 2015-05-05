/*
 * Copyright © 2015 Samsung Electronics Co., Ltd
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
 * xwayland-test: Confirm that we can map a window and we're running
 *		  under Xwayland, not just X.
 *
 *		  This is done in steps:
 *		  1) Confirm that the WL_SURFACE_ID atom exists
 *		  2) Confirm that the window manager's name is "Weston WM"
 *		  3) Make sure we can map a window
 */

#include "config.h"

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

#include "weston-test-runner.h"

TEST(xwayland_client_test)
{
	Display *display;
	Window window, root, *support;
	XEvent event;
	int screen, status, actual_format;
	unsigned long nitems, bytes;
	Atom atom, type_atom, actual_type;
	char *wm_name;

	display = XOpenDisplay(NULL);
	if (!display)
		exit(EXIT_FAILURE);

	atom = XInternAtom(display, "WL_SURFACE_ID", True);
	assert(atom != None);

	atom = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", True);
	assert(atom != None);

	screen = DefaultScreen(display);
	root = RootWindow(display, screen);

	status = XGetWindowProperty(display, root, atom, 0L, ~0L,
				    False, XA_WINDOW, &actual_type,
				    &actual_format, &nitems, &bytes,
				    (void *)&support);
	assert(status == Success);

	atom = XInternAtom(display, "_NET_WM_NAME", True);
	assert(atom != None);
	type_atom = XInternAtom(display, "UTF8_STRING", True);
	assert(atom != None);
	status = XGetWindowProperty(display, *support, atom, 0L, BUFSIZ,
				    False, type_atom, &actual_type,
				    &actual_format, &nitems, &bytes,
				    (void *)&wm_name);
	assert(status == Success);
	assert(nitems);
	assert(strcmp("Weston WM", wm_name) == 0);
	free(support);
	free(wm_name);

	window = XCreateSimpleWindow(display, root, 100, 100, 300, 300, 1,
				     BlackPixel(display, screen),
				     WhitePixel(display, screen));
	XSelectInput(display, window, ExposureMask);
	XMapWindow(display, window);

	alarm(4);
	while (1) {
		XNextEvent(display, &event);
		if (event.type == Expose)
			break;
	}

	XCloseDisplay(display);
	exit(EXIT_SUCCESS);
}
