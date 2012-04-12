/*
 * Copyright © 2008-2011 Kristian Høgsberg
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

#include <stdlib.h>
#include <string.h>
#include <linux/input.h>

#include "compositor.h"
#include "screenshooter-server-protocol.h"

struct screenshooter {
	struct wl_object base;
	struct weston_compositor *ec;
	struct wl_global *global;
	struct wl_client *client;
	struct weston_process process;
	struct wl_listener destroy_listener;
};

static void
screenshooter_shoot(struct wl_client *client,
		    struct wl_resource *resource,
		    struct wl_resource *output_resource,
		    struct wl_resource *buffer_resource)
{
	struct weston_output *output = output_resource->data;
	struct wl_buffer *buffer = buffer_resource->data;
	uint8_t *tmp, *d, *s;
	int32_t buffer_stride, output_stride, i;

	if (!wl_buffer_is_shm(buffer))
		return;

	if (buffer->width < output->current->width ||
	    buffer->height < output->current->height)
		return;

	buffer_stride = wl_shm_buffer_get_stride(buffer);
	output_stride = output->current->width * 4;
	tmp = malloc(output_stride * output->current->height);
	if (tmp == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	output->read_pixels(output, tmp);

	d = wl_shm_buffer_get_data(buffer) + output->y * buffer_stride +
							output->x * 4;
	s = tmp + output_stride * (output->current->height - 1);

	for (i = 0; i < output->current->height; i++) {
		memcpy(d, s, output_stride);
		d += buffer_stride;
		s -= output_stride;
	}

	free(tmp);
}

struct screenshooter_interface screenshooter_implementation = {
	screenshooter_shoot
};

static void
bind_shooter(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id)
{
	struct screenshooter *shooter = data;
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &screenshooter_interface,
			     &screenshooter_implementation, id, data);

	if (client != shooter->client) {
		wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "screenshooter failed: permission denied");
		wl_resource_destroy(resource);
	}
}

static void
screenshooter_sigchld(struct weston_process *process, int status)
{
	struct screenshooter *shooter =
		container_of(process, struct screenshooter, process);

	shooter->client = NULL;
}

static void
screenshooter_binding(struct wl_input_device *device, uint32_t time,
		 uint32_t key, uint32_t button, uint32_t axis,
		 int32_t state, void *data)
{
	struct screenshooter *shooter = data;
	const char *screenshooter_exe = LIBEXECDIR "/weston-screenshooter";

	if (!shooter->client)
		shooter->client = weston_client_launch(shooter->ec,
					&shooter->process,
					screenshooter_exe, screenshooter_sigchld);
}

static void
screenshooter_destroy(struct wl_listener *listener, void *data)
{
	struct screenshooter *shooter =
		container_of(listener, struct screenshooter, destroy_listener);

	wl_display_remove_global(shooter->ec->wl_display, shooter->global);
	free(shooter);
}

void
screenshooter_create(struct weston_compositor *ec)
{
	struct screenshooter *shooter;

	shooter = malloc(sizeof *shooter);
	if (shooter == NULL)
		return;

	shooter->base.interface = &screenshooter_interface;
	shooter->base.implementation =
		(void(**)(void)) &screenshooter_implementation;
	shooter->ec = ec;
	shooter->client = NULL;

	shooter->global = wl_display_add_global(ec->wl_display,
						&screenshooter_interface,
						shooter, bind_shooter);
	weston_compositor_add_binding(ec, KEY_S, 0, 0, MODIFIER_SUPER,
					screenshooter_binding, shooter);

	shooter->destroy_listener.notify = screenshooter_destroy;
	wl_signal_add(&ec->destroy_signal, &shooter->destroy_listener);
}
