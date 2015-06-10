/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Jason Ekstrand
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include "wayland-private.h"
#include "wayland-server.h"
#include "wayland-client.h"
#include "test-runner.h"
#include "test-compositor.h"

struct display_destroy_listener {
	struct wl_listener listener;
	int done;
};

static void
display_destroy_notify(struct wl_listener *l, void *data)
{
	struct display_destroy_listener *listener;

	listener = container_of(l, struct display_destroy_listener, listener);
	listener->done = 1;
}

TEST(display_destroy_listener)
{
	struct wl_display *display;
	struct display_destroy_listener a, b;

	display = wl_display_create();
	assert(display);

	a.listener.notify = &display_destroy_notify;
	a.done = 0;
	wl_display_add_destroy_listener(display, &a.listener);

	assert(wl_display_get_destroy_listener(display, display_destroy_notify) ==
	       &a.listener);

	b.listener.notify = display_destroy_notify;
	b.done = 0;
	wl_display_add_destroy_listener(display, &b.listener);

	wl_list_remove(&a.listener.link);

	wl_display_destroy(display);

	assert(!a.done);
	assert(b.done);
}

/* Fake 'client' which does not use wl_display_connect, and thus leaves the
 * file descriptor passed through WAYLAND_SOCKET intact. This should not
 * trigger an assertion in the leak check. */
static void
empty_client(void)
{
	return;
}

TEST(tc_leaks_tests)
{
	struct display *d = display_create();
	client_create(d, empty_client);
	display_run(d);
	display_destroy(d);
}

static void
registry_handle_globals(void *data, struct wl_registry *registry,
			uint32_t id, const char *intf, uint32_t ver)
{
	struct wl_seat **seat = data;

	if (strcmp(intf, "wl_seat") == 0) {
		*seat = wl_registry_bind(registry, id,
					 &wl_seat_interface, ver);
		assert(*seat);
	}
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_globals,
	NULL
};

static struct wl_seat *
client_get_seat(struct client *c)
{
	struct wl_seat *seat;
	struct wl_registry *reg = wl_display_get_registry(c->wl_display);
	assert(reg);

	wl_registry_add_listener(reg, &registry_listener, &seat);
	wl_display_roundtrip(c->wl_display);
	assert(seat);

	wl_registry_destroy(reg);

	return seat;
}

static void
check_for_error(struct client *c, struct wl_proxy *proxy)
{
	uint32_t ec, id;
	int err;
	const struct wl_interface *intf;

	/* client should be disconnected */
	assert(wl_display_roundtrip(c->wl_display) == -1);

	err = wl_display_get_error(c->wl_display);
	assert(err == EPROTO);

	ec = wl_display_get_protocol_error(c->wl_display, &intf, &id);
	assert(ec == 23);
	assert(intf == &wl_seat_interface);
	assert(id == wl_proxy_get_id(proxy));
}

static void
bind_seat(struct wl_client *client, void *data,
	  uint32_t vers, uint32_t id)
{
	struct display *d = data;
	struct client_info *ci;
	struct wl_resource *res;

	/* find the right client_info struct and save the
	 * resource as its data, so that we can use it later */
	wl_list_for_each(ci, &d->clients, link) {
		if (ci->wl_client == client)
			break;
	}

	res = wl_resource_create(client, &wl_seat_interface, vers, id);
	assert(res);

	ci->data = res;
}

static void
client_disconnect_nocheck(struct client *c)
{
	wl_proxy_destroy((struct wl_proxy *) c->tc);
	wl_display_disconnect(c->wl_display);
	free(c);
}

static void
post_error_main(void)
{
	struct client *c = client_connect();
	struct wl_seat *seat = client_get_seat(c);

	/* stop display so that it can post the error.
	 * The function should return -1, because of the posted error */
	assert(stop_display(c, 1) == -1);

	/* display should have posted error, check it! */
	check_for_error(c, (struct wl_proxy *) seat);

	/* don't call client_disconnect(c), because then the test would be
	 * aborted due to checks for error in this function */
	wl_proxy_destroy((struct wl_proxy *) seat);
	client_disconnect_nocheck(c);
}

TEST(post_error_to_one_client)
{
	struct display *d = display_create();
	struct client_info *cl;

	wl_global_create(d->wl_display, &wl_seat_interface,
			 1, d, bind_seat);

	cl = client_create(d, post_error_main);
	display_run(d);

	/* the display was stopped by client, so it can
	 * proceed in the code and post an error */
	assert(cl->data);
	wl_resource_post_error((struct wl_resource *) cl->data,
			       23, "Dummy error");

	/* this one should be ignored */
	wl_resource_post_error((struct wl_resource *) cl->data,
			       21, "Dummy error (ignore)");

	display_resume(d);
	display_destroy(d);
}

static void
post_error_main2(void)
{
	struct client *c = client_connect();
	struct wl_seat *seat = client_get_seat(c);

	/* the error should not be posted for this client */
	assert(stop_display(c, 2) >= 0);

	wl_proxy_destroy((struct wl_proxy *) seat);
	client_disconnect(c);
}

