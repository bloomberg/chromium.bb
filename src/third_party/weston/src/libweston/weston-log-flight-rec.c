/*
 * Copyright © 2019 Collabora Ltd.
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

#include <libweston/weston-log.h>
#include "helpers.h"
#include <libweston/libweston.h>

#include "weston-log-internal.h"

#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

struct weston_ring_buffer {
	uint32_t append_pos;	/**< where in the buffer we are */
	uint32_t size;		/**< max length of the ring buffer */
	char *buf;		/**< the buffer itself */
	FILE *file;		/**< where to write in case we need to dump the buf */
	bool overlap;		/**< in case buff overlaps, hint from where to print buf contents */
};

/** A black box type of stream, used to aggregate data continuously, and
 * when needed, to dump its contents for inspection.
 */
struct weston_debug_log_flight_recorder {
	struct weston_log_subscriber base;
	struct weston_ring_buffer rb;
};

static void
weston_ring_buffer_init(struct weston_ring_buffer *rb, size_t size, char *buf)
{
	rb->append_pos = 0;
	rb->size = size - 1;
	rb->buf = buf;
	rb->overlap = false;
	rb->file = stderr;
}

static struct weston_debug_log_flight_recorder *
to_flight_recorder(struct weston_log_subscriber *sub)
{
	return container_of(sub, struct weston_debug_log_flight_recorder, base);
}

static void
weston_log_flight_recorder_adjust_end(struct weston_ring_buffer *rb,
				      size_t bytes_to_advance)
{
	if (rb->append_pos == rb->size - bytes_to_advance)
		rb->append_pos = 0;
	else
		rb->append_pos += bytes_to_advance;
}

static void
weston_log_flight_recorder_write_chunks(struct weston_ring_buffer *rb,
					const char *data, size_t len)
{
	/* no of chunks that matches our buffer size */
	size_t nr_chunks = len / rb->size;

	/* bytes left that do not fill entire buffer */
	size_t bytes_left_last_chunk = len % rb->size;
	const char *c_data = data;

	/* might overlap multiple times, memcpy is redundant,
	 * that's why we don't even modify append_pos */
	while (nr_chunks-- > 0) {
		memcpy(&rb->buf[rb->append_pos], c_data, rb->size);
		c_data += rb->size;
	}

	if (bytes_left_last_chunk)
		memcpy(&rb->buf[rb->append_pos], c_data, bytes_left_last_chunk);

	/* adjust append_pos */
	weston_log_flight_recorder_adjust_end(rb, bytes_left_last_chunk);
}

static void
weston_log_flight_recorder_write_chunks_overlap(struct weston_ring_buffer *rb,
					 const char *data, size_t len)
{
	size_t transfer_remains =
		(rb->append_pos + len) - rb->size;
	size_t transfer_to_end = len - transfer_remains;
	const char *c_data = data;

	/* transfer what remains until the end of the buffer */
	memcpy(&rb->buf[rb->append_pos], c_data, transfer_to_end);
	c_data += transfer_to_end;

	/* reset append_pos as we filled up the buffer */
	rb->append_pos = 0;

	/* transfer what remains */
	weston_log_flight_recorder_write_chunks(rb, c_data, transfer_remains);
	rb->overlap = true;
}

static void
weston_log_flight_recorder_write_data(struct weston_ring_buffer *rb,
				      const char *data, size_t len)
{
	/*
	 * If append_pos is at the beginning of the buffer, we determine if we
	 * should do it in chunks, and if there are any bytes left we transfer
	 * those as well.
	 *
	 * If the append_pos is somewhere inside the buffer we determine how
	 * many bytes we need to transfer between we reach the end and overlap,
	 * then we proceed as in the first step.
	 */
	if (rb->append_pos == 0)
		weston_log_flight_recorder_write_chunks(rb, data, len);
	else
		weston_log_flight_recorder_write_chunks_overlap(rb, data, len);
}

static void
weston_log_flight_recorder_write(struct weston_log_subscriber *sub,
				 const char *data, size_t len)
{
	struct weston_debug_log_flight_recorder *flight_rec =
		to_flight_recorder(sub);
	struct weston_ring_buffer *rb = &flight_rec->rb;

	/* in case the data is bigger than the size of the buf */
	if (rb->size < len) {
		weston_log_flight_recorder_write_data(rb, data, len);
	} else {
		/* if we can transfer it without wrapping it do it at once */
		if (rb->append_pos <= rb->size - len) {
			memcpy(&rb->buf[rb->append_pos], data, len);

			/*
			 * adjust append_pos, take care of the situation were
			 * to fill up the entire buf
			 */
			weston_log_flight_recorder_adjust_end(rb, len);
		} else {
			weston_log_flight_recorder_write_data(rb, data, len);
		}
	}

}

static void
weston_log_flight_recorder_map_memory(struct weston_debug_log_flight_recorder *flight_rec)
{
	size_t i = 0;

	for (i = 0; i < flight_rec->rb.size; i++)
		flight_rec->rb.buf[i] = 0xff;
}

WL_EXPORT void
weston_log_subscriber_display_flight_rec(struct weston_log_subscriber *sub)
{
	struct weston_debug_log_flight_recorder *flight_rec =
		to_flight_recorder(sub);
	struct weston_ring_buffer *rb = &flight_rec->rb;

	if (rb->append_pos <= rb->size && !rb->overlap) {
		if (rb->append_pos)
			fwrite(rb->buf, sizeof(char), rb->append_pos, rb->file);
		else
			fwrite(rb->buf, sizeof(char), rb->size, rb->file);
	} else {
		/* from append_pos to size */
		fwrite(&rb->buf[rb->append_pos], sizeof(char),
		       rb->size - rb->append_pos, rb->file);
		/* from 0 to append_pos */
		fwrite(rb->buf, sizeof(char), rb->append_pos, rb->file);
	}
}

/** Create a flight recorder type of subscriber
 *
 * Allocates both the flight recorder and the underlying ring buffer. Use
 * weston_log_subscriber_destroy_flight_rec() to clean-up.
 *
 * @param size specify the maximum size (in bytes) of the backing storage
 * for the flight recorder
 * @returns a weston_log_subscriber object or NULL in case of failure
 */
WL_EXPORT struct weston_log_subscriber *
weston_log_subscriber_create_flight_rec(size_t size)
{
	struct weston_debug_log_flight_recorder *flight_rec;
	char *weston_rb;

	flight_rec = zalloc(sizeof(*flight_rec));
	if (!flight_rec)
		return NULL;

	flight_rec->base.write = weston_log_flight_recorder_write;
	flight_rec->base.destroy = NULL;
	flight_rec->base.complete = NULL;
	wl_list_init(&flight_rec->base.subscription_list);

	weston_rb = zalloc(sizeof(char) * size);
	if (!weston_rb) {
		free(flight_rec);
		return NULL;
	}

	weston_ring_buffer_init(&flight_rec->rb, size, weston_rb);

	/* write some data to the rb such that the memory gets mapped */
	weston_log_flight_recorder_map_memory(flight_rec);

	return &flight_rec->base;
}

/** Destroys the weston_log_subscriber object created with
 * weston_log_subscriber_create_flight_rec()
 *
 * @param sub the weston_log_subscriber object
 *
 */
WL_EXPORT void
weston_log_subscriber_destroy_flight_rec(struct weston_log_subscriber *sub)
{
	struct weston_debug_log_flight_recorder *flight_rec = to_flight_recorder(sub);
	free(flight_rec->rb.buf);
	free(flight_rec);
}
