/*
 * Copyright © 2012 Benjamin Franzke
 * Copyright © 2013 Intel Corporation
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
#include "launcher-util.h"
#include "launcher-logind.h"
#include "weston-launch.h"

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

struct weston_launcher {
	struct weston_compositor *compositor;
	struct weston_logind *logind;
	struct wl_event_loop *loop;
	int fd;
	struct wl_event_source *source;

	int kb_mode, tty, drm_fd;
	struct wl_event_source *vt_source;
};

int
weston_launcher_open(struct weston_launcher *launcher,
		     const char *path, int flags)
{
	int n, fd, ret = -1;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	union cmsg_data *data;
	char control[CMSG_SPACE(sizeof data->fd)];
	ssize_t len;
	struct weston_launcher_open *message;
	struct stat s;

	/* We really don't want to be leaking fds to child processes so
	 * we force this flag here.  If someone comes up with a legitimate
	 * reason to not CLOEXEC they'll need to unset the flag manually.
	 */
	flags |= O_CLOEXEC;

	if (launcher->logind)
		return weston_logind_open(launcher->logind, path, flags);

	if (launcher->fd == -1) {
		fd = open(path, flags);
		if (fd == -1)
			return -1;

		if (fstat(fd, &s) == -1) {
			close(fd);
			return -1;
		}

		if (major(s.st_rdev) == DRM_MAJOR) {
			launcher->drm_fd = fd;
			if (!is_drm_master(fd)) {
				weston_log("drm fd not master\n");
				close(fd);
				return -1;
			}
		}

		return fd;
	}

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

void
weston_launcher_close(struct weston_launcher *launcher, int fd)
{
	if (launcher->logind)
		weston_logind_close(launcher->logind, fd);

	close(fd);
}

