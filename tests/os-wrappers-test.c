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
#include <stdarg.h>
#include <fcntl.h>

#include "test-runner.h"
#include "../src/wayland-os.h"

static int fall_back;

static int (*real_socket)(int, int, int);
static int wrapped_calls_socket;

static int (*real_fcntl)(int, int, ...);
static int wrapped_calls_fcntl;

static void
init_fallbacks(int do_fallbacks)
{
	fall_back = do_fallbacks;
	real_socket = dlsym(RTLD_NEXT, "socket");
	real_fcntl = dlsym(RTLD_NEXT, "fcntl");
}

__attribute__ ((visibility("default"))) int
socket(int domain, int type, int protocol)
{
	wrapped_calls_socket++;

	if (fall_back && (type & SOCK_CLOEXEC)) {
		errno = EINVAL;
		return -1;
	}

	return real_socket(domain, type, protocol);
}

__attribute__ ((visibility("default"))) int
fcntl(int fd, int cmd, ...)
{
	va_list ap;
	void *arg;

	wrapped_calls_fcntl++;

	if (fall_back && (cmd == F_DUPFD_CLOEXEC)) {
		errno = EINVAL;
		return -1;
	}

	va_start(ap, cmd);
	arg = va_arg(ap, void*);
	va_end(ap);

	return real_fcntl(fd, cmd, arg);
}

static void
do_os_wrappers_socket_cloexec(int n)
{
	int fd;
	int nr_fds;

	nr_fds = count_open_fds();

	/* simply create a socket that closes on exec */
	fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
	assert(fd >= 0);

	/*
	 * Must have 2 calls if falling back, but must also allow
	 * falling back without a forced fallback.
	 */
	assert(wrapped_calls_socket > n);

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

static void
do_os_wrappers_dupfd_cloexec(int n)
{
	int base_fd;
	int fd;
	int nr_fds;

	nr_fds = count_open_fds();

	base_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
	assert(base_fd >= 0);

	fd = wl_os_dupfd_cloexec(base_fd, 13);
	assert(fd >= 13);

	close(base_fd);

	/*
	 * Must have 4 calls if falling back, but must also allow
	 * falling back without a forced fallback.
	 */
	assert(wrapped_calls_fcntl > n);

	exec_fd_leak_check(nr_fds);
}

TEST(os_wrappers_dupfd_cloexec)
{
	init_fallbacks(0);
	do_os_wrappers_dupfd_cloexec(0);
}

TEST(os_wrappers_dupfd_cloexec_fallback)
{
	init_fallbacks(1);
	do_os_wrappers_dupfd_cloexec(3);
}
