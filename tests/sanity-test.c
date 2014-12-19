/*
 * Copyright © 2012 Collabora, Ltd.
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

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "test-runner.h"
#include "wayland-util.h"

#define WL_HIDE_DEPRECATED
#include "test-compositor.h"

extern int leak_check_enabled;

TEST(empty)
{
}

TEST(exit_success)
{
	exit(EXIT_SUCCESS);
}

FAIL_TEST(exit_failure)
{
	exit(EXIT_FAILURE);
}

FAIL_TEST(fail_abort)
{
	abort();
}

FAIL_TEST(fail_kill)
{
	kill(getpid(), SIGTERM);
}

FAIL_TEST(fail_segv)
{
	* (char **) 0 = "Goodbye, world";
}

FAIL_TEST(sanity_assert)
{
	/* must fail */
	assert(0);
}

FAIL_TEST(sanity_malloc_direct)
{
	void *p;

	assert(leak_check_enabled);

	p = malloc(10);	/* memory leak */
	assert(p);	/* assert that we got memory, also prevents
			 * the malloc from getting optimized away. */
	free(NULL);	/* NULL must not be counted */
}

TEST(disable_leak_checks)
{
	volatile void *mem;
	assert(leak_check_enabled);
	/* normally this should be on the beginning of the test.
	 * Here we need to be sure, that the leak checks are
	 * turned on */
	DISABLE_LEAK_CHECKS;

	mem = malloc(16);
	assert(mem);
}

FAIL_TEST(sanity_malloc_indirect)
{
	struct wl_array array;

	assert(leak_check_enabled);

	wl_array_init(&array);

	/* call into library that calls malloc */
	wl_array_add(&array, 14);

	/* not freeing array, must leak */
}

FAIL_TEST(tc_client_memory_leaks)
{
	struct display *d = display_create();
	client_create(d, sanity_malloc_direct);
	display_run(d);
	display_destroy(d);
}

FAIL_TEST(tc_client_memory_leaks2)
{
	struct display *d = display_create();
	client_create(d, sanity_malloc_indirect);
	display_run(d);
	display_destroy(d);
}

FAIL_TEST(sanity_fd_leak)
{
	int fd[2];

	assert(leak_check_enabled);

	/* leak 2 file descriptors */
	if (pipe(fd) < 0)
		exit(EXIT_SUCCESS); /* failed to fail */
}

FAIL_TEST(sanity_fd_leak_exec)
{
	int fd[2];
	int nr_fds = count_open_fds();

	/* leak 2 file descriptors */
	if (pipe(fd) < 0)
		exit(EXIT_SUCCESS); /* failed to fail */

	exec_fd_leak_check(nr_fds);
}

TEST(sanity_fd_exec)
{
	int fd[2];
	int nr_fds = count_open_fds();

	/* create 2 file descriptors, that should pass over exec */
	assert(pipe(fd) >= 0);

	exec_fd_leak_check(nr_fds + 2);
}

static void
sanity_fd_no_leak(void)
{
	int fd[2];

	assert(leak_check_enabled);

	/* leak 2 file descriptors */
	if (pipe(fd) < 0)
		exit(EXIT_SUCCESS); /* failed to fail */

	close(fd[0]);
	close(fd[1]);
}

static void
sanity_client_no_leak(void)
{
	struct wl_display *display = wl_display_connect(NULL);
	assert(display);

	wl_display_disconnect(display);
}

TEST(tc_client_no_fd_leaks)
{
	struct display *d = display_create();

	/* Client which does not consume the WAYLAND_DISPLAY socket. */
	client_create(d, sanity_fd_no_leak);
	display_run(d);

	/* Client which does consume the WAYLAND_DISPLAY socket. */
	client_create(d, sanity_client_no_leak);
	display_run(d);

	display_destroy(d);
}

FAIL_TEST(tc_client_fd_leaks)
{
	struct display *d = display_create();

	client_create(d, sanity_fd_leak);
	display_run(d);

	display_destroy(d);
}

FAIL_TEST(tc_client_fd_leaks_exec)
{
	struct display *d = display_create();

	client_create(d, sanity_fd_leak);
	display_run(d);

	display_destroy(d);
}

FAIL_TEST(timeout_tst)
{
	test_set_timeout(1);
	/* test should reach timeout */
	test_sleep(2);
}

TEST(timeout2_tst)
{
	/* the test should end before reaching timeout,
	 * thus it should pass */
	test_set_timeout(1);
	/* 200 000 microsec = 0.2 sec */
	test_usleep(200000);
}

FAIL_TEST(timeout_reset_tst)
{
	test_set_timeout(5);
	test_set_timeout(10);
	test_set_timeout(1);

	/* test should fail on timeout */
	test_sleep(2);
}

TEST(timeout_turnoff)
{
	test_set_timeout(1);
	test_set_timeout(0);

	test_usleep(2);
}

/* test timeouts with test-compositor */
FAIL_TEST(tc_timeout_tst)
{
	struct display *d = display_create();
	client_create(d, timeout_tst);
	display_run(d);
	display_destroy(d);
}

FAIL_TEST(tc_timeout2_tst)
{
	struct display *d = display_create();
	client_create(d, timeout_reset_tst);
	display_run(d);
	display_destroy(d);
}

TEST(tc_timeout3_tst)
{
	struct display *d = display_create();

	client_create(d, timeout2_tst);
	display_run(d);

	client_create(d, timeout_turnoff);
	display_run(d);

	display_destroy(d);
}
