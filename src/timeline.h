/*
 * Copyright © 2014 Pekka Paalanen <pq@iki.fi>
 * Copyright © 2014 Collabora, Ltd.
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

#ifndef WESTON_TIMELINE_H
#define WESTON_TIMELINE_H

extern int weston_timeline_enabled_;

struct weston_compositor;

void
weston_timeline_open(struct weston_compositor *compositor);

void
weston_timeline_close(void);

enum timeline_type {
	TLT_END = 0,
	TLT_OUTPUT,
	TLT_SURFACE,
	TLT_VBLANK,
};

#define TYPEVERIFY(type, arg) ({			\
	typeof(arg) tmp___ = (arg);		\
	(void)((type)0 == tmp___);		\
	tmp___; })

#define TLP_END TLT_END, NULL
#define TLP_OUTPUT(o) TLT_OUTPUT, TYPEVERIFY(struct weston_output *, (o))
#define TLP_SURFACE(s) TLT_SURFACE, TYPEVERIFY(struct weston_surface *, (s))
#define TLP_VBLANK(t) TLT_VBLANK, TYPEVERIFY(const struct timespec *, (t))

#define TL_POINT(...) do { \
	if (weston_timeline_enabled_) \
		weston_timeline_point(__VA_ARGS__); \
} while (0)

void
weston_timeline_point(const char *name, ...);

#endif /* WESTON_TIMELINE_H */
