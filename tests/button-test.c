/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <linux/input.h>

#include "weston-test-client-helper.h"

TEST(simple_button_test)
{
	struct client *client;
	struct pointer *pointer;

	client = create_client_and_test_surface(100, 100, 100, 100);
	assert(client);

	pointer = client->input->pointer;

	assert(pointer->button == 0);
	assert(pointer->state == 0);

	weston_test_move_pointer(client->test->weston_test, 150, 150);
	client_roundtrip(client);
	assert(pointer->x == 50);
	assert(pointer->y == 50);

	weston_test_send_button(client->test->weston_test, BTN_LEFT,
			    WL_POINTER_BUTTON_STATE_PRESSED);
	client_roundtrip(client);
	assert(pointer->button == BTN_LEFT);
	assert(pointer->state == WL_POINTER_BUTTON_STATE_PRESSED);

	weston_test_send_button(client->test->weston_test, BTN_LEFT,
			    WL_POINTER_BUTTON_STATE_RELEASED);
	client_roundtrip(client);
	assert(pointer->button == BTN_LEFT);
	assert(pointer->state == WL_POINTER_BUTTON_STATE_RELEASED);
}
