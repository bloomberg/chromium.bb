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
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/major.h>

#include "compositor.h"
#include "weston-launch.h"
#include "launcher-impl.h"

#define DRM_MAJOR 226

#ifndef KDSKBMUTE
#define KDSKBMUTE	0x4B51
#endif

#ifdef HAVE_LIBDRM

#include <xf86drm.h>

static inline int
is_drm_master(int drm_fd)
{
	drm_magic_t magic;

	return drmGetMagic(drm_fd, &magic) == 0 &&
		drmAuthMagic(drm_fd, magic) == 0;
}

#else

static inline int
drmDropMaster(int drm_fd)
{
	return 0;
}

static inline int
drmSetMaster(int drm_fd)
{
	return 0;
}

static inline int
is_drm_master(int drm_fd)
{
	return 0;
}

#endif


union cmsg_data { unsigned char b[4]; int fd; };

struct launcher_weston_launch {
	struct weston_launcher base;
	struct weston_compositor *compositor;
	struct wl_event_loop *loop;
	int fd;
	struct wl_event_source *source;

	int kb_mode, tty, drm_fd;
	struct wl_event_source *vt_source;
};

static int
launcher_weston_launch_open(struct weston_launcher *launcher_base,
		     const char *path, int flags)
{
	struct launcher_weston_launch *launcher = wl_container_of(launcher_base, launcher, base);
	int n, ret;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	union cmsg_data *data;
	char control[CMSG_SPACE(sizeof data->fd)];
	ssize_t len;
	struct weston_launcher_open *message;

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

static void
launcher_weston_launch_close(struct weston_launcher *launcher_base, int fd)
{
	close(fd);
}

static void
launcher_weston_launch_restore(struct weston_launcher *launcher_base)
{
	struct launcher_weston_launch *launcher = wl_container_of(launcher_base, launcher, base);
	struct vt_mode mode = { 0 };

	if (ioctl(launcher->tty, KDSKBMUTE, 0) &&
	    ioctl(launcher->tty, KDSKBMODE, launcher->kb_mode))
		weston_log("failed to restore kb mode: %m\n");

	if (ioctl(launcher->tty, KDSETMODE, KD_TEXT))
		weston_log("failed to set KD_TEXT mode on tty: %m\n");

	/* We have to drop master before we switch the VT back in
	 * VT_AUTO, so we don't risk switching to a VT with another
	 * display server, that will then fail to set drm master. */
	drmDropMaster(launcher->drm_fd);

	mode.mode = VT_AUTO;
	if (ioctl(launcher->tty, VT_SETMODE, &mode) < 0)
		weston_log("could not reset vt handling\n");
}

static int
launcher_weston_launch_data(int fd, uint32_t mask, void *data)
{
	struct launcher_weston_launch *launcher = data;
	int len, ret;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		weston_log("launcher socket closed, exiting\n");
		/* Normally the weston-launch will reset the tty, but
		 * in this case it died or something, so do it here so
		 * we don't end up with a stuck vt. */
		launcher_weston_launch_restore(&launcher->base);
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

static int
launcher_weston_launch_activate_vt(struct weston_launcher *launcher_base, int vt)
{
	struct launcher_weston_launch *launcher = wl_container_of(launcher_base, launcher, base);
	return ioctl(launcher->tty, VT_ACTIVATE, vt);
}

static int
launcher_weston_launch_connect(struct weston_launcher **out, struct weston_compositor *compositor,
			       int tty, const char *seat_id, bool sync_drm)
{
	struct launcher_weston_launch *launcher;
	struct wl_event_loop *loop;

	launcher = malloc(sizeof *launcher);
	if (launcher == NULL)
		return -ENOMEM;

	launcher->base.iface = &launcher_weston_launch_iface;
	* (struct launcher_weston_launch **) out = launcher;
	launcher->compositor = compositor;
	launcher->drm_fd = -1;
	launcher->fd = weston_environment_get_fd("WESTON_LAUNCH_SOCK");
	if (launcher->fd != -1) {
		launcher->tty = weston_environment_get_fd("WESTON_TTY_FD");
		/* We don't get a chance to read out the original kb
		 * mode for the tty, so just hard code K_UNICODE here
		 * in case we have to clean if weston-launch dies. */
		launcher->kb_mode = K_UNICODE;

		loop = wl_display_get_event_loop(compositor->wl_display);
		launcher->source = wl_event_loop_add_fd(loop, launcher->fd,
							WL_EVENT_READABLE,
							launcher_weston_launch_data,
							launcher);
		if (launcher->source == NULL) {
			free(launcher);
			return -ENOMEM;
		}

		return 0;
	} else {
		return -1;
	}
}

static void
launcher_weston_launch_destroy(struct weston_launcher *launcher_base)
{
	struct launcher_weston_launch *launcher = wl_container_of(launcher_base, launcher, base);

	if (launcher->fd != -1) {
		close(launcher->fd);
		wl_event_source_remove(launcher->source);
	} else {
		launcher_weston_launch_restore(&launcher->base);
		wl_event_source_remove(launcher->vt_source);
	}

	if (launcher->tty >= 0)
		close(launcher->tty);

	free(launcher);
}

struct launcher_interface launcher_weston_launch_iface = {
	launcher_weston_launch_connect,
	launcher_weston_launch_destroy,
	launcher_weston_launch_open,
	launcher_weston_launch_close,
	launcher_weston_launch_activate_vt,
	launcher_weston_launch_restore,
};
