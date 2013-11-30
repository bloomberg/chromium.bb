/*
 * Copyright © 2013 David Herrmann
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

#include <errno.h>
#include <dbus.h>
#include <fcntl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <systemd/sd-login.h>
#include <unistd.h>

#include "compositor.h"
#include "dbus.h"
#include "logind-util.h"

#define DRM_MAJOR 226

#ifndef KDSKBMUTE
#define KDSKBMUTE	0x4B51
#endif

struct weston_logind {
	struct weston_compositor *compositor;
	char *seat;
	char *sid;
	unsigned int vtnr;
	int vt;
	int kb_mode;
	int sfd;
	struct wl_event_source *sfd_source;

	DBusConnection *dbus;
	struct wl_event_source *dbus_ctx;
	char *spath;
	DBusPendingCall *pending_active;
};

static int
weston_logind_take_device(struct weston_logind *wl, uint32_t major,
			  uint32_t minor, bool *paused_out)
{
	DBusMessage *m, *reply;
	bool b;
	int r, fd;
	dbus_bool_t paused;

	m = dbus_message_new_method_call("org.freedesktop.login1",
					 wl->spath,
					 "org.freedesktop.login1.Session",
					 "TakeDevice");
	if (!m)
		return -ENOMEM;

	b = dbus_message_append_args(m,
				     DBUS_TYPE_UINT32, &major,
				     DBUS_TYPE_UINT32, &minor,
				     DBUS_TYPE_INVALID);
	if (!b) {
		r = -ENOMEM;
		goto err_unref;
	}

	reply = dbus_connection_send_with_reply_and_block(wl->dbus, m,
							  -1, NULL);
	if (!reply) {
		r = -ENODEV;
		goto err_unref;
	}

	b = dbus_message_get_args(reply, NULL,
				  DBUS_TYPE_UNIX_FD, &fd,
				  DBUS_TYPE_BOOLEAN, &paused,
				  DBUS_TYPE_INVALID);
	if (!b) {
		r = -ENODEV;
		goto err_reply;
	}

	r = fd;
	if (paused_out)
		*paused_out = paused;

err_reply:
	dbus_message_unref(reply);
err_unref:
	dbus_message_unref(m);
	return r;
}

static void
weston_logind_release_device(struct weston_logind *wl, uint32_t major,
			     uint32_t minor)
{
	DBusMessage *m;
	bool b;

	m = dbus_message_new_method_call("org.freedesktop.login1",
					 wl->spath,
					 "org.freedesktop.login1.Session",
					 "ReleaseDevice");
	if (m) {
		b = dbus_message_append_args(m,
					     DBUS_TYPE_UINT32, &major,
					     DBUS_TYPE_UINT32, &minor,
					     DBUS_TYPE_INVALID);
		if (b)
			dbus_connection_send(wl->dbus, m, NULL);
		dbus_message_unref(m);
	}
}

static void
weston_logind_pause_device_complete(struct weston_logind *wl, uint32_t major,
				    uint32_t minor)
{
	DBusMessage *m;
	bool b;

	m = dbus_message_new_method_call("org.freedesktop.login1",
					 wl->spath,
					 "org.freedesktop.login1.Session",
					 "PauseDeviceComplete");
	if (m) {
		b = dbus_message_append_args(m,
					     DBUS_TYPE_UINT32, &major,
					     DBUS_TYPE_UINT32, &minor,
					     DBUS_TYPE_INVALID);
		if (b)
			dbus_connection_send(wl->dbus, m, NULL);
		dbus_message_unref(m);
	}
}

WL_EXPORT int
weston_logind_open(struct weston_logind *wl, const char *path,
		   int flags)
{
	struct stat st;
	int fl, r, fd;

	r = stat(path, &st);
	if (r < 0)
		return -1;
	if (!S_ISCHR(st.st_mode)) {
		errno = ENODEV;
		return -1;
	}

	fd = weston_logind_take_device(wl, major(st.st_rdev),
				       minor(st.st_rdev), NULL);
	if (fd < 0)
		return fd;

	/* Compared to weston_launcher_open() we cannot specify the open-mode
	 * directly. Instead, logind passes us an fd with sane default modes.
	 * For DRM and evdev this means O_RDWR | O_CLOEXEC. If we want
	 * something else, we need to change it afterwards. We currently
	 * only support dropping FD_CLOEXEC and setting O_NONBLOCK. Changing
	 * access-modes is not possible so accept whatever logind passes us. */

	fl = fcntl(fd, F_GETFL);
	if (fl < 0) {
		r = -errno;
		goto err_close;
	}

	if (flags & O_NONBLOCK)
		fl |= O_NONBLOCK;

	r = fcntl(fd, F_SETFL, fl);
	if (r < 0) {
		r = -errno;
		goto err_close;
	}

	fl = fcntl(fd, F_GETFD);
	if (fl < 0) {
		r = -errno;
		goto err_close;
	}

	if (!(flags & O_CLOEXEC))
		fl &= ~FD_CLOEXEC;

	r = fcntl(fd, F_SETFD, fl);
	if (r < 0) {
		r = -errno;
		goto err_close;
	}

	return fd;

