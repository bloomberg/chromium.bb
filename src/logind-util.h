/*
 * Copyright © 2013 David Herrmann
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
		      const char *seat_id, int tty, bool sync_drm);

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
		      const char *seat_id, int tty, bool sync_drm)
{
	return -ENOSYS;
}

static inline void
weston_logind_destroy(struct weston_logind *wl)
{
}

#endif /* defined(HAVE_SYSTEMD_LOGIN) && defined(HAVE_DBUS) */
