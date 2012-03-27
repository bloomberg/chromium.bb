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
	struct weston_process screenshooter_process;
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
	int32_t stride, i;

	if (!wl_buffer_is_shm(buffer))
		return;

	if (buffer->width < output->current->width ||
	    buffer->height < output->current->height)
		return;

	stride = wl_shm_buffer_get_stride(buffer);
	tmp = malloc(stride * buffer->height);
	if (tmp == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, output->current->width, output->current->height,
		     GL_BGRA_EXT, GL_UNSIGNED_BYTE, tmp);

	d = wl_shm_buffer_get_data(buffer);
	s = tmp + stride * (buffer->height - 1);

	for (i = 0; i < buffer->height; i++) {
		memcpy(d, s, stride);
		d += stride;
		s -= stride;
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
		wl_resource_destroy(resource, 0);
	}
}

static void
screenshooter_sigchld(struct weston_process *process, int status)
{
	struct screenshooter *shooter =
		container_of(process, struct screenshooter, screenshooter_process);

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
					&shooter->screenshooter_process,
					screenshooter_exe, screenshooter_sigchld);
}

struct screenshooter *
screenshooter_create(struct weston_compositor *ec)
{
	struct screenshooter *shooter;

	shooter = malloc(sizeof *shooter);
	if (shooter == NULL)
		return NULL;

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

	return shooter;
}

void
screenshooter_destroy(struct screenshooter *shooter)
{
	wl_display_remove_global(shooter->ec->wl_display, shooter->global);
	free(shooter);
}