static void
post_error_main3(void)
{
	struct client *c = client_connect();
	struct wl_seat *seat = client_get_seat(c);

	assert(stop_display(c, 2) == -1);
	check_for_error(c, (struct wl_proxy *) seat);

	/* don't call client_disconnect(c), because then the test would be
	 * aborted due to checks for error in this function */
	wl_proxy_destroy((struct wl_proxy *) seat);
	client_disconnect_nocheck(c);
}

/* all the testcases could be in one TEST, but splitting it
 * apart is better for debugging when the test fails */
TEST(post_error_to_one_from_two_clients)
{
	struct display *d = display_create();
	struct client_info *cl;

	wl_global_create(d->wl_display, &wl_seat_interface,
			 1, d, bind_seat);

	client_create(d, post_error_main2);
	cl = client_create(d, post_error_main3);
	display_run(d);

	/* post error only to the second client */
	assert(cl->data);
	wl_resource_post_error((struct wl_resource *) cl->data,
			       23, "Dummy error");
	wl_resource_post_error((struct wl_resource *) cl->data,
			       21, "Dummy error (ignore)");

	display_resume(d);
	display_destroy(d);
}

/* all the testcases could be in one TEST, but splitting it
 * apart is better for debugging when the test fails */
TEST(post_error_to_two_clients)
{
	struct display *d = display_create();
	struct client_info *cl, *cl2;

	wl_global_create(d->wl_display, &wl_seat_interface,
			 1, d, bind_seat);

	cl = client_create(d, post_error_main3);
	cl2 = client_create(d, post_error_main3);

	display_run(d);

	/* Try to send the error to both clients */
	assert(cl->data && cl2->data);
	wl_resource_post_error((struct wl_resource *) cl->data,
			       23, "Dummy error");
	wl_resource_post_error((struct wl_resource *) cl->data,
			       21, "Dummy error (ignore)");

	wl_resource_post_error((struct wl_resource *) cl2->data,
			       23, "Dummy error");
	wl_resource_post_error((struct wl_resource *) cl2->data,
			       21, "Dummy error (ignore)");

	display_resume(d);
	display_destroy(d);
}

static void
post_nomem_main(void)
{
	struct client *c = client_connect();
	struct wl_seat *seat = client_get_seat(c);

	assert(stop_display(c, 1) == -1);
	assert(wl_display_get_error(c->wl_display) == ENOMEM);

	wl_proxy_destroy((struct wl_proxy *) seat);
	client_disconnect_nocheck(c);
}

TEST(post_nomem_tst)
{
	struct display *d = display_create();
	struct client_info *cl;

	wl_global_create(d->wl_display, &wl_seat_interface,
			 1, d, bind_seat);

	cl = client_create(d, post_nomem_main);
	display_run(d);

	assert(cl->data);
	wl_resource_post_no_memory((struct wl_resource *) cl->data);
	display_resume(d);

	/* first client terminated. Run it again,
	 * but post no memory to client */
	cl = client_create(d, post_nomem_main);
	display_run(d);

	assert(cl->data);
	wl_client_post_no_memory(cl->wl_client);
	display_resume(d);

	display_destroy(d);
}

static void
register_reading(struct wl_display *display)
{
	while(wl_display_prepare_read(display) != 0 && errno == EAGAIN)
		assert(wl_display_dispatch_pending(display) >= 0);
	assert(wl_display_flush(display) >= 0);
}

/* create thread that will call prepare+read so that
 * it will block */
static pthread_t
create_thread(struct client *c, void *(*func)(void*))
{
	pthread_t thread;

	c->display_stopped = 0;
	/* func must set display->stopped to 1 before sleeping */
	assert(pthread_create(&thread, NULL, func, c) == 0);

	/* make sure the thread is sleeping. It's a little bit racy
	 * (setting display_stopped to 1 and calling wl_display_read_events)
	 * so call usleep once again after the loop ends - it should
	 * be sufficient... */
	while (c->display_stopped == 0)
		test_usleep(500);
	test_usleep(10000);

	return thread;
}

static void *
thread_read_error(void *data)
{
	struct client *c = data;

	register_reading(c->wl_display);

	/*
	 * Calling the read right now will block this thread
	 * until the other thread will read the data.
	 * However, after invoking an error, this
	 * thread should be woken up or it will block indefinitely.
	 */
	c->display_stopped = 1;
	assert(wl_display_read_events(c->wl_display) == -1);

	assert(wl_display_dispatch_pending(c->wl_display) == -1);
	assert(wl_display_get_error(c->wl_display));

	pthread_exit(NULL);
}