err_close:
	close(fd);
	weston_logind_release_device(wl, major(st.st_rdev),
				     minor(st.st_rdev));
	errno = -r;
	return -1;
}

WL_EXPORT void
weston_logind_close(struct weston_logind *wl, int fd)
{
	struct stat st;
	int r;

	r = fstat(fd, &st);
	if (r < 0) {
		weston_log("logind: cannot fstat fd: %m\n");
		return;
	}

	if (!S_ISCHR(st.st_mode)) {
		weston_log("logind: invalid device passed\n");
		return;
	}

	weston_logind_release_device(wl, major(st.st_rdev),
				     minor(st.st_rdev));
}

WL_EXPORT void
weston_logind_restore(struct weston_logind *wl)
{
	struct vt_mode mode = { 0 };

	ioctl(wl->vt, KDSETMODE, KD_TEXT);
	ioctl(wl->vt, KDSKBMUTE, 0);
	ioctl(wl->vt, KDSKBMODE, wl->kb_mode);
	mode.mode = VT_AUTO;
	ioctl(wl->vt, VT_SETMODE, &mode);
}

WL_EXPORT int
weston_logind_activate_vt(struct weston_logind *wl, int vt)
{
	int r;

	r = ioctl(wl->vt, VT_ACTIVATE, vt);
	if (r < 0)
		return -1;

	return 0;
}

static void
weston_logind_set_active(struct weston_logind *wl, bool active)
{
	if (!wl->compositor->session_active == !active)
		return;

	wl->compositor->session_active = active;

	wl_signal_emit(&wl->compositor->session_signal,
		       wl->compositor);
}

static void
get_active_cb(DBusPendingCall *pending, void *data)
{
	struct weston_logind *wl = data;
	DBusMessage *m;
	DBusMessageIter iter, sub;
	int type;
	dbus_bool_t b;

	dbus_pending_call_unref(wl->pending_active);
	wl->pending_active = NULL;

	m = dbus_pending_call_steal_reply(pending);
	if (!m)
		return;

	type = dbus_message_get_type(m);
	if (type != DBUS_MESSAGE_TYPE_METHOD_RETURN)
		goto err_unref;

	if (!dbus_message_iter_init(m, &iter) ||
	    dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
		goto err_unref;

	dbus_message_iter_recurse(&iter, &sub);

	if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_BOOLEAN)
		goto err_unref;

	dbus_message_iter_get_basic(&sub, &b);
	if (!b)
		weston_logind_set_active(wl, false);

err_unref:
	dbus_message_unref(m);
}

static void
weston_logind_get_active(struct weston_logind *wl)
{
	DBusPendingCall *pending;
	DBusMessage *m;
	bool b;
	const char *iface, *name;

	m = dbus_message_new_method_call("org.freedesktop.login1",
					 wl->spath,
					 "org.freedesktop.DBus.Properties",
					 "Get");
	if (!m)
		return;

	iface = "org.freedesktop.login1.Session";
	name = "Active";
	b = dbus_message_append_args(m,
				     DBUS_TYPE_STRING, &iface,
				     DBUS_TYPE_STRING, &name,
				     DBUS_TYPE_INVALID);
	if (!b)
		goto err_unref;

	b = dbus_connection_send_with_reply(wl->dbus, m, &pending, -1);
	if (!b)
		goto err_unref;

	b = dbus_pending_call_set_notify(pending, get_active_cb, wl, NULL);
	if (!b) {
		dbus_pending_call_cancel(pending);
		dbus_pending_call_unref(pending);
		goto err_unref;
	}

	if (wl->pending_active) {
		dbus_pending_call_cancel(wl->pending_active);
		dbus_pending_call_unref(wl->pending_active);
	}
	wl->pending_active = pending;
	return;

err_unref:
	dbus_message_unref(m);
}

