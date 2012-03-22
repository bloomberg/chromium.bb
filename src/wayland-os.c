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

#include "../config.h"
#include "wayland-os.h"

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
wl_os_socket_cloexec(int domain, int type, int protocol)
{
	int fd;

	fd = socket(domain, type | SOCK_CLOEXEC, protocol);
	if (fd >= 0)
		return fd;
	if (errno != EINVAL)
		return -1;

	fd = socket(domain, type, protocol);
	return set_cloexec_or_close(fd);
}

int
wl_os_dupfd_cloexec(int fd, long minfd)
{
	int newfd;

	newfd = fcntl(fd, F_DUPFD_CLOEXEC, minfd);
	if (newfd >= 0)
		return newfd;
	if (errno != EINVAL)
		return -1;

	newfd = fcntl(fd, F_DUPFD, minfd);
	return set_cloexec_or_close(newfd);
}

static ssize_t
recvmsg_cloexec_fallback(int sockfd, struct msghdr *msg, int flags)
{
	ssize_t len;
	struct cmsghdr *cmsg;
	unsigned char *data;
	int *fd;
	int *end;

	len = recvmsg(sockfd, msg, flags);
	if (len == -1)
		return -1;

	if (!msg->msg_control || msg->msg_controllen == 0)
		return len;

	cmsg = CMSG_FIRSTHDR(msg);
	for (; cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET ||
		    cmsg->cmsg_type != SCM_RIGHTS)
			continue;

		data = CMSG_DATA(cmsg);
		end = (int *)(data + cmsg->cmsg_len - CMSG_LEN(0));
		for (fd = (int *)data; fd < end; ++fd)
			*fd = set_cloexec_or_close(*fd);
	}

	return len;
}

ssize_t
wl_os_recvmsg_cloexec(int sockfd, struct msghdr *msg, int flags)
{
	ssize_t len;

	len = recvmsg(sockfd, msg, flags | MSG_CMSG_CLOEXEC);
	if (len >= 0)
		return len;
	if (errno != EINVAL)
		return -1;

	return recvmsg_cloexec_fallback(sockfd, msg, flags);
}

int
wl_os_epoll_create_cloexec(void)
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

int
wl_os_accept_cloexec(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int fd;

#ifdef HAVE_ACCEPT4
	fd = accept4(sockfd, addr, addrlen, SOCK_CLOEXEC);
	if (fd >= 0)
		return fd;
	if (errno != ENOSYS)
		return -1;
#endif

	fd = accept(sockfd, addr, addrlen);
	return set_cloexec_or_close(fd);
}
