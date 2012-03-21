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

#define _GNU_SOURCE

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>

#include "test-runner.h"
#include "../src/wayland-os.h"

static int fall_back;
static int wrapped_calls;

static int (*real_socket)(int, int, int);

static void
init_fallbacks(int do_fallbacks)
{
	fall_back = do_fallbacks;
	real_socket = dlsym(RTLD_NEXT, "socket");
}

__attribute__ ((visibility("default"))) int
socket(int domain, int type, int protocol)
{
	wrapped_calls++;

#ifdef SOCK_CLOEXEC
	if (fall_back && (type & SOCK_CLOEXEC)) {
		errno = EINVAL;
		return -1;
	}
#endif

	return real_socket(domain, type, protocol);
}

static void
do_os_wrappers_socket_cloexec(int n)
{
	int fd;
	int nr_fds;

	nr_fds = count_open_fds();

	/* simply create a socket that closes on exec */
	fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);

#ifdef SOCK_CLOEXEC
	/*
	 * Must have 2 calls if falling back, but must also allow
	 * falling back without a forced fallback.
	 */
	assert(wrapped_calls > n);
#else
	assert(wrapped_calls == 1);
#endif
	assert(fd >= 0);

	exec_fd_leak_check(nr_fds);
}

TEST(os_wrappers_socket_cloexec)
{
	/* normal case */
	init_fallbacks(0);
	do_os_wrappers_socket_cloexec(0);
}

TEST(os_wrappers_socket_cloexec_fallback)
{
	/* forced fallback */
	init_fallbacks(1);
	do_os_wrappers_socket_cloexec(1);
}