/* test posting an error in multi-threaded environment. */
static void
threading_post_err(void)
{
	DISABLE_LEAK_CHECKS;

	struct client *c = client_connect();
	pthread_t thread;

	/* register read intention */
	register_reading(c->wl_display);

	/* use this var as an indicator that thread is sleeping */
	c->display_stopped = 0;

	/* create new thread that will register its intention too */
	thread = create_thread(c, thread_read_error);

	/* so now we have sleeping thread waiting for a pthread_cond signal.
	 * The main thread must call wl_display_read_events().
	 * If this call fails, then it won't call broadcast at the
	 * end of the function and the sleeping thread will block indefinitely.
	 * Make the call fail and watch if libwayland will unblock the thread! */

	/* create error on fd, so that wl_display_read_events will fail.
	 * The same can happen when server hangs up */
	close(wl_display_get_fd(c->wl_display));
	/* this read events will fail and will
	 * post an error that should wake the sleeping thread
	 * and dispatch the incoming events */
	assert(wl_display_read_events(c->wl_display) == -1);

	/* kill test in 3 seconds. This should be enough time for the
	 * thread to exit if it's not blocking. If everything is OK, than
	 * the thread was woken up and the test will end before the SIGALRM */
	test_set_timeout(3);
	pthread_join(thread, NULL);

	client_disconnect_nocheck(c);
}

TEST(threading_errors_tst)
{
	struct display *d = display_create();

	client_create(d, threading_post_err);
	display_run(d);

	display_destroy(d);
}

static void *
thread_prepare_and_read(void *data)
{
	struct client *c = data;

	register_reading(c->wl_display);

	c->display_stopped = 1;

	assert(wl_display_read_events(c->wl_display) == 0);
	assert(wl_display_dispatch_pending(c->wl_display) == 0);

	pthread_exit(NULL);
}

/* test cancel read*/
static void
threading_cancel_read(void)
{
	DISABLE_LEAK_CHECKS;

	struct client *c = client_connect();
	pthread_t th1, th2, th3;

	register_reading(c->wl_display);

	th1 = create_thread(c, thread_prepare_and_read);
	th2 = create_thread(c, thread_prepare_and_read);
	th3 = create_thread(c, thread_prepare_and_read);

	/* all the threads are sleeping, waiting until read or cancel
	 * is called. Cancel the read and let the threads proceed */
	wl_display_cancel_read(c->wl_display);

	/* kill test in 3 seconds. This should be enough time for the
	 * thread to exit if it's not blocking. If everything is OK, than
	 * the thread was woken up and the test will end before the SIGALRM */
	test_set_timeout(3);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
	pthread_join(th3, NULL);

	client_disconnect(c);
}

TEST(threading_cancel_read_tst)
{
	struct display *d = display_create();

	client_create(d, threading_cancel_read);
	display_run(d);

	display_destroy(d);
}

static void
threading_read_eagain(void)
{
	DISABLE_LEAK_CHECKS;

	struct client *c = client_connect();
	pthread_t th1, th2, th3;

	register_reading(c->wl_display);

	th1 = create_thread(c, thread_prepare_and_read);
	th2 = create_thread(c, thread_prepare_and_read);
	th3 = create_thread(c, thread_prepare_and_read);

	/* All the threads are sleeping, waiting until read or cancel
	 * is called. Since we have no data on socket waiting,
	 * the wl_connection_read should end up with error and set errno
	 * to EAGAIN. Check if the threads are woken up in this case. */
	assert(wl_display_read_events(c->wl_display) == 0);
	/* errno should be still set to EAGAIN if wl_connection_read
	 * set it - check if we're testing the right case */
	assert(errno == EAGAIN);

	test_set_timeout(3);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
	pthread_join(th3, NULL);

	client_disconnect(c);
}

TEST(threading_read_eagain_tst)
{
	struct display *d = display_create();
	client_create(d, threading_read_eagain);

	display_run(d);

	display_destroy(d);
}

static void *
thread_prepare_and_read2(void *data)
{
	struct client *c = data;

	while(wl_display_prepare_read(c->wl_display) != 0 && errno == EAGAIN)
		assert(wl_display_dispatch_pending(c->wl_display) == -1);
	assert(wl_display_flush(c->wl_display) == -1);

	c->display_stopped = 1;

	assert(wl_display_read_events(c->wl_display) == -1);
	assert(wl_display_dispatch_pending(c->wl_display) == -1);

	pthread_exit(NULL);
}

static void
threading_read_after_error(void)
{
	DISABLE_LEAK_CHECKS;

	struct client *c = client_connect();
	pthread_t thread;

	/* create an error */
	close(wl_display_get_fd(c->wl_display));
	assert(wl_display_dispatch(c->wl_display) == -1);

	/* try to prepare for reading */
	while(wl_display_prepare_read(c->wl_display) != 0 && errno == EAGAIN)
		assert(wl_display_dispatch_pending(c->wl_display) == -1);
	assert(wl_display_flush(c->wl_display) == -1);

	assert(pthread_create(&thread, NULL,
			      thread_prepare_and_read2, c) == 0);

	/* make sure thread is sleeping */
	while (c->display_stopped == 0)
		test_usleep(500);
	test_usleep(10000);

	assert(wl_display_read_events(c->wl_display) == -1);

	/* kill test in 3 seconds */
	test_set_timeout(3);
	pthread_join(thread, NULL);

	client_disconnect_nocheck(c);
}

TEST(threading_read_after_error_tst)
{
	struct display *d = display_create();

	client_create(d, threading_read_after_error);
	display_run(d);

	display_destroy(d);
}
