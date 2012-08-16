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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "wayland-server.h"

struct wl_shm_pool {
	struct wl_resource resource;
	int refcount;
	char *data;
	int size;
};

struct wl_shm_buffer {
	struct wl_buffer buffer;
	int32_t stride;
	uint32_t format;
	int offset;
	struct wl_shm_pool *pool;
};

static void
shm_pool_unref(struct wl_shm_pool *pool)
{
	pool->refcount--;
	if (pool->refcount)
		return;

	munmap(pool->data, pool->size);
	free(pool);
}

static void
destroy_buffer(struct wl_resource *resource)
{
	struct wl_shm_buffer *buffer =
		container_of(resource, struct wl_shm_buffer, buffer.resource);

	if (buffer->pool)
		shm_pool_unref(buffer->pool);
	free(buffer);
}

static void
shm_buffer_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_buffer_interface shm_buffer_interface = {
	shm_buffer_destroy
};

static void
shm_pool_create_buffer(struct wl_client *client, struct wl_resource *resource,
		       uint32_t id, int32_t offset,
		       int32_t width, int32_t height,
		       int32_t stride, uint32_t format)
{
	struct wl_shm_pool *pool = resource->data;
	struct wl_shm_buffer *buffer;

	switch (format) {
	case WL_SHM_FORMAT_ARGB8888:
	case WL_SHM_FORMAT_XRGB8888:
		break;
	default:
		wl_resource_post_error(resource,
				       WL_SHM_ERROR_INVALID_FORMAT,
				       "invalid format");
		return;
	}

	if (offset < 0 || width <= 0 || height <= 0 || stride < width ||
	    INT32_MAX / stride <= height ||
	    offset > pool->size - stride * height) {
		wl_resource_post_error(resource,
				       WL_SHM_ERROR_INVALID_STRIDE,
				       "invalid width, height or stride (%dx%d, %u)",
				       width, height, stride);
		return;
	}

	buffer = malloc(sizeof *buffer);
	if (buffer == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	buffer->buffer.width = width;
	buffer->buffer.height = height;
	buffer->buffer.busy_count = 0;
	buffer->format = format;
	buffer->stride = stride;
	buffer->offset = offset;
	buffer->pool = pool;
	pool->refcount++;

	buffer->buffer.resource.object.id = id;
	buffer->buffer.resource.object.interface = &wl_buffer_interface;
	buffer->buffer.resource.object.implementation = (void (**)(void))
		&shm_buffer_interface;

	buffer->buffer.resource.data = buffer;
	buffer->buffer.resource.client = resource->client;
	buffer->buffer.resource.destroy = destroy_buffer;

	wl_client_add_resource(client, &buffer->buffer.resource);
}

static void
destroy_pool(struct wl_resource *resource)
{
	struct wl_shm_pool *pool = resource->data;

	shm_pool_unref(pool);
}

static void
shm_pool_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
shm_pool_resize(struct wl_client *client, struct wl_resource *resource,
		int32_t size)
{
	struct wl_shm_pool *pool = resource->data;
	void *data;

	data = mremap(pool->data, pool->size, size, MREMAP_MAYMOVE);

	if (data == MAP_FAILED) {
		wl_resource_post_error(resource,
				       WL_SHM_ERROR_INVALID_FD,
				       "failed mremap");
		return;
	}

	pool->data = data;
	pool->size = size;
}

struct wl_shm_pool_interface shm_pool_interface = {
	shm_pool_create_buffer,
	shm_pool_destroy,
	shm_pool_resize
};

static void
shm_create_pool(struct wl_client *client, struct wl_resource *resource,
		uint32_t id, int fd, int32_t size)
{
	struct wl_shm_pool *pool;

	pool = malloc(sizeof *pool);
	if (pool == NULL) {
		wl_resource_post_no_memory(resource);
		goto err_close;
	}

	if (size <= 0) {
		wl_resource_post_error(resource,
				       WL_SHM_ERROR_INVALID_STRIDE,
				       "invalid size (%d)", size);
		goto err_free;
	}

	pool->refcount = 1;
	pool->size = size;
	pool->data = mmap(NULL, size,
			  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (pool->data == MAP_FAILED) {
		wl_resource_post_error(resource,
				       WL_SHM_ERROR_INVALID_FD,
				       "failed mmap fd %d", fd);
		goto err_free;
	}
	close(fd);

	pool->resource.object.id = id;
	pool->resource.object.interface = &wl_shm_pool_interface;
	pool->resource.object.implementation =
		(void (**)(void)) &shm_pool_interface;

	pool->resource.data = pool;
	pool->resource.client = client;
	pool->resource.destroy = destroy_pool;

	wl_client_add_resource(client, &pool->resource);

	return;

err_close:
	close(fd);
err_free:
	free(pool);
}

static const struct wl_shm_interface shm_interface = {
	shm_create_pool
};

static void
bind_shm(struct wl_client *client,
	 void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &wl_shm_interface,
					&shm_interface, id, data);

	wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
	wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);
}

WL_EXPORT int
wl_display_init_shm(struct wl_display *display)
{
	if (!wl_display_add_global(display, &wl_shm_interface, NULL, bind_shm))
		return -1;

	return 0;
}

WL_EXPORT struct wl_buffer *
wl_shm_buffer_create(struct wl_client *client,
		     uint32_t id, int32_t width, int32_t height,
		     int32_t stride, uint32_t format)
{
	struct wl_shm_buffer *buffer;
			     
	switch (format) {
	case WL_SHM_FORMAT_ARGB8888:
	case WL_SHM_FORMAT_XRGB8888:
		break;
	default:
		return NULL;
	}

	buffer = malloc(sizeof *buffer + stride * height);
	if (buffer == NULL)
		return NULL;

	buffer->buffer.width = width;
	buffer->buffer.height = height;
	buffer->buffer.busy_count = 0;
	buffer->format = format;
	buffer->stride = stride;
	buffer->offset = 0;
	buffer->pool = NULL;

	buffer->buffer.resource.object.id = id;
	buffer->buffer.resource.object.interface = &wl_buffer_interface;
	buffer->buffer.resource.object.implementation = (void (**)(void))
		&shm_buffer_interface;

	buffer->buffer.resource.data = buffer;
	buffer->buffer.resource.client = client;
	buffer->buffer.resource.destroy = destroy_buffer;

	wl_client_add_resource(client, &buffer->buffer.resource);

	return &buffer->buffer;
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

	if (buffer->pool)
		return buffer->pool->data + buffer->offset;
	else
		return buffer + 1;
}

WL_EXPORT uint32_t
wl_shm_buffer_get_format(struct wl_buffer *buffer_base)
{
	struct wl_shm_buffer *buffer = (struct wl_shm_buffer *) buffer_base;

	return buffer->format;
}

WL_EXPORT int32_t
wl_shm_buffer_get_width(struct wl_buffer *buffer_base)
{
	struct wl_shm_buffer *buffer = (struct wl_shm_buffer *) buffer_base;

	return buffer->buffer.width;
}

WL_EXPORT int32_t
wl_shm_buffer_get_height(struct wl_buffer *buffer_base)
{
	struct wl_shm_buffer *buffer = (struct wl_shm_buffer *) buffer_base;

	return buffer->buffer.height;
}
