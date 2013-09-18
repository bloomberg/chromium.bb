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
	struct wl_event_source *source;
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

static int
weston_launcher_data(int fd, uint32_t mask, void *data)
{
	struct weston_launcher *launcher = data;
	int len, ret;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		weston_log("launcher socket closed, exiting\n");
		exit(-1);
	}

	do {
		len = recv(launcher->fd, &ret, sizeof ret, 0);
	} while (len < 0 && errno == EINTR);

	switch (ret) {
	case WESTON_LAUNCHER_ACTIVATE:
		launcher->compositor->session_active = 1;
		wl_signal_emit(&launcher->compositor->session_signal,
			       launcher->compositor);
		break;
	case WESTON_LAUNCHER_DEACTIVATE:
		launcher->compositor->session_active = 0;
		wl_signal_emit(&launcher->compositor->session_signal,
			       launcher->compositor);
		break;
	default:
		weston_log("unexpected event from weston-launch\n");
		break;
	}

	return 1;
}

struct weston_launcher *
weston_launcher_connect(struct weston_compositor *compositor)
{
	struct weston_launcher *launcher;
	struct wl_event_loop *loop;
	int fd;

	fd = weston_environment_get_fd("WESTON_LAUNCHER_SOCK");
	if (fd == -1)
		return NULL;

	launcher = malloc(sizeof *launcher);
	if (launcher == NULL)
		return NULL;

	launcher->compositor = compositor;
	launcher->fd = fd;

	loop = wl_display_get_event_loop(compositor->wl_display);
	launcher->source = wl_event_loop_add_fd(loop, launcher->fd,
						WL_EVENT_READABLE,
						weston_launcher_data,
						launcher);
	if (launcher->source == NULL) {
		free(launcher);
		return NULL;
	}

	return launcher;
}

void
weston_launcher_destroy(struct weston_launcher *launcher)
{
	close(launcher->fd);
	free(launcher);
}