void
weston_launcher_restore(struct weston_launcher *launcher)
{
	struct vt_mode mode = { 0 };

	if (launcher->logind)
		return weston_logind_restore(launcher->logind);

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
weston_launcher_data(int fd, uint32_t mask, void *data)
{
	struct weston_launcher *launcher = data;
	int len, ret;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		weston_log("launcher socket closed, exiting\n");
		/* Normally the weston-launch will reset the tty, but
		 * in this case it died or something, so do it here so
		 * we don't end up with a stuck vt. */
		weston_launcher_restore(launcher);
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
vt_handler(int signal_number, void *data)
{
	struct weston_launcher *launcher = data;
	struct weston_compositor *compositor = launcher->compositor;

	if (compositor->session_active) {
		compositor->session_active = 0;
		wl_signal_emit(&compositor->session_signal, compositor);
		drmDropMaster(launcher->drm_fd);
		ioctl(launcher->tty, VT_RELDISP, 1);
	} else {
		ioctl(launcher->tty, VT_RELDISP, VT_ACKACQ);
		drmSetMaster(launcher->drm_fd);
		compositor->session_active = 1;
		wl_signal_emit(&compositor->session_signal, compositor);
	}

	return 1;
}

static int
setup_tty(struct weston_launcher *launcher, int tty)
{
	struct wl_event_loop *loop;
	struct vt_mode mode = { 0 };
	struct stat buf;
	char tty_device[32] ="<stdin>";
	int ret, kd_mode;

	if (tty == 0) {
		launcher->tty = dup(tty);
		if (launcher->tty == -1) {
			weston_log("couldn't dup stdin: %m\n");
			return -1;
		}
	} else {
		snprintf(tty_device, sizeof tty_device, "/dev/tty%d", tty);
		launcher->tty = open(tty_device, O_RDWR | O_CLOEXEC);
		if (launcher->tty == -1) {
			weston_log("couldn't open tty %s: %m\n", tty_device);
			return -1;
		}
	}

	if (fstat(launcher->tty, &buf) == -1 ||
	    major(buf.st_rdev) != TTY_MAJOR || minor(buf.st_rdev) == 0) {
		weston_log("%s not a vt\n", tty_device);
		weston_log("if running weston from ssh, "
			   "use --tty to specify a tty\n");
		goto err_close;
	}

	ret = ioctl(launcher->tty, KDGETMODE, &kd_mode);
	if (ret) {
		weston_log("failed to get VT mode: %m\n");
		return -1;
	}
	if (kd_mode != KD_TEXT) {
		weston_log("%s is already in graphics mode, "
			   "is another display server running?\n", tty_device);
		goto err_close;
	}

	ioctl(launcher->tty, VT_ACTIVATE, minor(buf.st_rdev));
	ioctl(launcher->tty, VT_WAITACTIVE, minor(buf.st_rdev));

	if (ioctl(launcher->tty, KDGKBMODE, &launcher->kb_mode)) {
		weston_log("failed to read keyboard mode: %m\n");
		goto err_close;
	}

	if (ioctl(launcher->tty, KDSKBMUTE, 1) &&
	    ioctl(launcher->tty, KDSKBMODE, K_OFF)) {
		weston_log("failed to set K_OFF keyboard mode: %m\n");
		goto err_close;
	}

	ret = ioctl(launcher->tty, KDSETMODE, KD_GRAPHICS);
	if (ret) {
		weston_log("failed to set KD_GRAPHICS mode on tty: %m\n");
		goto err_close;
	}

	/*
	 * SIGRTMIN is used as global VT-acquire+release signal. Note that
	 * SIGRT* must be tested on runtime, as their exact values are not
	 * known at compile-time. POSIX requires 32 of them to be available.
	 */
	if (SIGRTMIN > SIGRTMAX) {
		weston_log("not enough RT signals available: %u-%u\n",
			   SIGRTMIN, SIGRTMAX);
		ret = -EINVAL;
		goto err_close;
	}

	mode.mode = VT_PROCESS;
	mode.relsig = SIGRTMIN;
	mode.acqsig = SIGRTMIN;
	if (ioctl(launcher->tty, VT_SETMODE, &mode) < 0) {
		weston_log("failed to take control of vt handling\n");
		goto err_close;
	}

	loop = wl_display_get_event_loop(launcher->compositor->wl_display);
	launcher->vt_source =
		wl_event_loop_add_signal(loop, SIGRTMIN, vt_handler, launcher);
	if (!launcher->vt_source)
		goto err_close;

	return 0;

 err_close:
	close(launcher->tty);
	return -1;
}

int
weston_launcher_activate_vt(struct weston_launcher *launcher, int vt)
{
	if (launcher->logind)
		return weston_logind_activate_vt(launcher->logind, vt);

	return ioctl(launcher->tty, VT_ACTIVATE, vt);
}

struct weston_launcher *
weston_launcher_connect(struct weston_compositor *compositor, int tty,
			const char *seat_id, bool sync_drm)
{
	struct weston_launcher *launcher;
	struct wl_event_loop *loop;
	int r;

	launcher = malloc(sizeof *launcher);
	if (launcher == NULL)
		return NULL;

	launcher->logind = NULL;
	launcher->compositor = compositor;
	launcher->drm_fd = -1;
	launcher->fd = weston_environment_get_fd("WESTON_LAUNCHER_SOCK");
	if (launcher->fd != -1) {
		launcher->tty = weston_environment_get_fd("WESTON_TTY_FD");
		/* We don't get a chance to read out the original kb
		 * mode for the tty, so just hard code K_UNICODE here
		 * in case we have to clean if weston-launch dies. */
		launcher->kb_mode = K_UNICODE;

		loop = wl_display_get_event_loop(compositor->wl_display);
		launcher->source = wl_event_loop_add_fd(loop, launcher->fd,
							WL_EVENT_READABLE,
							weston_launcher_data,
							launcher);
		if (launcher->source == NULL) {
			free(launcher);
			return NULL;
		}
	} else {
		r = weston_logind_connect(&launcher->logind, compositor,
					  seat_id, tty, sync_drm);
		if (r < 0) {
			launcher->logind = NULL;
			if (geteuid() == 0) {
				if (setup_tty(launcher, tty) == -1) {
					free(launcher);
					return NULL;
				}
			} else {
				free(launcher);
				return NULL;
			}
		}
	}

	return launcher;
}

void
weston_launcher_destroy(struct weston_launcher *launcher)
{
	if (launcher->logind) {
		weston_logind_destroy(launcher->logind);
	} else if (launcher->fd != -1) {
		close(launcher->fd);
		wl_event_source_remove(launcher->source);
	} else {
		weston_launcher_restore(launcher);
		wl_event_source_remove(launcher->vt_source);
	}

	if (launcher->tty >= 0)
		close(launcher->tty);

	free(launcher);
}
