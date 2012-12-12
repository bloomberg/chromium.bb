/*
 * Copyright © 2012 Intel Corporation
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
 */

#include "weston-test-client-helper.h"

TEST(simple_keyboard_test)
{
	struct client *client;
	struct surface *expect_focus = NULL;
	struct keyboard *keyboard;
	uint32_t expect_key = 0;
	uint32_t expect_state = 0;

	client = client_create(10, 10, 1, 1);
	assert(client);

	keyboard = client->input->keyboard;

	while(1) {
		assert(keyboard->key == expect_key);
		assert(keyboard->state == expect_state);
		assert(keyboard->focus == expect_focus);

		if (keyboard->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			expect_state = WL_KEYBOARD_KEY_STATE_RELEASED;
			wl_test_send_key(client->test->wl_test, expect_key,
				expect_state);
		} else if (keyboard->focus) {
			expect_focus = NULL;
			wl_test_activate_surface(client->test->wl_test,
						 NULL);
		} else if (expect_key < 10) {
			expect_key++;
			expect_focus = client->surface;
			expect_state = WL_KEYBOARD_KEY_STATE_PRESSED;
			wl_test_activate_surface(client->test->wl_test,
						 expect_focus->wl_surface);
			wl_test_send_key(client->test->wl_test, expect_key,
					 expect_state);
		} else {
			break;
		}

		client_roundtrip(client);
	}
}
