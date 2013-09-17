/*
 * Copyright © 2012 Benjamin Franzke
 * Copyright © 2013 Intel Corporation
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>

#include <xf86drm.h>

#include "compositor.h"
#include "launcher-util.h"
#include "weston-launch.h"

union cmsg_data { unsigned char b[4]; int fd; };

struct weston_launcher {
	struct weston_compositor *compositor;
	int fd;
};

int
weston_launcher_open(struct weston_launcher *launcher,
		     const char *path, int flags)
{
	int n, ret = -1;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	union cmsg_data *data;
	char control[CMSG_SPACE(sizeof data->fd)];
	ssize_t len;
	struct weston_launcher_open *message;

	if (launcher == NULL)
		return open(path, flags | O_CLOEXEC);

	n = sizeof(*message) + strlen(path) + 1;
	message = malloc(n);
	if (!message)
		return -1;

	message->header.opcode = WESTON_LAUNCHER_OPEN;
	message->flags = flags;
	strcpy(message->path, path);

	do {
		len = send(launcher->fd, message, n, 0);
	} while (len < 0 && errno == EINTR);
	free(message);

	memset(&msg, 0, sizeof msg);
	iov.iov_base = &ret;
	iov.iov_len = sizeof ret;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof control;
	
	do {
		len = recvmsg(launcher->fd, &msg, MSG_CMSG_CLOEXEC);
	} while (len < 0 && errno == EINTR);

	if (len != sizeof ret ||
	    ret < 0)
		return -1;

	cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg ||
	    cmsg->cmsg_level != SOL_SOCKET ||
	    cmsg->cmsg_type != SCM_RIGHTS) {
		fprintf(stderr, "invalid control message\n");
		return -1;
	}

	data = (union cmsg_data *) CMSG_DATA(cmsg);
	if (data->fd == -1) {
		fprintf(stderr, "missing drm fd in socket request\n");
		return -1;
	}

	return data->fd;
}

int
weston_launcher_drm_set_master(struct weston_launcher *launcher,
			       int drm_fd, char master)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	char control[CMSG_SPACE(sizeof(drm_fd))];
	int ret;
	ssize_t len;
	struct weston_launcher_set_master message;
	union cmsg_data *data;

	if (launcher == NULL) {
		if (master)
			return drmSetMaster(drm_fd);
		else
			return drmDropMaster(drm_fd);
	}

	memset(&msg, 0, sizeof msg);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof control;
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(drm_fd));

	data = (union cmsg_data *) CMSG_DATA(cmsg);
	data->fd = drm_fd;
	msg.msg_controllen = cmsg->cmsg_len;

	iov.iov_base = &message;
	iov.iov_len = sizeof message;

	message.header.opcode = WESTON_LAUNCHER_DRM_SET_MASTER;
	message.set_master = master;

	do {
		len = sendmsg(launcher->fd, &msg, 0);
	} while (len < 0 && errno == EINTR);
	if (len < 0)
		return -1;

	do {
		len = recv(launcher->fd, &ret, sizeof ret, 0);
	} while (len < 0 && errno == EINTR);
	if (len < 0)
		return -1;

	return ret;
}

struct weston_launcher *
weston_launcher_connect(struct weston_compositor *compositor)
{
	struct weston_launcher *launcher;
	int fd;

	fd = weston_environment_get_fd("WESTON_LAUNCHER_SOCK");
	if (fd == -1)
		return NULL;

	launcher = malloc(sizeof *launcher);
	if (launcher == NULL)
		return NULL;

	launcher->compositor = compositor;
	launcher->fd = fd;

	return launcher;
}

void
weston_launcher_destroy(struct weston_launcher *launcher)
{
	close(launcher->fd);
	free(launcher);
}
