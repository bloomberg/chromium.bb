/*
 * Copyright © 2013 Marek Chalupa
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

#include "wayland-server.h"
#include "test-runner.h"

static void
signal_notify(struct wl_listener *listener, void *data)
{
	/* only increase counter*/
	++(*((int *) data));
}

TEST(signal_init)
{
	struct wl_signal signal;

	wl_signal_init(&signal);

	/* Test if listeners' list is initialized */
	assert(&signal.listener_list == signal.listener_list.next
		&& "Maybe wl_signal implementation changed?");
	assert(signal.listener_list.next == signal.listener_list.prev
		&& "Maybe wl_signal implementation changed?");
}

TEST(signal_add_get)
{
	struct wl_signal signal;

	/* we just need different values of notify */
	struct wl_listener l1 = {.notify = (wl_notify_func_t) 0x1};
	struct wl_listener l2 = {.notify = (wl_notify_func_t) 0x2};
	struct wl_listener l3 = {.notify = (wl_notify_func_t) 0x3};
	/* one real, why not */
	struct wl_listener l4 = {.notify = signal_notify};

	wl_signal_init(&signal);

	wl_signal_add(&signal, &l1);
	wl_signal_add(&signal, &l2);
	wl_signal_add(&signal, &l3);
	wl_signal_add(&signal, &l4);

	assert(wl_signal_get(&signal, signal_notify) == &l4);
	assert(wl_signal_get(&signal, (wl_notify_func_t) 0x3) == &l3);
	assert(wl_signal_get(&signal, (wl_notify_func_t) 0x2) == &l2);
	assert(wl_signal_get(&signal, (wl_notify_func_t) 0x1) == &l1);

	/* get should not be destructive */
	assert(wl_signal_get(&signal, signal_notify) == &l4);
	assert(wl_signal_get(&signal, (wl_notify_func_t) 0x3) == &l3);
	assert(wl_signal_get(&signal, (wl_notify_func_t) 0x2) == &l2);
	assert(wl_signal_get(&signal, (wl_notify_func_t) 0x1) == &l1);
}

TEST(signal_emit_to_one_listener)
{
	int count = 0;
	int counter;

	struct wl_signal signal;
	struct wl_listener l1 = {.notify = signal_notify};

	wl_signal_init(&signal);
	wl_signal_add(&signal, &l1);

	for (counter = 0; counter < 100; counter++)
		wl_signal_emit(&signal, &count);

	assert(counter == count);
}

TEST(signal_emit_to_more_listeners)
{
	int count = 0;
	int counter;

	struct wl_signal signal;
	struct wl_listener l1 = {.notify = signal_notify};
	struct wl_listener l2 = {.notify = signal_notify};
	struct wl_listener l3 = {.notify = signal_notify};

	wl_signal_init(&signal);
	wl_signal_add(&signal, &l1);
	wl_signal_add(&signal, &l2);
	wl_signal_add(&signal, &l3);

	for (counter = 0; counter < 100; counter++)
		wl_signal_emit(&signal, &count);

	assert(3 * counter == count);
}
