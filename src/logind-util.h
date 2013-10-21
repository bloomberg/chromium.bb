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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compositor.h"

struct weston_logind;

#if defined(HAVE_SYSTEMD_LOGIN) && defined(HAVE_DBUS)

#include <systemd/sd-login.h>

int
weston_logind_open(struct weston_logind *wl, const char *path,
		   int flags);

void
weston_logind_close(struct weston_logind *wl, int fd);

void
weston_logind_restore(struct weston_logind *wl);

int
weston_logind_activate_vt(struct weston_logind *wl, int vt);

int
weston_logind_connect(struct weston_logind **out,
		      struct weston_compositor *compositor,
		      const char *seat_id, int tty);

void
weston_logind_destroy(struct weston_logind *wl);

static inline int
weston_sd_session_get_vt(const char *sid, unsigned int *out)
{
#ifdef HAVE_SYSTEMD_LOGIN_209
	return sd_session_get_vt(sid, out);
#else
	int r;
	char *tty;

	r = sd_session_get_tty(sid, &tty);
	if (r < 0)
		return r;

	r = sscanf(tty, "tty%u", out);
	free(tty);

	if (r != 1)
		return -EINVAL;

	return 0;
#endif
}

#else /* defined(HAVE_SYSTEMD_LOGIN) && defined(HAVE_DBUS) */

static inline int
weston_logind_open(struct weston_logind *wl, const char *path,
		   int flags)
{
	return -ENOSYS;
}

static inline void
weston_logind_close(struct weston_logind *wl, int fd)
{
}

static inline void
weston_logind_restore(struct weston_logind *wl)
{
}

static inline int
weston_logind_activate_vt(struct weston_logind *wl, int vt)
{
	return -ENOSYS;
}

static inline int
weston_logind_connect(struct weston_logind **out,
		      struct weston_compositor *compositor,
		      const char *seat_id, int tty)
{
	return -ENOSYS;
}

static inline void
weston_logind_destroy(struct weston_logind *wl)
{
}

#endif /* defined(HAVE_SYSTEMD_LOGIN) && defined(HAVE_DBUS) */
