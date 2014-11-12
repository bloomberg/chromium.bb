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
