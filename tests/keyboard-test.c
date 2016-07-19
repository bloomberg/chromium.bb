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

#include <stdint.h>

#include "weston-test-client-helper.h"

TEST(simple_keyboard_test)
{
	struct client *client;
	struct surface *expect_focus = NULL;
	struct keyboard *keyboard;
	uint32_t expect_key = 0;
	uint32_t expect_state = 0;

	client = create_client_and_test_surface(10, 10, 1, 1);
	assert(client);

	keyboard = client->input->keyboard;

	while (1) {
		assert(keyboard->key == expect_key);
		assert(keyboard->state == expect_state);
		assert(keyboard->focus == expect_focus);

		if (keyboard->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			expect_state = WL_KEYBOARD_KEY_STATE_RELEASED;
			weston_test_send_key(client->test->weston_test,
					     expect_key, expect_state);
		} else if (keyboard->focus) {
			expect_focus = NULL;
			weston_test_activate_surface(
				client->test->weston_test, NULL);
		} else if (expect_key < 10) {
			expect_key++;
			expect_focus = client->surface;
			expect_state = WL_KEYBOARD_KEY_STATE_PRESSED;
			weston_test_activate_surface(
				client->test->weston_test,
				expect_focus->wl_surface);
			weston_test_send_key(client->test->weston_test,
					     expect_key, expect_state);
		} else {
			break;
		}

		client_roundtrip(client);
	}
}