static void
disconnected_dbus(struct weston_logind *wl)
{
	weston_log("logind: dbus connection lost, exiting..\n");
	weston_logind_restore(wl);
	exit(-1);
}

static void
session_removed(struct weston_logind *wl, DBusMessage *m)
{
	const char *name, *obj;
	bool r;

	r = dbus_message_get_args(m, NULL,
				  DBUS_TYPE_STRING, &name,
				  DBUS_TYPE_OBJECT_PATH, &obj,
				  DBUS_TYPE_INVALID);
	if (!r) {
		weston_log("logind: cannot parse SessionRemoved dbus signal\n");
		return;
	}

	if (!strcmp(name, wl->sid)) {
		weston_log("logind: our session got closed, exiting..\n");
		weston_logind_restore(wl);
		exit(-1);
	}
}

static void
property_changed(struct weston_logind *wl, DBusMessage *m)
{
	DBusMessageIter iter, sub, entry;
	const char *interface, *name;
	dbus_bool_t b;

	if (!dbus_message_iter_init(m, &iter) ||
	    dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		goto error;

	dbus_message_iter_get_basic(&iter, &interface);

	if (!dbus_message_iter_next(&iter) ||
	    dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		goto error;

	dbus_message_iter_recurse(&iter, &sub);

	while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_DICT_ENTRY) {
		dbus_message_iter_recurse(&sub, &entry);

		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING)
			goto error;

		dbus_message_iter_get_basic(&entry, &name);
		if (!dbus_message_iter_next(&entry))
			goto error;

		if (!strcmp(name, "Active")) {
			if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_BOOLEAN) {
				dbus_message_iter_get_basic(&entry, &b);
				if (!b)
					weston_logind_set_active(wl, false);
				return;
			}
		}

		dbus_message_iter_next(&sub);
	}

	if (!dbus_message_iter_next(&iter) ||
	    dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		goto error;

	dbus_message_iter_recurse(&iter, &sub);

	while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRING) {
		dbus_message_iter_get_basic(&sub, &name);

		if (!strcmp(name, "Active")) {
			weston_logind_get_active(wl);
			return;
		}

		dbus_message_iter_next(&sub);
	}

	return;

error:
	weston_log("logind: cannot parse PropertiesChanged dbus signal\n");
}

static void
device_paused(struct weston_logind *wl, DBusMessage *m)
{
	bool r;
	const char *type;
	uint32_t major, minor;

	r = dbus_message_get_args(m, NULL,
				  DBUS_TYPE_UINT32, &major,
				  DBUS_TYPE_UINT32, &minor,
				  DBUS_TYPE_STRING, &type,
				  DBUS_TYPE_INVALID);
	if (!r) {
		weston_log("logind: cannot parse PauseDevice dbus signal\n");
		return;
	}

	/* "pause" means synchronous pausing. Acknowledge it unconditionally
	 * as we support asynchronous device shutdowns, anyway.
	 * "force" means asynchronous pausing.
	 * "gone" means the device is gone. We handle it the same as "force" as
	 * a following udev event will be caught, too.
	 *
	 * If it's our main DRM device, tell the compositor to go asleep. */

	if (!strcmp(type, "pause"))
		weston_logind_pause_device_complete(wl, major, minor);

	if (major == DRM_MAJOR)
		weston_logind_set_active(wl, false);
}

