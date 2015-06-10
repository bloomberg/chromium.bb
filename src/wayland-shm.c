/*
 * Copyright © 2008 Kristian Høgsberg
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
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *    Benjamin Franzke <benjaminfranzke@googlemail.com>
 *
 */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>

#include "wayland-private.h"
#include "wayland-server.h"

/* This once_t is used to synchronize installing the SIGBUS handler
 * and creating the TLS key. This will be done in the first call
 * wl_shm_buffer_begin_access which can happen from any thread */
static pthread_once_t wl_shm_sigbus_once = PTHREAD_ONCE_INIT;
static pthread_key_t wl_shm_sigbus_data_key;
static struct sigaction wl_shm_old_sigbus_action;

struct wl_shm_pool {
	struct wl_resource *resource;
	int refcount;
	char *data;
	int32_t size;
};

struct wl_shm_buffer {
	struct wl_resource *resource;
	int32_t width, height;
	int32_t stride;
	uint32_t format;
	int offset;
	struct wl_shm_pool *pool;
};

struct wl_shm_sigbus_data {
	struct wl_shm_pool *current_pool;
	int access_count;
	int fallback_mapping_used;
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
	struct wl_shm_buffer *buffer = wl_resource_get_user_data(resource);

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

static bool
format_is_supported(struct wl_client *client, uint32_t format)
{
	struct wl_display *display = wl_client_get_display(client);
	struct wl_array *formats;
	uint32_t *p;

	switch (format) {
	case WL_SHM_FORMAT_ARGB8888:
	case WL_SHM_FORMAT_XRGB8888:
		return true;
	default:
		formats = wl_display_get_additional_shm_formats(display);
		wl_array_for_each(p, formats)
			if(*p == format)
				return true;
	}

	return false;
}

static void
shm_pool_create_buffer(struct wl_client *client, struct wl_resource *resource,
		       uint32_t id, int32_t offset,
		       int32_t width, int32_t height,
		       int32_t stride, uint32_t format)
{
	struct wl_shm_pool *pool = wl_resource_get_user_data(resource);
	struct wl_shm_buffer *buffer;

	if (!format_is_supported(client, format)) {
		wl_resource_post_error(resource,
				       WL_SHM_ERROR_INVALID_FORMAT,
				       "invalid format 0x%x", format);
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
		wl_client_post_no_memory(client);
		return;
	}

	buffer->width = width;
	buffer->height = height;
	buffer->format = format;
	buffer->stride = stride;
	buffer->offset = offset;
	buffer->pool = pool;
	pool->refcount++;

	buffer->resource =
		wl_resource_create(client, &wl_buffer_interface, 1, id);
	if (buffer->resource == NULL) {
		wl_client_post_no_memory(client);
		shm_pool_unref(pool);
		free(buffer);
		return;
	}

	wl_resource_set_implementation(buffer->resource,
				       &shm_buffer_interface,
				       buffer, destroy_buffer);
}

static void
destroy_pool(struct wl_resource *resource)
{
	struct wl_shm_pool *pool = wl_resource_get_user_data(resource);

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
	struct wl_shm_pool *pool = wl_resource_get_user_data(resource);
	void *data;

	if (size < pool->size) {
		wl_resource_post_error(resource,
				       WL_SHM_ERROR_INVALID_FD,
				       "shrinking pool invalid");
		return;
	}

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
		wl_client_post_no_memory(client);
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
		goto err_close;
	}
	close(fd);

	pool->resource =
		wl_resource_create(client, &wl_shm_pool_interface, 1, id);
	if (!pool->resource) {
		wl_client_post_no_memory(client);
		munmap(pool->data, pool->size);
		free(pool);
		return;
	}

	wl_resource_set_implementation(pool->resource,
				       &shm_pool_interface,
				       pool, destroy_pool);

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
	struct wl_display *display = wl_client_get_display(client);
	struct wl_array *additional_formats;
	uint32_t *p;

	resource = wl_resource_create(client, &wl_shm_interface, 1, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &shm_interface, data, NULL);

	wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
	wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);

	additional_formats = wl_display_get_additional_shm_formats(display);
	wl_array_for_each(p, additional_formats)
		wl_shm_send_format(resource, *p);
}

