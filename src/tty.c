/*
 * Copyright © 2010 Intel Corporation
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

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/major.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "compositor.h"

/* Introduced in 2.6.38 */
#ifndef K_OFF
#define K_OFF 0x04
#endif

struct tty {
	struct weston_compositor *compositor;
	int fd;
	struct termios terminal_attributes;

	struct wl_event_source *input_source;
	struct wl_event_source *vt_source;
	tty_vt_func_t vt_func;
	int vt, starting_vt, has_vt;
	int kb_mode;
};

static int vt_handler(int signal_number, void *data)
{
	struct tty *tty = data;

	if (tty->has_vt) {
		tty->vt_func(tty->compositor, TTY_LEAVE_VT);
		tty->has_vt = 0;

		ioctl(tty->fd, VT_RELDISP, 1);
	} else {
		ioctl(tty->fd, VT_RELDISP, VT_ACKACQ);

		tty->vt_func(tty->compositor, TTY_ENTER_VT);
		tty->has_vt = 1;
	}

	return 1;
}

static int
on_tty_input(int fd, uint32_t mask, void *data)
{
	struct tty *tty = data;

	/* Ignore input to tty.  We get keyboard events from evdev */
	tcflush(tty->fd, TCIFLUSH);

	return 1;
}

static int
try_open_vt(struct tty *tty)
{
	int tty0, fd;
	char filename[16];

	tty0 = open("/dev/tty0", O_WRONLY | O_CLOEXEC);
	if (tty0 < 0) {
		weston_log("could not open tty0: %m\n");
		return -1;
	}

	if (ioctl(tty0, VT_OPENQRY, &tty->vt) < 0 || tty->vt == -1) {
		weston_log("could not open tty0: %m\n");
		close(tty0);
		return -1;
	}

	close(tty0);
	snprintf(filename, sizeof filename, "/dev/tty%d", tty->vt);
	weston_log("compositor: using new vt %s\n", filename);
	fd = open(filename, O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (fd < 0)
		return fd;

	return fd;
}

int
tty_activate_vt(struct tty *tty, int vt)
{
	return ioctl(tty->fd, VT_ACTIVATE, vt);
}

struct tty *
tty_create(struct weston_compositor *compositor, tty_vt_func_t vt_func,
           int tty_nr)
{
	struct termios raw_attributes;
	struct vt_mode mode = { 0 };
	int ret;
	struct tty *tty;
	struct wl_event_loop *loop;
	struct stat buf;
	char filename[16];
	struct vt_stat vts;

	tty = zalloc(sizeof *tty);
	if (tty == NULL)
		return NULL;

	tty->compositor = compositor;
	tty->vt_func = vt_func;

	tty->fd = weston_environment_get_fd("WESTON_TTY_FD");
	if (tty->fd < 0)
		tty->fd = STDIN_FILENO;

	if (tty_nr > 0) {
		snprintf(filename, sizeof filename, "/dev/tty%d", tty_nr);
		weston_log("compositor: using %s\n", filename);
		tty->fd = open(filename, O_RDWR | O_NOCTTY | O_CLOEXEC);
		tty->vt = tty_nr;
	} else if (fstat(tty->fd, &buf) == 0 &&
		   major(buf.st_rdev) == TTY_MAJOR &&
		   minor(buf.st_rdev) > 0) {
		if (tty->fd == STDIN_FILENO)
			tty->fd = fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0);
		tty->vt = minor(buf.st_rdev);
	} else {
		/* Fall back to try opening a new VT.  This typically
		 * requires root. */
		tty->fd = try_open_vt(tty);
	}

	if (tty->fd < 0) {
		weston_log("failed to open tty: %m\n");
		free(tty);
		return NULL;
	}

	if (ioctl(tty->fd, VT_GETSTATE, &vts) == 0)
		tty->starting_vt = vts.v_active;
	else
		tty->starting_vt = tty->vt;

	if (tty->starting_vt != tty->vt) {
		if (ioctl(tty->fd, VT_ACTIVATE, tty->vt) < 0 ||
		    ioctl(tty->fd, VT_WAITACTIVE, tty->vt) < 0) {
			weston_log("failed to swtich to new vt\n");
			goto err;
		}
	}

	if (tcgetattr(tty->fd, &tty->terminal_attributes) < 0) {
		weston_log("could not get terminal attributes: %m\n");
		goto err;
	}

	/* Ignore control characters and disable echo */
	raw_attributes = tty->terminal_attributes;
	cfmakeraw(&raw_attributes);

	/* Fix up line endings to be normal (cfmakeraw hoses them) */
	raw_attributes.c_oflag |= OPOST | OCRNL;

	if (tcsetattr(tty->fd, TCSANOW, &raw_attributes) < 0)
		weston_log("could not put terminal into raw mode: %m\n");

	loop = wl_display_get_event_loop(compositor->wl_display);

	ioctl(tty->fd, KDGKBMODE, &tty->kb_mode);
	ret = ioctl(tty->fd, KDSKBMODE, K_OFF);
	if (ret) {
		ret = ioctl(tty->fd, KDSKBMODE, K_RAW);
		if (ret) {
			weston_log("failed to set keyboard mode on tty: %m\n");
			goto err_attr;
		}

		tty->input_source = wl_event_loop_add_fd(loop, tty->fd,
							 WL_EVENT_READABLE,
							 on_tty_input, tty);
		if (!tty->input_source)
			goto err_kdkbmode;
	}

	ret = ioctl(tty->fd, KDSETMODE, KD_GRAPHICS);
	if (ret) {
		weston_log("failed to set KD_GRAPHICS mode on tty: %m\n");
		goto err_kdkbmode;
	}

	tty->has_vt = 1;
	mode.mode = VT_PROCESS;
	mode.relsig = SIGUSR1;
	mode.acqsig = SIGUSR1;
	if (ioctl(tty->fd, VT_SETMODE, &mode) < 0) {
		weston_log("failed to take control of vt handling\n");
		goto err_kdmode;
	}

	tty->vt_source =
		wl_event_loop_add_signal(loop, SIGUSR1, vt_handler, tty);
	if (!tty->vt_source)
		goto err_vtmode;

	return tty;

err_vtmode:
	ioctl(tty->fd, VT_SETMODE, &mode);

err_kdmode:
	ioctl(tty->fd, KDSETMODE, KD_TEXT);

err_kdkbmode:
	if (tty->input_source)
		wl_event_source_remove(tty->input_source);
	ioctl(tty->fd, KDSKBMODE, tty->kb_mode);

err_attr:
	tcsetattr(tty->fd, TCSANOW, &tty->terminal_attributes);

err:
	close(tty->fd);
	free(tty);
	return NULL;
}

void tty_reset(struct tty *tty)
{
	struct vt_mode mode = { 0 };

	if (ioctl(tty->fd, KDSKBMODE, tty->kb_mode))
		weston_log("failed to restore keyboard mode: %m\n");

	if (ioctl(tty->fd, KDSETMODE, KD_TEXT))
		weston_log("failed to set KD_TEXT mode on tty: %m\n");

	if (tcsetattr(tty->fd, TCSANOW, &tty->terminal_attributes) < 0)
		weston_log("could not restore terminal to canonical mode\n");

	mode.mode = VT_AUTO;
	if (ioctl(tty->fd, VT_SETMODE, &mode) < 0)
		weston_log("could not reset vt handling\n");

	if (tty->has_vt && tty->vt != tty->starting_vt) {
		ioctl(tty->fd, VT_ACTIVATE, tty->starting_vt);
		ioctl(tty->fd, VT_WAITACTIVE, tty->starting_vt);
	}
}

void
tty_destroy(struct tty *tty)
{
	if (tty->input_source)
		wl_event_source_remove(tty->input_source);

	wl_event_source_remove(tty->vt_source);

	tty_reset(tty);

	close(tty->fd);

	free(tty);
}