static void
device_resumed(struct weston_logind *wl, DBusMessage *m)
{
	bool r;
	uint32_t major;

	r = dbus_message_get_args(m, NULL,
				  DBUS_TYPE_UINT32, &major,
				  /*DBUS_TYPE_UINT32, &minor,
				  DBUS_TYPE_UNIX_FD, &fd,*/
				  DBUS_TYPE_INVALID);
	if (!r) {
		weston_log("logind: cannot parse ResumeDevice dbus signal\n");
		return;
	}

	/* DeviceResumed messages provide us a new file-descriptor for
	 * resumed devices. For DRM devices it's the same as before, for evdev
	 * devices it's a new open-file. As we reopen evdev devices, anyway,
	 * there is no need for us to handle this event for evdev. For DRM, we
	 * notify the compositor to wake up. */

	if (major == DRM_MAJOR)
		weston_logind_set_active(wl, true);
}

static DBusHandlerResult
filter_dbus(DBusConnection *c, DBusMessage *m, void *data)
{
	struct weston_logind *wl = data;

	if (dbus_message_is_signal(m, DBUS_INTERFACE_LOCAL, "Disconnected")) {
		disconnected_dbus(wl);
	} else if (dbus_message_is_signal(m, "org.freedesktop.login1.Manager",
					  "SessionRemoved")) {
		session_removed(wl, m);
	} else if (dbus_message_is_signal(m, "org.freedesktop.DBus.Properties",
					  "PropertiesChanged")) {
		property_changed(wl, m);
	} else if (dbus_message_is_signal(m, "org.freedesktop.login1.Session",
					  "PauseDevice")) {
		device_paused(wl, m);
	} else if (dbus_message_is_signal(m, "org.freedesktop.login1.Session",
					  "ResumeDevice")) {
		device_resumed(wl, m);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int
weston_logind_setup_dbus(struct weston_logind *wl)
{
	bool b;
	int r;

	r = asprintf(&wl->spath, "/org/freedesktop/login1/session/%s",
		     wl->sid);
	if (r < 0)
		return -ENOMEM;

	b = dbus_connection_add_filter(wl->dbus, filter_dbus, wl, NULL);
	if (!b) {
		weston_log("logind: cannot add dbus filter\n");
		r = -ENOMEM;
		goto err_spath;
	}

	r = weston_dbus_add_match_signal(wl->dbus,
					 "org.freedesktop.login1",
					 "org.freedesktop.login1.Manager",
					 "SessionRemoved",
					 "/org/freedesktop/login1");
	if (r < 0) {
		weston_log("logind: cannot add dbus match\n");
		goto err_spath;
	}

	r = weston_dbus_add_match_signal(wl->dbus,
					"org.freedesktop.login1",
					"org.freedesktop.login1.Session",
					"PauseDevice",
					wl->spath);
	if (r < 0) {
		weston_log("logind: cannot add dbus match\n");
		goto err_spath;
	}

	r = weston_dbus_add_match_signal(wl->dbus,
					"org.freedesktop.login1",
					"org.freedesktop.login1.Session",
					"ResumeDevice",
					wl->spath);
	if (r < 0) {
		weston_log("logind: cannot add dbus match\n");
		goto err_spath;
	}

	r = weston_dbus_add_match_signal(wl->dbus,
					"org.freedesktop.login1",
					"org.freedesktop.DBus.Properties",
					"PropertiesChanged",
					wl->spath);
	if (r < 0) {
		weston_log("logind: cannot add dbus match\n");
		goto err_spath;
	}

	return 0;

err_spath:
	/* don't remove any dbus-match as the connection is closed, anyway */
	free(wl->spath);
	return r;
}

static void
weston_logind_destroy_dbus(struct weston_logind *wl)
{
	/* don't remove any dbus-match as the connection is closed, anyway */
	free(wl->spath);
}

static int
weston_logind_take_control(struct weston_logind *wl)
{
	DBusError err;
	DBusMessage *m, *reply;
	dbus_bool_t force;
	bool b;
	int r;

	dbus_error_init(&err);

	m = dbus_message_new_method_call("org.freedesktop.login1",
					 wl->spath,
					 "org.freedesktop.login1.Session",
					 "TakeControl");
	if (!m)
		return -ENOMEM;

	force = false;
	b = dbus_message_append_args(m,
				     DBUS_TYPE_BOOLEAN, &force,
				     DBUS_TYPE_INVALID);
	if (!b) {
		r = -ENOMEM;
		goto err_unref;
	}

	reply = dbus_connection_send_with_reply_and_block(wl->dbus,
							  m, -1, &err);
	if (!reply) {
		if (dbus_error_has_name(&err, DBUS_ERROR_UNKNOWN_METHOD))
			weston_log("logind: old systemd version detected\n");
		else
			weston_log("logind: cannot take control over session %s\n", wl->sid);

		dbus_error_free(&err);
		r = -EIO;
		goto err_unref;
	}

	dbus_message_unref(reply);
	dbus_message_unref(m);
	return 0;

err_unref:
	dbus_message_unref(m);
	return r;
}

static void
weston_logind_release_control(struct weston_logind *wl)
{
	DBusMessage *m;

	m = dbus_message_new_method_call("org.freedesktop.login1",
					 wl->spath,
					 "org.freedesktop.login1.Session",
					 "ReleaseControl");
	if (m) {
		dbus_connection_send(wl->dbus, m, NULL);
		dbus_message_unref(m);
	}
}

static int
signal_event(int fd, uint32_t mask, void *data)
{
	struct weston_logind *wl = data;
	struct signalfd_siginfo sig;

	if (read(fd, &sig, sizeof sig) != sizeof sig) {
		weston_log("logind: cannot read signalfd: %m\n");
		return 0;
	}

	switch (sig.ssi_signo) {
	case SIGUSR1:
		ioctl(wl->vt, VT_RELDISP, 1);
		break;
	case SIGUSR2:
		ioctl(wl->vt, VT_RELDISP, VT_ACKACQ);
		break;
	}

	return 0;
}

static int
weston_logind_setup_vt(struct weston_logind *wl)
{
	struct stat st;
	char buf[64];
	struct vt_mode mode = { 0 };
	int r;
	sigset_t mask;
	struct wl_event_loop *loop;

	snprintf(buf, sizeof(buf), "/dev/tty%d", wl->vtnr);
	buf[sizeof(buf) - 1] = 0;

	wl->vt = open(buf, O_RDWR|O_CLOEXEC|O_NONBLOCK);

	if (wl->vt < 0) {
		r = -errno;
		weston_log("logind: cannot open VT %s: %m\n", buf);
		return r;
	}

	if (fstat(wl->vt, &st) == -1 ||
	    major(st.st_rdev) != TTY_MAJOR || minor(st.st_rdev) <= 0 ||
	    minor(st.st_rdev) >= 64) {
		r = -EINVAL;
		weston_log("logind: TTY %s is no virtual terminal\n", buf);
		goto err_close;
	}

	/*r = setsid();
	if (r < 0 && errno != EPERM) {
		r = -errno;
		weston_log("logind: setsid() failed: %m\n");
		goto err_close;
	}

	r = ioctl(wl->vt, TIOCSCTTY, 0);
	if (r < 0)
		weston_log("logind: VT %s already in use\n", buf);*/

	if (ioctl(wl->vt, KDGKBMODE, &wl->kb_mode) < 0) {
		weston_log("logind: cannot read keyboard mode on %s: %m\n",
			   buf);
		wl->kb_mode = K_UNICODE;
	} else if (wl->kb_mode == K_OFF) {
		wl->kb_mode = K_UNICODE;
	}

	if (ioctl(wl->vt, KDSKBMUTE, 1) < 0 &&
	    ioctl(wl->vt, KDSKBMODE, K_OFF) < 0) {
		r = -errno;
		weston_log("logind: cannot set K_OFF KB-mode on %s: %m\n",
			   buf);
		goto err_close;
	}

	if (ioctl(wl->vt, KDSETMODE, KD_GRAPHICS) < 0) {
		r = -errno;
		weston_log("logind: cannot set KD_GRAPHICS mode on %s: %m\n",
			   buf);
		goto err_kbmode;
	}

	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sigaddset(&mask, SIGUSR2);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	wl->sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (wl->sfd < 0) {
		r = -errno;
		weston_log("logind: cannot create signalfd: %m\n");
		goto err_mode;
	}

	loop = wl_display_get_event_loop(wl->compositor->wl_display);
	wl->sfd_source = wl_event_loop_add_fd(loop, wl->sfd,
					      WL_EVENT_READABLE,
					      signal_event, wl);
	if (!wl->sfd_source) {
		r = -errno;
		weston_log("logind: cannot create signalfd source: %m\n");
		goto err_sfd;
	}

	mode.mode = VT_PROCESS;
	mode.relsig = SIGUSR1;
	mode.acqsig = SIGUSR2;
	if (ioctl(wl->vt, VT_SETMODE, &mode) < 0) {
		r = -errno;
		weston_log("logind: cannot take over VT: %m\n");
		goto err_sfd_source;
	}

	weston_log("logind: using VT %s\n", buf);
	return 0;

err_sfd_source:
	wl_event_source_remove(wl->sfd_source);
err_sfd:
	close(wl->sfd);
err_mode:
	ioctl(wl->vt, KDSETMODE, KD_TEXT);
err_kbmode:
	ioctl(wl->vt, KDSKBMUTE, 0);
	ioctl(wl->vt, KDSKBMODE, wl->kb_mode);
err_close:
	close(wl->vt);
	return r;
}

static void
weston_logind_destroy_vt(struct weston_logind *wl)
{
	weston_logind_restore(wl);
	wl_event_source_remove(wl->sfd_source);
	close(wl->sfd);
	close(wl->vt);
}

WL_EXPORT int
weston_logind_connect(struct weston_logind **out,
		      struct weston_compositor *compositor,
		      const char *seat_id, int tty)
{
	struct weston_logind *wl;
	struct wl_event_loop *loop;
	char *t;
	int r;

	wl = calloc(1, sizeof(*wl));
	if (!wl) {
		r = -ENOMEM;
		goto err_out;
	}

	wl->compositor = compositor;

	wl->seat = strdup(seat_id);
	if (!wl->seat) {
		r = -ENOMEM;
		goto err_wl;
	}

	r = sd_pid_get_session(getpid(), &wl->sid);
	if (r < 0) {
		weston_log("logind: not running in a systemd session\n");
		goto err_seat;
	}

	t = NULL;
	r = sd_session_get_seat(wl->sid, &t);
	if (r < 0) {
		weston_log("logind: failed to get session seat\n");
		free(t);
		goto err_session;
	} else if (strcmp(seat_id, t)) {
		weston_log("logind: weston's seat '%s' differs from session-seat '%s'\n",
			   seat_id, t);
		r = -EINVAL;
		free(t);
		goto err_session;
	}
	free(t);

	r = weston_sd_session_get_vt(wl->sid, &wl->vtnr);
	if (r < 0) {
		weston_log("logind: session not running on a VT\n");
		goto err_session;
	} else if (tty > 0 && wl->vtnr != (unsigned int )tty) {
		weston_log("logind: requested VT --tty=%d differs from real session VT %u\n",
			   tty, wl->vtnr);
		r = -EINVAL;
		goto err_session;
	}

	loop = wl_display_get_event_loop(compositor->wl_display);
	r = weston_dbus_open(loop, DBUS_BUS_SYSTEM, &wl->dbus, &wl->dbus_ctx);
	if (r < 0) {
		weston_log("logind: cannot connect to system dbus\n");
		goto err_session;
	}

	r = weston_logind_setup_dbus(wl);
	if (r < 0)
		goto err_dbus;

	r = weston_logind_take_control(wl);
	if (r < 0)
		goto err_dbus_cleanup;

	r = weston_logind_setup_vt(wl);
	if (r < 0)
		goto err_control;

	weston_log("logind: session control granted\n");
	*out = wl;
	return 0;

err_control:
	weston_logind_release_control(wl);
err_dbus_cleanup:
	weston_logind_destroy_dbus(wl);
err_dbus:
	weston_dbus_close(wl->dbus, wl->dbus_ctx);
err_session:
	free(wl->sid);
err_seat:
	free(wl->seat);
err_wl:
	free(wl);
err_out:
	weston_log("logind: cannot setup systemd-logind helper (%d), using legacy fallback\n", r);
	errno = -r;
	return -1;
}

WL_EXPORT void
weston_logind_destroy(struct weston_logind *wl)
{
	if (wl->pending_active) {
		dbus_pending_call_cancel(wl->pending_active);
		dbus_pending_call_unref(wl->pending_active);
	}

	weston_logind_destroy_vt(wl);
	weston_logind_release_control(wl);
	weston_logind_destroy_dbus(wl);
	weston_dbus_close(wl->dbus, wl->dbus_ctx);
	free(wl->sid);
	free(wl->seat);
	free(wl);
}