WL_EXPORT int
wl_display_init_shm(struct wl_display *display)
{
	if (!wl_global_create(display, &wl_shm_interface, 1, NULL, bind_shm))
		return -1;

	return 0;
}

WL_EXPORT struct wl_shm_buffer *
wl_shm_buffer_create(struct wl_client *client,
		     uint32_t id, int32_t width, int32_t height,
		     int32_t stride, uint32_t format)
{
	struct wl_shm_buffer *buffer;

	if (!format_is_supported(client, format))
		return NULL;

	buffer = malloc(sizeof *buffer + stride * height);
	if (buffer == NULL)
		return NULL;

	buffer->width = width;
	buffer->height = height;
	buffer->format = format;
	buffer->stride = stride;
	buffer->offset = 0;
	buffer->pool = NULL;

	buffer->resource =
		wl_resource_create(client, &wl_buffer_interface, 1, id);
	if (buffer->resource == NULL) {
		free(buffer);
		return NULL;
	}

	wl_resource_set_implementation(buffer->resource,
				       &shm_buffer_interface,
				       buffer, destroy_buffer);

	return buffer;
}

WL_EXPORT struct wl_shm_buffer *
wl_shm_buffer_get(struct wl_resource *resource)
{
	if (resource == NULL)
		return NULL;

	if (wl_resource_instance_of(resource, &wl_buffer_interface,
				    &shm_buffer_interface))
		return wl_resource_get_user_data(resource);
	else
		return NULL;
}

WL_EXPORT int32_t
wl_shm_buffer_get_stride(struct wl_shm_buffer *buffer)
{
	return buffer->stride;
}


/** Get a pointer to the memory for the SHM buffer
 *
 * \param buffer The buffer object
 *
 * Returns a pointer which can be used to read the data contained in
 * the given SHM buffer.
 *
 * As this buffer is memory-mapped, reading from it may generate
 * SIGBUS signals. This can happen if the client claims that the
 * buffer is larger than it is or if something truncates the
 * underlying file. To prevent this signal from causing the compositor
 * to crash you should call wl_shm_buffer_begin_access and
 * wl_shm_buffer_end_access around code that reads from the memory.
 *
 * \memberof wl_shm_buffer
 */
WL_EXPORT void *
wl_shm_buffer_get_data(struct wl_shm_buffer *buffer)
{
	if (buffer->pool)
		return buffer->pool->data + buffer->offset;
	else
		return buffer + 1;
}

WL_EXPORT uint32_t
wl_shm_buffer_get_format(struct wl_shm_buffer *buffer)
{
	return buffer->format;
}

WL_EXPORT int32_t
wl_shm_buffer_get_width(struct wl_shm_buffer *buffer)
{
	return buffer->width;
}

WL_EXPORT int32_t
wl_shm_buffer_get_height(struct wl_shm_buffer *buffer)
{
	return buffer->height;
}

static void
reraise_sigbus(void)
{
	/* If SIGBUS is raised for some other reason than accessing
	 * the pool then we'll uninstall the signal handler so we can
	 * reraise it. This would presumably kill the process */
	sigaction(SIGBUS, &wl_shm_old_sigbus_action, NULL);
	raise(SIGBUS);
}

static void
sigbus_handler(int signum, siginfo_t *info, void *context)
{
	struct wl_shm_sigbus_data *sigbus_data =
		pthread_getspecific(wl_shm_sigbus_data_key);
	struct wl_shm_pool *pool;

	if (sigbus_data == NULL) {
		reraise_sigbus();
		return;
	}

	pool = sigbus_data->current_pool;

	/* If the offending address is outside the mapped space for
	 * the pool then the error is a real problem so we'll reraise
	 * the signal */
	if (pool == NULL ||
	    (char *) info->si_addr < pool->data ||
	    (char *) info->si_addr >= pool->data + pool->size) {
		reraise_sigbus();
		return;
	}

	sigbus_data->fallback_mapping_used = 1;

	/* This should replace the previous mapping */
	if (mmap(pool->data, pool->size,
		 PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
		 0, 0) == (void *) -1) {
		reraise_sigbus();
		return;
	}
}

