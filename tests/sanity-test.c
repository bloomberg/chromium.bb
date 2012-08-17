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

FAIL_TEST(sanity_malloc_indirect)
{
	struct wl_array array;

	assert(leak_check_enabled);

	wl_array_init(&array);

	/* call into library that calls malloc */
	wl_array_add(&array, 14);

	/* not freeing array, must leak */
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
