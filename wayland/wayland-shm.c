/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *    Benjamin Franzke <benjaminfranzke@googlemail.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "wayland-server.h"

struct wl_shm {
	struct wl_object object;
	const struct wl_shm_callbacks *callbacks;
};

struct wl_shm_buffer {
	struct wl_buffer buffer;
	struct wl_shm *shm;
	int32_t stride;
	void *data;
};

static void
destroy_buffer(struct wl_resource *resource, struct wl_client *client)
{
	struct wl_shm_buffer *buffer =
		container_of(resource, struct wl_shm_buffer, buffer.resource);

	munmap(buffer->data, buffer->stride * buffer->buffer.height);

	buffer->shm->callbacks->buffer_destroyed(&buffer->buffer);

	free(buffer);
}

static void
shm_buffer_damage(struct wl_client *client, struct wl_buffer *buffer_base,
		  int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wl_shm_buffer *buffer = (struct wl_shm_buffer *) buffer_base;

	buffer->shm->callbacks->buffer_damaged(buffer_base, x, y,
					       width, height);
}

static void
shm_buffer_destroy(struct wl_client *client, struct wl_buffer *buffer)
{
	wl_resource_destroy(&buffer->resource, client, 0);
}

const static struct wl_buffer_interface shm_buffer_interface = {
	shm_buffer_damage,
	shm_buffer_destroy
};

static struct wl_shm_buffer *
wl_shm_buffer_init(struct wl_shm *shm, uint32_t id,
		   int32_t width, int32_t height,
		   int32_t stride, struct wl_visual *visual,
		   void *data)
{
	struct wl_shm_buffer *buffer;

	buffer = malloc(sizeof *buffer);
	if (buffer == NULL)
		return NULL;

	buffer->buffer.width = width;
	buffer->buffer.height = height;
	buffer->buffer.visual = visual;
	buffer->stride = stride;
	buffer->data = data;

	buffer->buffer.resource.object.id = id;
	buffer->buffer.resource.object.interface = &wl_buffer_interface;
	buffer->buffer.resource.object.implementation = (void (**)(void))
		&shm_buffer_interface;

	buffer->buffer.resource.destroy = destroy_buffer;

	buffer->shm = shm;
	
	buffer->shm->callbacks->buffer_created(&buffer->buffer);

	return buffer;
}

static void
shm_create_buffer(struct wl_client *client, struct wl_shm *shm,
		  uint32_t id, int fd, int32_t width, int32_t height,
		  uint32_t stride, struct wl_visual *visual)
{
	struct wl_shm_buffer *buffer;
	void *data;

	if (visual->object.interface != &wl_visual_interface) {
		wl_client_post_error(client, &shm->object,
				     WL_SHM_ERROR_INVALID_VISUAL,
				     "invalid visual");
		close(fd);
		return;
	}

	if (width < 0 || height < 0 || stride < width) {
		wl_client_post_error(client, &shm->object,
				     WL_SHM_ERROR_INVALID_STRIDE,
				     "invalid width, height or stride (%dx%d, %u)",
				     width, height, stride);
		close(fd);
		return;
	}

	data = mmap(NULL, stride * height,
		    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	close(fd);
	if (data == MAP_FAILED) {
		wl_client_post_error(client, &shm->object,
				     WL_SHM_ERROR_INVALID_FD,
				     "failed mmap fd %d", fd);
		return;
	}

	buffer = wl_shm_buffer_init(shm, id,
				    width, height, stride, visual,
				    data);
	if (buffer == NULL) {
		munmap(data, stride * height);
		wl_client_post_no_memory(client);
		return;
	}

	wl_client_add_resource(client, &buffer->buffer.resource);
}

const static struct wl_shm_interface shm_interface = {
	shm_create_buffer
};


WL_EXPORT struct wl_shm *
wl_shm_init(struct wl_display *display,
	    const struct wl_shm_callbacks *callbacks)
{
	struct wl_shm *shm;

	shm = malloc(sizeof *shm);
	if (!shm)
		return NULL;

	shm->object.interface = &wl_shm_interface;
	shm->object.implementation = (void (**)(void)) &shm_interface;
	wl_display_add_object(display, &shm->object);
	wl_display_add_global(display, &shm->object, NULL);

	shm->callbacks = callbacks;

	return shm;
}

WL_EXPORT void
wl_shm_finish(struct wl_shm *shm)
{
	/* FIXME: add wl_display_del_{object,global} */

	free(shm);
}

WL_EXPORT int
wl_buffer_is_shm(struct wl_buffer *buffer)
{
	return buffer->resource.object.implementation == 
		(void (**)(void)) &shm_buffer_interface;
}

WL_EXPORT int32_t
wl_shm_buffer_get_stride(struct wl_buffer *buffer_base)
{
	struct wl_shm_buffer *buffer = (struct wl_shm_buffer *) buffer_base;

	if (!wl_buffer_is_shm(buffer_base))
		return 0;

	return buffer->stride;
}

WL_EXPORT void *
wl_shm_buffer_get_data(struct wl_buffer *buffer_base)
{
	struct wl_shm_buffer *buffer = (struct wl_shm_buffer *) buffer_base;

	if (!wl_buffer_is_shm(buffer_base))
		return NULL;

	return buffer->data;
}
