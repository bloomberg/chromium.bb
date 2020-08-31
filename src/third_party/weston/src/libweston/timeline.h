/*
 * Copyright © 2014 Pekka Paalanen <pq@iki.fi>
 * Copyright © 2014 Collabora, Ltd.
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
	TLT_GPU,
};

#define TYPEVERIFY(type, arg) ({			\
	typeof(arg) tmp___ = (arg);		\
	(void)((type)0 == tmp___);		\
	tmp___; })

#define TLP_END TLT_END, NULL
#define TLP_OUTPUT(o) TLT_OUTPUT, TYPEVERIFY(struct weston_output *, (o))
#define TLP_SURFACE(s) TLT_SURFACE, TYPEVERIFY(struct weston_surface *, (s))
#define TLP_VBLANK(t) TLT_VBLANK, TYPEVERIFY(const struct timespec *, (t))
#define TLP_GPU(t) TLT_GPU, TYPEVERIFY(const struct timespec *, (t))

#define TL_POINT(...) do { \
	if (weston_timeline_enabled_) \
		weston_timeline_point(__VA_ARGS__); \
} while (0)

void
weston_timeline_point(const char *name, ...);

#endif /* WESTON_TIMELINE_H */
