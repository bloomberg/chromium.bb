/*
 * Copyright © 2014 Jonas Ådahl
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

#include <assert.h>

#include "wayland-client.h"
#include "wayland-private.h"
#include "wayland-server.h"
#include "test-runner.h"

TEST(message_version)
{
	unsigned int i;
	const struct {
		const struct wl_message *message;
		int expected_version;
	} messages[] = {
		{ &wl_pointer_interface.events[WL_POINTER_ENTER], 1 },
		{ &wl_surface_interface.events[WL_SURFACE_ENTER], 1 },
		{ &wl_pointer_interface.methods[WL_POINTER_SET_CURSOR], 1 },
		{ &wl_pointer_interface.methods[WL_POINTER_RELEASE], 3 },
		{ &wl_surface_interface.methods[WL_SURFACE_DESTROY], 1 },
		{ &wl_surface_interface.methods[WL_SURFACE_SET_BUFFER_TRANSFORM], 2 },
		{ &wl_surface_interface.methods[WL_SURFACE_SET_BUFFER_SCALE], 3 },
	};

	for (i = 0; i < ARRAY_LENGTH(messages); ++i) {
		assert(wl_message_get_since(messages[i].message) ==
		       messages[i].expected_version);
	}
}