static void
destroy_sigbus_data(void *data)
{
	struct wl_shm_sigbus_data *sigbus_data = data;

	free(sigbus_data);
}

static void
init_sigbus_data_key(void)
{
	struct sigaction new_action = {
		.sa_sigaction = sigbus_handler,
		.sa_flags = SA_SIGINFO | SA_NODEFER
	};

	sigemptyset(&new_action.sa_mask);

	sigaction(SIGBUS, &new_action, &wl_shm_old_sigbus_action);

	pthread_key_create(&wl_shm_sigbus_data_key, destroy_sigbus_data);
}

/** Mark that the given SHM buffer is about to be accessed
 *
 * \param buffer The SHM buffer
 *
 * An SHM buffer is a memory-mapped file given by the client.
 * According to POSIX, reading from a memory-mapped region that
 * extends off the end of the file will cause a SIGBUS signal to be
 * generated. Normally this would cause the compositor to terminate.
 * In order to make the compositor robust against clients that change
 * the size of the underlying file or lie about its size, you should
 * protect access to the buffer by calling this function before
 * reading from the memory and call wl_shm_buffer_end_access
 * afterwards. This will install a signal handler for SIGBUS which
 * will prevent the compositor from crashing.
 *
 * After calling this function the signal handler will remain
 * installed for the lifetime of the compositor process. Note that
 * this function will not work properly if the compositor is also
 * installing its own handler for SIGBUS.
 *
 * If a SIGBUS signal is received for an address within the range of
 * the SHM pool of the given buffer then the client will be sent an
 * error event when wl_shm_buffer_end_access is called. If the signal
 * is for an address outside that range then the signal handler will
 * reraise the signal which would will likely cause the compositor to
 * terminate.
 *
 * It is safe to nest calls to these functions as long as the nested
 * calls are all accessing the same buffer. The number of calls to
 * wl_shm_buffer_end_access must match the number of calls to
 * wl_shm_buffer_begin_access. These functions are thread-safe and it
 * is allowed to simultaneously access different buffers or the same
 * buffer from multiple threads.
 *
 * \memberof wl_shm_buffer
 */
WL_EXPORT void
wl_shm_buffer_begin_access(struct wl_shm_buffer *buffer)
{
	struct wl_shm_pool *pool = buffer->pool;
	struct wl_shm_sigbus_data *sigbus_data;

	pthread_once(&wl_shm_sigbus_once, init_sigbus_data_key);

	sigbus_data = pthread_getspecific(wl_shm_sigbus_data_key);
	if (sigbus_data == NULL) {
		sigbus_data = malloc(sizeof *sigbus_data);
		if (sigbus_data == NULL)
			return;

		memset(sigbus_data, 0, sizeof *sigbus_data);

		pthread_setspecific(wl_shm_sigbus_data_key, sigbus_data);
	}

	assert(sigbus_data->current_pool == NULL ||
	       sigbus_data->current_pool == pool);

	sigbus_data->current_pool = pool;
	sigbus_data->access_count++;
}

/** Ends the access to a buffer started by wl_shm_buffer_begin_access
 *
 * \param buffer The SHM buffer
 *
 * This should be called after wl_shm_buffer_begin_access once the
 * buffer is no longer being accessed. If a SIGBUS signal was
 * generated in-between these two calls then the resource for the
 * given buffer will be sent an error.
 *
 * \memberof wl_shm_buffer
 */
WL_EXPORT void
wl_shm_buffer_end_access(struct wl_shm_buffer *buffer)
{
	struct wl_shm_sigbus_data *sigbus_data =
		pthread_getspecific(wl_shm_sigbus_data_key);

	assert(sigbus_data && sigbus_data->access_count >= 1);

	if (--sigbus_data->access_count == 0) {
		if (sigbus_data->fallback_mapping_used) {
			wl_resource_post_error(buffer->resource,
					       WL_SHM_ERROR_INVALID_FD,
					       "error accessing SHM buffer");
			sigbus_data->fallback_mapping_used = 0;
		}

		sigbus_data->current_pool = NULL;
	}
}
