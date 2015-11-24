/*
 * Copyright © 2014, 2015 Collabora, Ltd.
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

#ifndef WESTON_LINUX_DMABUF_H
#define WESTON_LINUX_DMABUF_H

#include <stdint.h>

#define MAX_DMABUF_PLANES 4

struct linux_dmabuf_buffer;
typedef void (*dmabuf_user_data_destroy_func)(
			struct linux_dmabuf_buffer *buffer);

struct dmabuf_attributes {
	int32_t width;
	int32_t height;
	uint32_t format;
	uint32_t flags; /* enum zlinux_buffer_params_flags */
	int n_planes;
	int fd[MAX_DMABUF_PLANES];
	uint32_t offset[MAX_DMABUF_PLANES];
	uint32_t stride[MAX_DMABUF_PLANES];
	uint64_t modifier[MAX_DMABUF_PLANES];
};

struct linux_dmabuf_buffer {
	struct wl_resource *buffer_resource;
	struct wl_resource *params_resource;
	struct weston_compositor *compositor;
	struct dmabuf_attributes attributes;

	void *user_data;
	dmabuf_user_data_destroy_func user_data_destroy_func;

	/* XXX:
	 *
	 * Add backend private data. This would be for the backend
	 * to do all additional imports it might ever use in advance.
	 * The basic principle, even if not implemented in drivers today,
	 * is that dmabufs are first attached, but the actual allocation
	 * is deferred to first use. This would allow the exporter and all
	 * attachers to agree on how to allocate.
	 *
	 * The DRM backend would use this to create drmFBs for each
	 * dmabuf_buffer, just in case at some point it would become
	 * feasible to scan it out directly. This would improve the
	 * possibilities to successfully scan out, avoiding compositing.
	 */
};

int
linux_dmabuf_setup(struct weston_compositor *compositor);

struct linux_dmabuf_buffer *
linux_dmabuf_buffer_get(struct wl_resource *resource);

void
linux_dmabuf_buffer_set_user_data(struct linux_dmabuf_buffer *buffer,
				  void *data,
				  dmabuf_user_data_destroy_func func);
void *
linux_dmabuf_buffer_get_user_data(struct linux_dmabuf_buffer *buffer);

void
linux_dmabuf_buffer_send_server_error(struct linux_dmabuf_buffer *buffer,
				      const char *msg);

#endif /* WESTON_LINUX_DMABUF_H */
