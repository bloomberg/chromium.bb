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

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdlib.h>

#include "os-compatibility.h"

static int
set_cloexec_or_close(int fd)
{
	long flags;

	if (fd == -1)
		return -1;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		goto err;

	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
		goto err;

	return fd;

err:
	close(fd);
	return -1;
}

int
os_socketpair_cloexec(int domain, int type, int protocol, int *sv)
{
	int ret;

#ifdef SOCK_CLOEXEC
	ret = socketpair(domain, type | SOCK_CLOEXEC, protocol, sv);
	if (ret == 0 || errno != EINVAL)
		return ret;
#endif

	ret = socketpair(domain, type, protocol, sv);
	if (ret < 0)
		return ret;

	sv[0] = set_cloexec_or_close(sv[0]);
	sv[1] = set_cloexec_or_close(sv[1]);

	if (sv[0] != -1 && sv[1] != -1)
		return 0;

	close(sv[0]);
	close(sv[1]);
	return -1;
}

int
os_epoll_create_cloexec(void)
{
	int fd;

#ifdef EPOLL_CLOEXEC
	fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd >= 0)
		return fd;
	if (errno != EINVAL)
		return -1;
#endif

	fd = epoll_create(1);
	return set_cloexec_or_close(fd);
}

static int
create_tmpfile_cloexec(char *tmpname)
{
	int fd;

#ifdef HAVE_MKOSTEMP
	fd = mkostemp(tmpname, O_CLOEXEC);
	if (fd >= 0)
		unlink(tmpname);
#else
	fd = mkstemp(tmpname);
	if (fd >= 0) {
		fd = set_cloexec_or_close(fd);
		unlink(tmpname);
	}
#endif

	return fd;
}

/*
 * Create a new, unique, anonymous file of the given size, and
 * return the file descriptor for it. The file descriptor is set
 * CLOEXEC. The file is immediately suitable for mmap()'ing
 * the given size at offset zero.
 *
 * The file should not have a permanent backing store like a disk,
 * but may have if XDG_RUNTIME_DIR is not properly implemented in OS.
 *
 * The file name is deleted from the file system.
 *
 * The file is suitable for buffer sharing between processes by
 * transmitting the file descriptor over Unix sockets using the
 * SCM_RIGHTS methods.
 */
int
os_create_anonymous_file(off_t size)
{
	static const char template[] = "/weston-shared-XXXXXX";
	const char *path;
	char *name;
	int fd;

	path = getenv("XDG_RUNTIME_DIR");
	if (!path) {
		errno = ENOENT;
		return -1;
	}

	name = malloc(strlen(path) + sizeof(template));
	if (!name)
		return -1;

	strcpy(name, path);
	strcat(name, template);

	fd = create_tmpfile_cloexec(name);

	free(name);

	if (fd < 0)
		return -1;

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}
