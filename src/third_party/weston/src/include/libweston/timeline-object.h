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

#ifndef WESTON_TIMELINE_OBJECT_H
#define WESTON_TIMELINE_OBJECT_H

/*
 * This struct can be embedded in objects related to timeline output.
 * It must be initialized to all-zero. Afterwards, the timeline code
 * will handle it alone. No clean-up is necessary.
 */
struct weston_timeline_object {
	/*
	 * Timeline series gets bumped every time a new log is opened.
	 * This triggers id allocation and object info emission.
	 * 0 is an invalid series value.
	 */
	unsigned series;

	/* Object id in the timeline JSON output. 0 is invalid. */
	unsigned id;

	/*
	 * If non-zero, forces a re-emission of object description.
	 * Should be set to non-zero, when changing long-lived
	 * object state that is not emitted on normal timeline
	 * events.
	 */
	unsigned force_refresh;
};

#endif /* WESTON_TIMELINE_OBJECT_H */
