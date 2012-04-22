/*
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2011-2012 Collabora, Ltd.
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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <assert.h>
#include <signal.h>
#include <math.h>

#include <wayland-server.h>
#include "compositor.h"
#include "desktop-shell-server-protocol.h"
#include "../shared/config-parser.h"

struct desktop_shell {
	struct weston_compositor *compositor;

	struct wl_listener lock_listener;
	struct wl_listener unlock_listener;
	struct wl_listener destroy_listener;

	struct weston_layer fullscreen_layer;
	struct weston_layer panel_layer;
	struct weston_layer toplevel_layer;
	struct weston_layer background_layer;
	struct weston_layer lock_layer;

	struct {
		struct weston_process process;
		struct wl_client *client;
		struct wl_resource *desktop_shell;

		unsigned deathcount;
		uint32_t deathstamp;
	} child;

	bool locked;
	bool prepare_event_sent;

	struct shell_surface *lock_surface;
	struct wl_listener lock_surface_listener;

	struct wl_list backgrounds;
	struct wl_list panels;

	struct {
		char *path;
		int duration;
		struct wl_resource *binding;
		struct wl_list surfaces;
		struct weston_process process;
	} screensaver;

	uint32_t binding_modifier;
	struct weston_surface *debug_repaint_surface;
};

enum shell_surface_type {
	SHELL_SURFACE_NONE,

	SHELL_SURFACE_PANEL,
	SHELL_SURFACE_BACKGROUND,
	SHELL_SURFACE_LOCK,
	SHELL_SURFACE_SCREENSAVER,

	SHELL_SURFACE_TOPLEVEL,
	SHELL_SURFACE_TRANSIENT,
	SHELL_SURFACE_FULLSCREEN,
	SHELL_SURFACE_MAXIMIZED,
	SHELL_SURFACE_POPUP
};

struct ping_timer {
	struct wl_event_source *source;
	uint32_t serial;
};

struct shell_surface {
	struct wl_resource resource;

	struct weston_surface *surface;
	struct wl_listener surface_destroy_listener;
	struct shell_surface *parent;
	struct desktop_shell *shell;

	enum shell_surface_type type;
	int32_t saved_x, saved_y;
	bool saved_position_valid;
	int unresponsive;

	struct {
		struct weston_transform transform;
		struct weston_matrix rotation;
	} rotation;

	struct {
		struct wl_pointer_grab grab;
		int32_t x, y;
		struct weston_transform parent_transform;
		int32_t initial_up;
		struct wl_input_device *device;
		uint32_t serial;
	} popup;

	struct {
		enum wl_shell_surface_fullscreen_method type;
		struct weston_transform transform; /* matrix from x, y */
		uint32_t framerate;
		struct weston_surface *black_surface;
	} fullscreen;

	struct ping_timer *ping_timer;

	struct {
		struct weston_animation current;
		int exists;
		int fading_in;
		uint32_t timestamp;
	} unresponsive_animation;

	struct weston_output *fullscreen_output;
	struct weston_output *output;
	struct wl_list link;

	int force_configure;
};

struct shell_grab {
	struct wl_pointer_grab grab;
	struct shell_surface *shsurf;
	struct wl_listener shsurf_destroy_listener;
};

struct weston_move_grab {
	struct shell_grab base;
	int32_t dx, dy;
};

struct rotate_grab {
	struct shell_grab base;
	struct weston_matrix rotation;
	struct {
		int32_t x;
		int32_t y;
	} center;
};

static struct shell_surface *
get_shell_surface(struct weston_surface *surface);

static struct desktop_shell *
shell_surface_get_shell(struct shell_surface *shsurf);

static bool
shell_surface_is_top_fullscreen(struct shell_surface *shsurf)
{
	struct desktop_shell *shell;
	struct weston_surface *top_fs_es;

	shell = shell_surface_get_shell(shsurf);
	
	if (wl_list_empty(&shell->fullscreen_layer.surface_list))
		return false;

	top_fs_es = container_of(shell->fullscreen_layer.surface_list.next,
			         struct weston_surface, 
				 layer_link);
	return (shsurf == get_shell_surface(top_fs_es));
}

static void
destroy_shell_grab_shsurf(struct wl_listener *listener, void *data)
{
	struct shell_grab *grab;

	grab = container_of(listener, struct shell_grab,
			    shsurf_destroy_listener);

	grab->shsurf = NULL;
}

static void
shell_grab_init(struct shell_grab *grab,
		const struct wl_pointer_grab_interface *interface,
		struct shell_surface *shsurf)
{
	grab->grab.interface = interface;
	grab->shsurf = shsurf;
	grab->shsurf_destroy_listener.notify = destroy_shell_grab_shsurf;
	wl_signal_add(&shsurf->resource.destroy_signal,
		      &grab->shsurf_destroy_listener);

}

static void
shell_grab_finish(struct shell_grab *grab)
{
	wl_list_remove(&grab->shsurf_destroy_listener.link);
}

static void
center_on_output(struct weston_surface *surface,
		 struct weston_output *output);

static uint32_t
get_modifier(char *modifier)
{
	if (!modifier)
		return MODIFIER_SUPER;

	if (!strcmp("ctrl", modifier))
		return MODIFIER_CTRL;
	else if (!strcmp("alt", modifier))
		return MODIFIER_ALT;
	else if (!strcmp("super", modifier))
		return MODIFIER_SUPER;
	else
		return MODIFIER_SUPER;
}

static void
shell_configuration(struct desktop_shell *shell)
{
	char *config_file;
	char *path = NULL;
	int duration = 60;
	char *modifier = NULL;

	struct config_key shell_keys[] = {
		{ "binding-modifier",   CONFIG_KEY_STRING, &modifier },
	};

	struct config_key saver_keys[] = {
		{ "path",       CONFIG_KEY_STRING,  &path },
		{ "duration",   CONFIG_KEY_INTEGER, &duration },
	};

	struct config_section cs[] = {
		{ "shell", shell_keys, ARRAY_LENGTH(shell_keys), NULL },
		{ "screensaver", saver_keys, ARRAY_LENGTH(saver_keys), NULL },
	};

	config_file = config_file_path("weston.ini");
	parse_config_file(config_file, cs, ARRAY_LENGTH(cs), shell);
	free(config_file);

	shell->screensaver.path = path;
	shell->screensaver.duration = duration;
	shell->binding_modifier = get_modifier(modifier);
}

static void
noop_grab_focus(struct wl_pointer_grab *grab,
		struct wl_surface *surface, int32_t x, int32_t y)
{
	grab->focus = NULL;
}

static void
move_grab_motion(struct wl_pointer_grab *grab,
		 uint32_t time, int32_t x, int32_t y)
{
	struct weston_move_grab *move = (struct weston_move_grab *) grab;
	struct wl_input_device *device = grab->input_device;
	struct shell_surface *shsurf = move->base.shsurf;
	struct weston_surface *es;

	if (!shsurf)
		return;

	es = shsurf->surface;

	weston_surface_configure(es,
				 device->x + move->dx,
				 device->y + move->dy,
				 es->geometry.width, es->geometry.height);
}

static void
move_grab_button(struct wl_pointer_grab *grab,
		 uint32_t time, uint32_t button, int32_t state)
{
	struct shell_grab *shell_grab = container_of(grab, struct shell_grab,
						    grab);
	struct wl_input_device *device = grab->input_device;

	if (device->button_count == 0 && state == 0) {
		shell_grab_finish(shell_grab);
		wl_input_device_end_pointer_grab(device);
		free(grab);
	}
}

static const struct wl_pointer_grab_interface move_grab_interface = {
	noop_grab_focus,
	move_grab_motion,
	move_grab_button,
};

static void
unresponsive_surface_fade(struct shell_surface *shsurf, bool reverse)
{
	shsurf->unresponsive_animation.fading_in = reverse ? 0 : 1;

	if(!shsurf->unresponsive_animation.exists) {
		wl_list_insert(&shsurf->surface->compositor->animation_list,
		       &shsurf->unresponsive_animation.current.link);
		shsurf->unresponsive_animation.exists = 1;
		shsurf->unresponsive_animation.timestamp = weston_compositor_get_time();
		weston_surface_damage(shsurf->surface);
	}
}

static void
unresponsive_fade_frame(struct weston_animation *animation,
		struct weston_output *output, uint32_t msecs)
{
	struct shell_surface *shsurf =
		container_of(animation, struct shell_surface, unresponsive_animation.current);
	struct weston_surface *surface = shsurf->surface;
	unsigned int step = 8;

	if (!surface || !shsurf)
		return;

	if (shsurf->unresponsive_animation.fading_in) {
		while (step < msecs - shsurf->unresponsive_animation.timestamp) {
			if (surface->saturation > 1)
				surface->saturation -= 5;
			if (surface->brightness > 200)
				surface->brightness--;

			shsurf->unresponsive_animation.timestamp += step;
		}

		if (surface->saturation <= 1 && surface->brightness <= 200) {
			wl_list_remove(&shsurf->unresponsive_animation.current.link);
			shsurf->unresponsive_animation.exists = 0;
		}
	}
	else {
		while (step < msecs - shsurf->unresponsive_animation.timestamp) {
			if (surface->saturation < 255)
				surface->saturation += 5;
			if (surface->brightness < 255)
				surface->brightness++;

			shsurf->unresponsive_animation.timestamp += step;
		}

		if (surface->saturation >= 255 && surface->brightness >= 255) {
			surface->saturation = surface->brightness = 255;
			wl_list_remove(&shsurf->unresponsive_animation.current.link);
			shsurf->unresponsive_animation.exists = 0;
		}
	}

	surface->geometry.dirty = 1;
	weston_surface_damage(surface);
}

static void
ping_timer_destroy(struct shell_surface *shsurf)
{
	if (!shsurf || !shsurf->ping_timer)
		return;

	if (shsurf->ping_timer->source)
		wl_event_source_remove(shsurf->ping_timer->source);

	free(shsurf->ping_timer);
	shsurf->ping_timer = NULL;
}

static int
ping_timeout_handler(void *data)
{
	struct shell_surface *shsurf = data;

	/* Client is not responding */
	shsurf->unresponsive = 1;
	unresponsive_surface_fade(shsurf, false);

	return 1;
}

static void
ping_handler(struct weston_surface *surface, uint32_t serial)
{
	struct shell_surface *shsurf;
	shsurf = get_shell_surface(surface);
	struct wl_event_loop *loop;
	int ping_timeout = 2500;

	if (!shsurf)
		return;
	if (!shsurf->resource.client)
		return;

	if (!shsurf->ping_timer) {
		shsurf->ping_timer = malloc(sizeof shsurf->ping_timer);
		if (!shsurf->ping_timer)
			return;

		shsurf->ping_timer->serial = serial;
		loop = wl_display_get_event_loop(surface->compositor->wl_display);
		shsurf->ping_timer->source =
			wl_event_loop_add_timer(loop, ping_timeout_handler, shsurf);
		wl_event_source_timer_update(shsurf->ping_timer->source, ping_timeout);

		wl_shell_surface_send_ping(&shsurf->resource, serial);
	}
}

static void
shell_surface_pong(struct wl_client *client, struct wl_resource *resource,
							uint32_t serial)
{
	struct shell_surface *shsurf = resource->data;

	if (shsurf->ping_timer->serial == serial) {
		if (shsurf->unresponsive) {
			/* Received pong from previously unresponsive client */
			unresponsive_surface_fade(shsurf, true);
		}
		shsurf->unresponsive = 0;
		ping_timer_destroy(shsurf);
	}
}

static int
weston_surface_move(struct weston_surface *es,
		    struct weston_input_device *wd)
{
	struct weston_move_grab *move;
	struct shell_surface *shsurf = get_shell_surface(es);

	if (!shsurf)
		return -1;

	move = malloc(sizeof *move);
	if (!move)
		return -1;

	shell_grab_init(&move->base, &move_grab_interface, shsurf);

	move->dx = es->geometry.x - wd->input_device.grab_x;
	move->dy = es->geometry.y - wd->input_device.grab_y;

	wl_input_device_start_pointer_grab(&wd->input_device,
					   &move->base.grab);

	wl_input_device_set_pointer_focus(&wd->input_device, NULL, 0, 0);

	return 0;
}

static void
shell_surface_move(struct wl_client *client, struct wl_resource *resource,
		   struct wl_resource *input_resource, uint32_t serial)
{
	struct weston_input_device *wd = input_resource->data;
	struct shell_surface *shsurf = resource->data;

	if (wd->input_device.button_count == 0 ||
	    wd->input_device.grab_serial != serial ||
	    wd->input_device.pointer_focus != &shsurf->surface->surface)
		return;

	if (weston_surface_move(shsurf->surface, wd) < 0)
		wl_resource_post_no_memory(resource);
}

struct weston_resize_grab {
	struct shell_grab base;
	uint32_t edges;
	int32_t width, height;
};

static void
resize_grab_motion(struct wl_pointer_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	struct weston_resize_grab *resize = (struct weston_resize_grab *) grab;
	struct wl_input_device *device = grab->input_device;
	int32_t width, height;
	int32_t from_x, from_y;
	int32_t to_x, to_y;

	if (!resize->base.shsurf)
		return;

	weston_surface_from_global(resize->base.shsurf->surface,
				   device->grab_x, device->grab_y,
				   &from_x, &from_y);
	weston_surface_from_global(resize->base.shsurf->surface,
				   device->x, device->y, &to_x, &to_y);

	if (resize->edges & WL_SHELL_SURFACE_RESIZE_LEFT) {
		width = resize->width + from_x - to_x;
	} else if (resize->edges & WL_SHELL_SURFACE_RESIZE_RIGHT) {
		width = resize->width + to_x - from_x;
	} else {
		width = resize->width;
	}

	if (resize->edges & WL_SHELL_SURFACE_RESIZE_TOP) {
		height = resize->height + from_y - to_y;
	} else if (resize->edges & WL_SHELL_SURFACE_RESIZE_BOTTOM) {
		height = resize->height + to_y - from_y;
	} else {
		height = resize->height;
	}

	wl_shell_surface_send_configure(&resize->base.shsurf->resource,
					resize->edges, width, height);
}

static void
resize_grab_button(struct wl_pointer_grab *grab,
		   uint32_t time, uint32_t button, int32_t state)
{
	struct weston_resize_grab *resize = (struct weston_resize_grab *) grab;
	struct wl_input_device *device = grab->input_device;

	if (device->button_count == 0 && state == 0) {
		shell_grab_finish(&resize->base);
		wl_input_device_end_pointer_grab(device);
		free(grab);
	}
}

static const struct wl_pointer_grab_interface resize_grab_interface = {
	noop_grab_focus,
	resize_grab_motion,
	resize_grab_button,
};

static int
weston_surface_resize(struct shell_surface *shsurf,
		      struct weston_input_device *wd, uint32_t edges)
{
	struct weston_resize_grab *resize;

	if (shsurf->type == SHELL_SURFACE_FULLSCREEN)
		return 0;

	if (edges == 0 || edges > 15 ||
	    (edges & 3) == 3 || (edges & 12) == 12)
		return 0;

	resize = malloc(sizeof *resize);
	if (!resize)
		return -1;

	shell_grab_init(&resize->base, &resize_grab_interface, shsurf);

	resize->edges = edges;
	resize->width = shsurf->surface->geometry.width;
	resize->height = shsurf->surface->geometry.height;

	wl_input_device_start_pointer_grab(&wd->input_device,
					   &resize->base.grab);

	wl_input_device_set_pointer_focus(&wd->input_device, NULL, 0, 0);

	return 0;
}

static void
shell_surface_resize(struct wl_client *client, struct wl_resource *resource,
		     struct wl_resource *input_resource, uint32_t serial,
		     uint32_t edges)
{
	struct weston_input_device *wd = input_resource->data;
	struct shell_surface *shsurf = resource->data;

	if (shsurf->type == SHELL_SURFACE_FULLSCREEN)
		return;

	if (wd->input_device.button_count == 0 ||
	    wd->input_device.grab_serial != serial ||
	    wd->input_device.pointer_focus != &shsurf->surface->surface)
		return;

	if (weston_surface_resize(shsurf, wd, edges) < 0)
		wl_resource_post_no_memory(resource);
}

static struct weston_output *
get_default_output(struct weston_compositor *compositor)
{
	return container_of(compositor->output_list.next,
			    struct weston_output, link);
}

static void
shell_unset_fullscreen(struct shell_surface *shsurf)
{
	/* undo all fullscreen things here */
	if (shsurf->fullscreen.type == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER &&
	    shell_surface_is_top_fullscreen(shsurf)) {
		weston_output_switch_mode(shsurf->fullscreen_output,
		                          shsurf->fullscreen_output->origin);
	}
	shsurf->fullscreen.type = WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT;
	shsurf->fullscreen.framerate = 0;
	wl_list_remove(&shsurf->fullscreen.transform.link);
	wl_list_init(&shsurf->fullscreen.transform.link);
	if (shsurf->fullscreen.black_surface)
		weston_surface_destroy(shsurf->fullscreen.black_surface);
	shsurf->fullscreen.black_surface = NULL;
	shsurf->fullscreen_output = NULL;
	shsurf->force_configure = 1;
	weston_surface_set_position(shsurf->surface,
				    shsurf->saved_x, shsurf->saved_y);
}

static int
reset_shell_surface_type(struct shell_surface *surface)
{
	switch (surface->type) {
	case SHELL_SURFACE_FULLSCREEN:
		shell_unset_fullscreen(surface);
		break;
	case SHELL_SURFACE_MAXIMIZED:
		surface->output = get_default_output(surface->surface->compositor);
		weston_surface_set_position(surface->surface,
					    surface->saved_x,
					    surface->saved_y);
		break;
	case SHELL_SURFACE_PANEL:
	case SHELL_SURFACE_BACKGROUND:
		wl_list_remove(&surface->link);
		wl_list_init(&surface->link);
		break;
	case SHELL_SURFACE_SCREENSAVER:
	case SHELL_SURFACE_LOCK:
		wl_resource_post_error(&surface->resource,
				       WL_DISPLAY_ERROR_INVALID_METHOD,
				       "cannot reassign surface type");
		return -1;
	case SHELL_SURFACE_NONE:
	case SHELL_SURFACE_TOPLEVEL:
	case SHELL_SURFACE_TRANSIENT:
	case SHELL_SURFACE_POPUP:
		break;
	}

	surface->type = SHELL_SURFACE_NONE;
	return 0;
}

static void
set_toplevel(struct shell_surface *shsurf)
{
       if (reset_shell_surface_type(shsurf))
               return;

       shsurf->type = SHELL_SURFACE_TOPLEVEL;
}

static void
shell_surface_set_toplevel(struct wl_client *client,
			   struct wl_resource *resource)

{
	struct shell_surface *surface = resource->data;

	set_toplevel(surface);
}

static void
shell_surface_set_transient(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *parent_resource,
			    int x, int y, uint32_t flags)
{
	struct shell_surface *shsurf = resource->data;
	struct weston_surface *es = shsurf->surface;
	struct shell_surface *pshsurf = parent_resource->data;
	struct weston_surface *pes = pshsurf->surface;

	if (reset_shell_surface_type(shsurf))
		return;

	/* assign to parents output */
	shsurf->output = pes->output;
 	weston_surface_set_position(es, pes->geometry.x + x,
					pes->geometry.y + y);

	shsurf->type = SHELL_SURFACE_TRANSIENT;
}

static struct desktop_shell *
shell_surface_get_shell(struct shell_surface *shsurf)
{
	return shsurf->shell;
}

static int
get_output_panel_height(struct desktop_shell *shell,
			struct weston_output *output)
{
	struct shell_surface *priv;
	int panel_height = 0;

	if (!output)
		return 0;

	wl_list_for_each(priv, &shell->panels, link) {
		if (priv->output == output) {
			panel_height = priv->surface->geometry.height;
			break;
		}
	}
	return panel_height;
}

static void
shell_surface_set_maximized(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *output_resource )
{
	struct shell_surface *shsurf = resource->data;
	struct weston_surface *es = shsurf->surface;
	struct desktop_shell *shell = NULL;
	uint32_t edges = 0, panel_height = 0;

	/* get the default output, if the client set it as NULL
	   check whether the ouput is available */
	if (output_resource)
		shsurf->output = output_resource->data;
	else
		shsurf->output = get_default_output(es->compositor);

	if (reset_shell_surface_type(shsurf))
		return;

	shsurf->saved_x = es->geometry.x;
	shsurf->saved_y = es->geometry.y;
	shsurf->saved_position_valid = true;

	shell = shell_surface_get_shell(shsurf);
	panel_height = get_output_panel_height(shell, es->output);
	edges = WL_SHELL_SURFACE_RESIZE_TOP|WL_SHELL_SURFACE_RESIZE_LEFT;

	wl_shell_surface_send_configure(&shsurf->resource, edges,
					es->output->current->width,
					es->output->current->height - panel_height);

	shsurf->type = SHELL_SURFACE_MAXIMIZED;
}

static void
black_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy);

static struct weston_surface *
create_black_surface(struct weston_compositor *ec,
		     struct weston_surface *fs_surface,
		     GLfloat x, GLfloat y, int w, int h)
{
	struct weston_surface *surface = NULL;

	surface = weston_surface_create(ec);
	if (surface == NULL) {
		fprintf(stderr, "no memory\n");
		return NULL;
	}

	surface->configure = black_surface_configure;
	surface->private = fs_surface;
	weston_surface_configure(surface, x, y, w, h);
	weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1);
	return surface;
}

/* Create black surface and append it to the associated fullscreen surface.
 * Handle size dismatch and positioning according to the method. */
static void
shell_configure_fullscreen(struct shell_surface *shsurf)
{
	struct weston_output *output = shsurf->fullscreen_output;
	struct weston_surface *surface = shsurf->surface;
	struct weston_matrix *matrix;
	float scale;

	center_on_output(surface, output);

	if (!shsurf->fullscreen.black_surface)
		shsurf->fullscreen.black_surface =
			create_black_surface(surface->compositor,
					     surface,
					     output->x, output->y,
					     output->current->width,
					     output->current->height);

	wl_list_remove(&shsurf->fullscreen.black_surface->layer_link);
	wl_list_insert(&surface->layer_link,
		       &shsurf->fullscreen.black_surface->layer_link);
	shsurf->fullscreen.black_surface->output = output;

	switch (shsurf->fullscreen.type) {
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT:
		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE:
		matrix = &shsurf->fullscreen.transform.matrix;
		weston_matrix_init(matrix);
		scale = (float)output->current->width/(float)surface->geometry.width;
		weston_matrix_scale(matrix, scale, scale, 1);
		wl_list_remove(&shsurf->fullscreen.transform.link);
		wl_list_insert(surface->geometry.transformation_list.prev,
			       &shsurf->fullscreen.transform.link);
		weston_surface_set_position(surface, output->x, output->y);
		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER:
		if (shell_surface_is_top_fullscreen(shsurf)) {
			struct weston_mode mode = {0, 
				surface->geometry.width,
				surface->geometry.height,
				shsurf->fullscreen.framerate};

			if (weston_output_switch_mode(output, &mode) == 0) {
				weston_surface_configure(shsurf->fullscreen.black_surface, 
					                 output->x, output->y,
							 output->current->width,
							 output->current->height);
				weston_surface_set_position(surface, output->x, output->y);
				break;
			}
		}
		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_FILL:
		break;
	default:
		break;
	}
}

/* make the fullscreen and black surface at the top */
static void
shell_stack_fullscreen(struct shell_surface *shsurf)
{
	struct weston_output *output = shsurf->fullscreen_output;
	struct weston_surface *surface = shsurf->surface;
	struct desktop_shell *shell = shell_surface_get_shell(shsurf);

	wl_list_remove(&surface->layer_link);
	wl_list_insert(&shell->fullscreen_layer.surface_list,
		       &surface->layer_link);
	weston_surface_damage(surface);

	if (!shsurf->fullscreen.black_surface)
		shsurf->fullscreen.black_surface =
			create_black_surface(surface->compositor,
					     surface,
					     output->x, output->y,
					     output->current->width,
					     output->current->height);

	wl_list_remove(&shsurf->fullscreen.black_surface->layer_link);
	wl_list_insert(&surface->layer_link,
		       &shsurf->fullscreen.black_surface->layer_link);
	weston_surface_damage(shsurf->fullscreen.black_surface);
}

static void
shell_map_fullscreen(struct shell_surface *shsurf)
{
	shell_stack_fullscreen(shsurf);
	shell_configure_fullscreen(shsurf);
}

static void
shell_surface_set_fullscreen(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t method,
			     uint32_t framerate,
			     struct wl_resource *output_resource)
{
	struct shell_surface *shsurf = resource->data;
	struct weston_surface *es = shsurf->surface;

	if (output_resource)
		shsurf->output = output_resource->data;
	else
		shsurf->output = get_default_output(es->compositor);

	if (reset_shell_surface_type(shsurf))
		return;

	shsurf->fullscreen_output = shsurf->output;
	shsurf->fullscreen.type = method;
	shsurf->fullscreen.framerate = framerate;
	shsurf->type = SHELL_SURFACE_FULLSCREEN;

	shsurf->saved_x = es->geometry.x;
	shsurf->saved_y = es->geometry.y;
	shsurf->saved_position_valid = true;

	if (weston_surface_is_mapped(es))
		shsurf->force_configure = 1;

	wl_shell_surface_send_configure(&shsurf->resource, 0,
					shsurf->output->current->width,
					shsurf->output->current->height);
}

static void
popup_grab_focus(struct wl_pointer_grab *grab,
		 struct wl_surface *surface, int32_t x, int32_t y)
{
	struct wl_input_device *device = grab->input_device;
	struct shell_surface *priv =
		container_of(grab, struct shell_surface, popup.grab);
	struct wl_client *client = priv->surface->surface.resource.client;

	if (surface && surface->resource.client == client) {
		wl_input_device_set_pointer_focus(device, surface, x, y);
		grab->focus = surface;
	} else {
		wl_input_device_set_pointer_focus(device, NULL, 0, 0);
		grab->focus = NULL;
	}
}

static void
popup_grab_motion(struct wl_pointer_grab *grab,
		  uint32_t time, int32_t sx, int32_t sy)
{
	struct wl_resource *resource;

	resource = grab->input_device->pointer_focus_resource;
	if (resource)
		wl_input_device_send_motion(resource, time, sx, sy);
}

static void
popup_grab_button(struct wl_pointer_grab *grab,
		  uint32_t time, uint32_t button, int32_t state)
{
	struct wl_resource *resource;
	struct shell_surface *shsurf =
		container_of(grab, struct shell_surface, popup.grab);
	struct wl_display *display;
	uint32_t serial;

	resource = grab->input_device->pointer_focus_resource;
	if (resource) {
		display = wl_client_get_display(resource->client);
		serial = wl_display_get_serial(display);
		wl_input_device_send_button(resource, serial,
					    time, button, state);
	} else if (state == 0 &&
		   (shsurf->popup.initial_up ||
		    time - shsurf->popup.device->grab_time > 500)) {
		wl_shell_surface_send_popup_done(&shsurf->resource);
		wl_input_device_end_pointer_grab(grab->input_device);
		shsurf->popup.grab.input_device = NULL;
	}

	if (state == 0)
		shsurf->popup.initial_up = 1;
}

static const struct wl_pointer_grab_interface popup_grab_interface = {
	popup_grab_focus,
	popup_grab_motion,
	popup_grab_button,
};

static void
shell_map_popup(struct shell_surface *shsurf)
{
	struct wl_input_device *device = shsurf->popup.device;
	struct weston_surface *es = shsurf->surface;
	struct weston_surface *parent = shsurf->parent->surface;

	es->output = parent->output;
	shsurf->popup.grab.interface = &popup_grab_interface;

	weston_surface_update_transform(parent);
	if (parent->transform.enabled) {
		shsurf->popup.parent_transform.matrix =
			parent->transform.matrix;
	} else {
		/* construct x, y translation matrix */
		weston_matrix_init(&shsurf->popup.parent_transform.matrix);
		shsurf->popup.parent_transform.matrix.d[12] =
			parent->geometry.x;
		shsurf->popup.parent_transform.matrix.d[13] =
			parent->geometry.y;
	}
	wl_list_insert(es->geometry.transformation_list.prev,
		       &shsurf->popup.parent_transform.link);
	weston_surface_set_position(es, shsurf->popup.x, shsurf->popup.y);

	shsurf->popup.initial_up = 0;

	/* We don't require the grab to still be active, but if another
	 * grab has started in the meantime, we end the popup now. */
	if (device->grab_serial == shsurf->popup.serial) {
		wl_input_device_start_pointer_grab(device,
						   &shsurf->popup.grab);
	} else {
		wl_shell_surface_send_popup_done(&shsurf->resource);
	}
}

static void
shell_surface_set_popup(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *input_device_resource,
			uint32_t serial,
			struct wl_resource *parent_resource,
			int32_t x, int32_t y, uint32_t flags)
{
	struct shell_surface *shsurf = resource->data;

	shsurf->type = SHELL_SURFACE_POPUP;
	shsurf->parent = parent_resource->data;
	shsurf->popup.device = input_device_resource->data;
	shsurf->popup.serial = serial;
	shsurf->popup.x = x;
	shsurf->popup.y = y;
}

static const struct wl_shell_surface_interface shell_surface_implementation = {
	shell_surface_pong,
	shell_surface_move,
	shell_surface_resize,
	shell_surface_set_toplevel,
	shell_surface_set_transient,
	shell_surface_set_fullscreen,
	shell_surface_set_popup,
	shell_surface_set_maximized
};

static void
destroy_shell_surface(struct shell_surface *shsurf)
{
	if (shsurf->popup.grab.input_device)
		wl_input_device_end_pointer_grab(shsurf->popup.grab.input_device);

	if (shsurf->fullscreen.type == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER &&
	    shell_surface_is_top_fullscreen(shsurf)) {
		weston_output_switch_mode(shsurf->fullscreen_output,
					  shsurf->fullscreen_output->origin);
	}

	if (shsurf->fullscreen.black_surface)
		weston_surface_destroy(shsurf->fullscreen.black_surface);

	/* As destroy_resource() use wl_list_for_each_safe(),
	 * we can always remove the listener.
	 */
	wl_list_remove(&shsurf->surface_destroy_listener.link);
	shsurf->surface->configure = NULL;
	ping_timer_destroy(shsurf);

	wl_list_remove(&shsurf->link);
	free(shsurf);
}

static void
shell_destroy_shell_surface(struct wl_resource *resource)
{
	struct shell_surface *shsurf = resource->data;

	destroy_shell_surface(shsurf);
}

static void
shell_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct shell_surface *shsurf = container_of(listener,
						    struct shell_surface,
						    surface_destroy_listener);

	/* tricky way to check if resource was in fact created */
	if (shsurf->resource.object.implementation != 0)
		wl_resource_destroy(&shsurf->resource);
	else
		destroy_shell_surface(shsurf);
}

static struct shell_surface *
get_shell_surface(struct weston_surface *surface)
{
	struct wl_listener *listener;

	listener = wl_signal_get(&surface->surface.resource.destroy_signal,
				 shell_handle_surface_destroy);
	if (listener)
		return container_of(listener, struct shell_surface,
				    surface_destroy_listener);

	return NULL;
}

static void
shell_surface_configure(struct weston_surface *, int32_t, int32_t);

static void
create_shell_surface(void *shell, struct weston_surface *surface,
		     struct shell_surface **ret)
{
	struct shell_surface *shsurf;

	if (surface->configure) {
		fprintf(stderr, "surface->configure already set\n");
		return;
	}

	shsurf = calloc(1, sizeof *shsurf);
	if (!shsurf) {
		fprintf(stderr, "no memory to allocate shell surface\n");
		return;
	}

	surface->configure = shell_surface_configure;
	surface->compositor->shell_interface.shell = shell;

	shsurf->shell = (struct desktop_shell *) shell;
	shsurf->unresponsive = 0;
	shsurf->unresponsive_animation.exists = 0;
	shsurf->unresponsive_animation.fading_in = 0;
	shsurf->unresponsive_animation.current.frame = unresponsive_fade_frame;
	shsurf->saved_position_valid = false;
	shsurf->surface = surface;
	shsurf->fullscreen.type = WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT;
	shsurf->fullscreen.framerate = 0;
	shsurf->fullscreen.black_surface = NULL;
	shsurf->ping_timer = NULL;
	wl_list_init(&shsurf->fullscreen.transform.link);

	wl_signal_init(&shsurf->resource.destroy_signal);
	shsurf->surface_destroy_listener.notify = shell_handle_surface_destroy;
	wl_signal_add(&surface->surface.resource.destroy_signal,
		      &shsurf->surface_destroy_listener);

	/* init link so its safe to always remove it in destroy_shell_surface */
	wl_list_init(&shsurf->link);

	/* empty when not in use */
	wl_list_init(&shsurf->rotation.transform.link);
	weston_matrix_init(&shsurf->rotation.rotation);

	shsurf->type = SHELL_SURFACE_NONE;

	*ret = shsurf;
}

static void
shell_get_shell_surface(struct wl_client *client,
			struct wl_resource *resource,
			uint32_t id,
			struct wl_resource *surface_resource)
{
	struct weston_surface *surface = surface_resource->data;
	struct desktop_shell *shell = resource->data;
	struct shell_surface *shsurf;

	if (get_shell_surface(surface)) {
		wl_resource_post_error(surface_resource,
			WL_DISPLAY_ERROR_INVALID_OBJECT,
			"desktop_shell::get_shell_surface already requested");
		return;
	}

       create_shell_surface(shell, surface, &shsurf);
       if (!shsurf) {
	       wl_resource_post_error(surface_resource,
				      WL_DISPLAY_ERROR_INVALID_OBJECT,
				      "surface->configure already set");
	       return;
       }

       shsurf->resource.destroy = shell_destroy_shell_surface;
       shsurf->resource.object.id = id;
       shsurf->resource.object.interface = &wl_shell_surface_interface;
       shsurf->resource.object.implementation =
	       (void (**)(void)) &shell_surface_implementation;
       shsurf->resource.data = shsurf;

       wl_client_add_resource(client, &shsurf->resource);
}

static const struct wl_shell_interface shell_implementation = {
	shell_get_shell_surface
};

static void
handle_screensaver_sigchld(struct weston_process *proc, int status)
{
	proc->pid = 0;
}

static void
launch_screensaver(struct desktop_shell *shell)
{
	if (shell->screensaver.binding)
		return;

	if (!shell->screensaver.path)
		return;

	if (shell->screensaver.process.pid != 0) {
		fprintf(stderr, "old screensaver still running\n");
		return;
	}

	weston_client_launch(shell->compositor,
			   &shell->screensaver.process,
			   shell->screensaver.path,
			   handle_screensaver_sigchld);
}

static void
terminate_screensaver(struct desktop_shell *shell)
{
	if (shell->screensaver.process.pid == 0)
		return;

	kill(shell->screensaver.process.pid, SIGTERM);
}

static void
show_screensaver(struct desktop_shell *shell, struct shell_surface *surface)
{
	struct wl_list *list;

	if (shell->lock_surface)
		list = &shell->lock_surface->surface->layer_link;
	else
		list = &shell->lock_layer.surface_list;

	wl_list_remove(&surface->surface->layer_link);
	wl_list_insert(list, &surface->surface->layer_link);
	surface->surface->output = surface->output;
	weston_surface_damage(surface->surface);
}

static void
hide_screensaver(struct desktop_shell *shell, struct shell_surface *surface)
{
	wl_list_remove(&surface->surface->layer_link);
	wl_list_init(&surface->surface->layer_link);
	surface->surface->output = NULL;
}

static void
desktop_shell_set_background(struct wl_client *client,
			     struct wl_resource *resource,
			     struct wl_resource *output_resource,
			     struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = resource->data;
	struct shell_surface *shsurf = surface_resource->data;
	struct weston_surface *surface = shsurf->surface;
	struct shell_surface *priv;

	if (reset_shell_surface_type(shsurf))
		return;

	wl_list_for_each(priv, &shell->backgrounds, link) {
		if (priv->output == output_resource->data) {
			priv->surface->output = NULL;
			wl_list_remove(&priv->surface->layer_link);
			wl_list_remove(&priv->link);
			break;
		}
	}

	shsurf->type = SHELL_SURFACE_BACKGROUND;
	shsurf->output = output_resource->data;

	wl_list_insert(&shell->backgrounds, &shsurf->link);

	weston_surface_set_position(surface, shsurf->output->x,
				    shsurf->output->y);

	desktop_shell_send_configure(resource, 0,
				     surface_resource,
				     shsurf->output->current->width,
				     shsurf->output->current->height);
}

static void
desktop_shell_set_panel(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *output_resource,
			struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = resource->data;
	struct shell_surface *shsurf = surface_resource->data;
	struct weston_surface *surface = shsurf->surface;
	struct shell_surface *priv;

	if (reset_shell_surface_type(shsurf))
		return;

	wl_list_for_each(priv, &shell->panels, link) {
		if (priv->output == output_resource->data) {
			priv->surface->output = NULL;
			wl_list_remove(&priv->surface->layer_link);
			wl_list_remove(&priv->link);
			break;
		}
	}

	shsurf->type = SHELL_SURFACE_PANEL;
	shsurf->output = output_resource->data;

	wl_list_insert(&shell->panels, &shsurf->link);

	weston_surface_set_position(surface, shsurf->output->x,
				    shsurf->output->y);

	desktop_shell_send_configure(resource, 0,
				     surface_resource,
				     shsurf->output->current->width,
				     shsurf->output->current->height);
}

static void
handle_lock_surface_destroy(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
	    container_of(listener, struct desktop_shell, lock_surface_listener);

	fprintf(stderr, "lock surface gone\n");
	shell->lock_surface = NULL;
}

static void
desktop_shell_set_lock_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = resource->data;
	struct shell_surface *surface = surface_resource->data;

	if (reset_shell_surface_type(surface))
		return;

	shell->prepare_event_sent = false;

	if (!shell->locked)
		return;

	shell->lock_surface = surface;

	shell->lock_surface_listener.notify = handle_lock_surface_destroy;
	wl_signal_add(&surface_resource->destroy_signal,
		      &shell->lock_surface_listener);

	shell->lock_surface->type = SHELL_SURFACE_LOCK;
}

static void
resume_desktop(struct desktop_shell *shell)
{
	struct shell_surface *tmp;

	wl_list_for_each(tmp, &shell->screensaver.surfaces, link)
		hide_screensaver(shell, tmp);

	terminate_screensaver(shell);

	wl_list_remove(&shell->lock_layer.link);
	wl_list_insert(&shell->compositor->cursor_layer.link,
		       &shell->fullscreen_layer.link);
	wl_list_insert(&shell->fullscreen_layer.link,
		       &shell->panel_layer.link);
	wl_list_insert(&shell->panel_layer.link, &shell->toplevel_layer.link);

	shell->locked = false;
	shell->compositor->idle_time = shell->compositor->option_idle_time;
	weston_compositor_wake(shell->compositor);
	weston_compositor_damage_all(shell->compositor);
}

static void
desktop_shell_unlock(struct wl_client *client,
		     struct wl_resource *resource)
{
	struct desktop_shell *shell = resource->data;

	shell->prepare_event_sent = false;

	if (shell->locked)
		resume_desktop(shell);
}

static const struct desktop_shell_interface desktop_shell_implementation = {
	desktop_shell_set_background,
	desktop_shell_set_panel,
	desktop_shell_set_lock_surface,
	desktop_shell_unlock
};

static enum shell_surface_type
get_shell_surface_type(struct weston_surface *surface)
{
	struct shell_surface *shsurf;

	shsurf = get_shell_surface(surface);
	if (!shsurf)
		return SHELL_SURFACE_NONE;
	return shsurf->type;
}

static void
move_binding(struct wl_input_device *device, uint32_t time,
	     uint32_t key, uint32_t button, uint32_t axis, int32_t state, void *data)
{
	struct weston_surface *surface =
		(struct weston_surface *) device->pointer_focus;

	if (surface == NULL)
		return;

	switch (get_shell_surface_type(surface)) {
		case SHELL_SURFACE_PANEL:
		case SHELL_SURFACE_BACKGROUND:
		case SHELL_SURFACE_FULLSCREEN:
		case SHELL_SURFACE_SCREENSAVER:
			return;
		default:
			break;
	}

	weston_surface_move(surface, (struct weston_input_device *) device);
}

static void
resize_binding(struct wl_input_device *device, uint32_t time,
	       uint32_t key, uint32_t button, uint32_t axis, int32_t state, void *data)
{
	struct weston_surface *surface =
		(struct weston_surface *) device->pointer_focus;
	uint32_t edges = 0;
	int32_t x, y;
	struct shell_surface *shsurf;

	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (!shsurf)
		return;

	switch (shsurf->type) {
		case SHELL_SURFACE_PANEL:
		case SHELL_SURFACE_BACKGROUND:
		case SHELL_SURFACE_FULLSCREEN:
		case SHELL_SURFACE_SCREENSAVER:
			return;
		default:
			break;
	}

	weston_surface_from_global(surface,
				   device->grab_x, device->grab_y, &x, &y);

	if (x < surface->geometry.width / 3)
		edges |= WL_SHELL_SURFACE_RESIZE_LEFT;
	else if (x < 2 * surface->geometry.width / 3)
		edges |= 0;
	else
		edges |= WL_SHELL_SURFACE_RESIZE_RIGHT;

	if (y < surface->geometry.height / 3)
		edges |= WL_SHELL_SURFACE_RESIZE_TOP;
	else if (y < 2 * surface->geometry.height / 3)
		edges |= 0;
	else
		edges |= WL_SHELL_SURFACE_RESIZE_BOTTOM;

	weston_surface_resize(shsurf, (struct weston_input_device *) device,
			      edges);
}

static void
surface_opacity_binding(struct wl_input_device *device, uint32_t time,
	       uint32_t key, uint32_t button, uint32_t axis, int32_t value, void *data)
{
	uint32_t step = 15;
	struct shell_surface *shsurf;
	struct weston_surface *surface =
		(struct weston_surface *) device->pointer_focus;

	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (!shsurf)
		return;

	switch (shsurf->type) {
		case SHELL_SURFACE_BACKGROUND:
		case SHELL_SURFACE_SCREENSAVER:
			return;
		default:
			break;
	}

	surface->alpha += value * step;

	if (surface->alpha > 255)
		surface->alpha = 255;
	if (surface->alpha < step)
		surface->alpha = step;

	surface->geometry.dirty = 1;
	weston_surface_damage(surface);
}

static void
zoom_binding(struct wl_input_device *device, uint32_t time,
	       uint32_t key, uint32_t button, uint32_t axis, int32_t value, void *data)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *compositor = wd->compositor;
	struct weston_output *output;

	wl_list_for_each(output, &compositor->output_list, link) {
		if (pixman_region32_contains_point(&output->region,
						device->x, device->y, NULL)) {
			output->zoom.active = 1;
			output->zoom.level += output->zoom.increment * -value;

			if (output->zoom.level >= 1.0) {
				output->zoom.active = 0;
				output->zoom.level = 1.0;
			}

			if (output->zoom.level < output->zoom.increment)
				output->zoom.level = output->zoom.increment;

			weston_output_update_zoom(output, device->x, device->y);
		}
	}
}

static void
terminate_binding(struct wl_input_device *device, uint32_t time,
		  uint32_t key, uint32_t button, uint32_t axis, int32_t state, void *data)
{
	struct weston_compositor *compositor = data;

	if (state)
		wl_display_terminate(compositor->wl_display);
}

static void
rotate_grab_motion(struct wl_pointer_grab *grab,
		 uint32_t time, int32_t x, int32_t y)
{
	struct rotate_grab *rotate =
		container_of(grab, struct rotate_grab, base.grab);
	struct wl_input_device *device = grab->input_device;
	struct shell_surface *shsurf = rotate->base.shsurf;
	struct weston_surface *surface;
	GLfloat cx, cy, dx, dy, cposx, cposy, dposx, dposy, r;

	if (!shsurf)
		return;

	surface = shsurf->surface;

	cx = 0.5f * surface->geometry.width;
	cy = 0.5f * surface->geometry.height;

	dx = device->x - rotate->center.x;
	dy = device->y - rotate->center.y;
	r = sqrtf(dx * dx + dy * dy);

	wl_list_remove(&shsurf->rotation.transform.link);
	shsurf->surface->geometry.dirty = 1;

	if (r > 20.0f) {
		struct weston_matrix *matrix =
			&shsurf->rotation.transform.matrix;

		weston_matrix_init(&rotate->rotation);
		rotate->rotation.d[0] = dx / r;
		rotate->rotation.d[4] = -dy / r;
		rotate->rotation.d[1] = -rotate->rotation.d[4];
		rotate->rotation.d[5] = rotate->rotation.d[0];

		weston_matrix_init(matrix);
		weston_matrix_translate(matrix, -cx, -cy, 0.0f);
		weston_matrix_multiply(matrix, &shsurf->rotation.rotation);
		weston_matrix_multiply(matrix, &rotate->rotation);
		weston_matrix_translate(matrix, cx, cy, 0.0f);

		wl_list_insert(
			&shsurf->surface->geometry.transformation_list,
			&shsurf->rotation.transform.link);
	} else {
		wl_list_init(&shsurf->rotation.transform.link);
		weston_matrix_init(&shsurf->rotation.rotation);
		weston_matrix_init(&rotate->rotation);
	}

	/* We need to adjust the position of the surface
	 * in case it was resized in a rotated state before */
	cposx = surface->geometry.x + cx;
	cposy = surface->geometry.y + cy;
	dposx = rotate->center.x - cposx;
	dposy = rotate->center.y - cposy;
	if (dposx != 0.0f || dposy != 0.0f) {
		weston_surface_set_position(surface,
					    surface->geometry.x + dposx,
					    surface->geometry.y + dposy);
	}

	/* Repaint implies weston_surface_update_transform(), which
	 * lazily applies the damage due to rotation update.
	 */
	weston_compositor_schedule_repaint(shsurf->surface->compositor);
}

static void
rotate_grab_button(struct wl_pointer_grab *grab,
		 uint32_t time, uint32_t button, int32_t state)
{
	struct rotate_grab *rotate =
		container_of(grab, struct rotate_grab, base.grab);
	struct wl_input_device *device = grab->input_device;
	struct shell_surface *shsurf = rotate->base.shsurf;

	if (device->button_count == 0 && state == 0) {
		if (shsurf)
			weston_matrix_multiply(&shsurf->rotation.rotation,
					       &rotate->rotation);
		shell_grab_finish(&rotate->base);
		wl_input_device_end_pointer_grab(device);
		free(rotate);
	}
}

static const struct wl_pointer_grab_interface rotate_grab_interface = {
	noop_grab_focus,
	rotate_grab_motion,
	rotate_grab_button,
};

static void
rotate_binding(struct wl_input_device *device, uint32_t time,
	       uint32_t key, uint32_t button, uint32_t axis, int32_t state, void *data)
{
	struct weston_surface *base_surface =
		(struct weston_surface *) device->pointer_focus;
	struct shell_surface *surface;
	struct rotate_grab *rotate;
	GLfloat dx, dy;
	GLfloat r;

	if (base_surface == NULL)
		return;

	surface = get_shell_surface(base_surface);
	if (!surface)
		return;

	switch (surface->type) {
		case SHELL_SURFACE_PANEL:
		case SHELL_SURFACE_BACKGROUND:
		case SHELL_SURFACE_FULLSCREEN:
		case SHELL_SURFACE_SCREENSAVER:
			return;
		default:
			break;
	}

	rotate = malloc(sizeof *rotate);
	if (!rotate)
		return;

	shell_grab_init(&rotate->base, &rotate_grab_interface, surface);

	weston_surface_to_global(surface->surface,
				 surface->surface->geometry.width / 2,
				 surface->surface->geometry.height / 2,
				 &rotate->center.x, &rotate->center.y);

	wl_input_device_start_pointer_grab(device, &rotate->base.grab);

	dx = device->x - rotate->center.x;
	dy = device->y - rotate->center.y;
	r = sqrtf(dx * dx + dy * dy);
	if (r > 20.0f) {
		struct weston_matrix inverse;

		weston_matrix_init(&inverse);
		inverse.d[0] = dx / r;
		inverse.d[4] = dy / r;
		inverse.d[1] = -inverse.d[4];
		inverse.d[5] = inverse.d[0];
		weston_matrix_multiply(&surface->rotation.rotation, &inverse);

		weston_matrix_init(&rotate->rotation);
		rotate->rotation.d[0] = dx / r;
		rotate->rotation.d[4] = -dy / r;
		rotate->rotation.d[1] = -rotate->rotation.d[4];
		rotate->rotation.d[5] = rotate->rotation.d[0];
	} else {
		weston_matrix_init(&surface->rotation.rotation);
		weston_matrix_init(&rotate->rotation);
	}

	wl_input_device_set_pointer_focus(device, NULL, 0, 0);
}

static void
activate(struct desktop_shell *shell, struct weston_surface *es,
	 struct weston_input_device *device)
{
	struct weston_surface *surf, *prev;

	weston_surface_activate(es, device);

	switch (get_shell_surface_type(es)) {
	case SHELL_SURFACE_BACKGROUND:
	case SHELL_SURFACE_PANEL:
	case SHELL_SURFACE_LOCK:
		break;

	case SHELL_SURFACE_SCREENSAVER:
		/* always below lock surface */
		if (shell->lock_surface)
			weston_surface_restack(es,
					       &shell->lock_surface->surface->layer_link);
		break;
	case SHELL_SURFACE_FULLSCREEN:
		/* should on top of panels */
		shell_stack_fullscreen(get_shell_surface(es));
		shell_configure_fullscreen(get_shell_surface(es));
		break;
	default:
		/* move the fullscreen surfaces down into the toplevel layer */
		if (!wl_list_empty(&shell->fullscreen_layer.surface_list)) {
			wl_list_for_each_reverse_safe(surf,
						      prev, 
					              &shell->fullscreen_layer.surface_list, 
						      layer_link)
				weston_surface_restack(surf,
						       &shell->toplevel_layer.surface_list); 
		}

		weston_surface_restack(es,
				       &shell->toplevel_layer.surface_list);
		break;
	}
}

/* no-op func for checking black surface */
static void
black_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
}

static bool 
is_black_surface (struct weston_surface *es, struct weston_surface **fs_surface)
{
	if (es->configure == black_surface_configure) {
		if (fs_surface)
			*fs_surface = (struct weston_surface *)es->private;
		return true;
	}
	return false;
}

static void
click_to_activate_binding(struct wl_input_device *device,
			  uint32_t time, uint32_t key,
			  uint32_t button, uint32_t axis, int32_t state, void *data)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct desktop_shell *shell = data;
	struct weston_surface *focus;
	struct weston_surface *upper;

	focus = (struct weston_surface *) device->pointer_focus;
	if (!focus)
		return;

	if (is_black_surface(focus, &upper))
		focus = upper;

	if (state && device->pointer_grab == &device->default_pointer_grab)
		activate(shell, focus, wd);
}

static void
lock(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, lock_listener);
	struct weston_input_device *device;
	struct shell_surface *shsurf;
	struct weston_output *output;

	if (shell->locked) {
		wl_list_for_each(output, &shell->compositor->output_list, link)
			/* TODO: find a way to jump to other DPMS levels */
			if (output->set_dpms)
				output->set_dpms(output, WESTON_DPMS_STANDBY);
		return;
	}

	shell->locked = true;

	/* Hide all surfaces by removing the fullscreen, panel and
	 * toplevel layers.  This way nothing else can show or receive
	 * input events while we are locked. */

	wl_list_remove(&shell->panel_layer.link);
	wl_list_remove(&shell->toplevel_layer.link);
	wl_list_remove(&shell->fullscreen_layer.link);
	wl_list_insert(&shell->compositor->cursor_layer.link,
		       &shell->lock_layer.link);

	launch_screensaver(shell);

	wl_list_for_each(shsurf, &shell->screensaver.surfaces, link)
		show_screensaver(shell, shsurf);

	if (!wl_list_empty(&shell->screensaver.surfaces)) {
		shell->compositor->idle_time = shell->screensaver.duration;
		weston_compositor_wake(shell->compositor);
		shell->compositor->state = WESTON_COMPOSITOR_IDLE;
	}

	/* reset pointer foci */
	weston_compositor_schedule_repaint(shell->compositor);

	/* reset keyboard foci */
	wl_list_for_each(device, &shell->compositor->input_device_list, link) {
		wl_input_device_set_keyboard_focus(&device->input_device,
						   NULL);
	}

	/* TODO: disable bindings that should not work while locked. */

	/* All this must be undone in resume_desktop(). */
}

static void
unlock(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, unlock_listener);

	if (!shell->locked || shell->lock_surface) {
		weston_compositor_wake(shell->compositor);
		return;
	}

	/* If desktop-shell client has gone away, unlock immediately. */
	if (!shell->child.desktop_shell) {
		resume_desktop(shell);
		return;
	}

	if (shell->prepare_event_sent)
		return;

	desktop_shell_send_prepare_lock_surface(shell->child.desktop_shell);
	shell->prepare_event_sent = true;
}

static void
center_on_output(struct weston_surface *surface, struct weston_output *output)
{
	struct weston_mode *mode = output->current;
	GLfloat x = (mode->width - surface->geometry.width) / 2;
	GLfloat y = (mode->height - surface->geometry.height) / 2;

	weston_surface_set_position(surface, output->x + x, output->y + y);
}

static void
map(struct desktop_shell *shell, struct weston_surface *surface,
    int32_t width, int32_t height, int32_t sx, int32_t sy)
{
	struct weston_compositor *compositor = shell->compositor;
	struct shell_surface *shsurf;
	enum shell_surface_type surface_type = SHELL_SURFACE_NONE;
	struct weston_surface *parent;
	int panel_height = 0;

	shsurf = get_shell_surface(surface);
	if (shsurf)
		surface_type = shsurf->type;

	surface->geometry.width = width;
	surface->geometry.height = height;
	surface->geometry.dirty = 1;

	/* initial positioning, see also configure() */
	switch (surface_type) {
	case SHELL_SURFACE_TOPLEVEL:
		weston_surface_set_position(surface, 10 + random() % 400,
					    10 + random() % 400);
		break;
	case SHELL_SURFACE_SCREENSAVER:
		center_on_output(surface, shsurf->fullscreen_output);
		break;
	case SHELL_SURFACE_FULLSCREEN:
		shell_map_fullscreen(shsurf);
		break;
	case SHELL_SURFACE_MAXIMIZED:
		/* use surface configure to set the geometry */
		panel_height = get_output_panel_height(shell,surface->output);
		weston_surface_set_position(surface, surface->output->x,
					    surface->output->y + panel_height);
		break;
	case SHELL_SURFACE_LOCK:
		center_on_output(surface, get_default_output(compositor));
		break;
	case SHELL_SURFACE_POPUP:
		shell_map_popup(shsurf);
	case SHELL_SURFACE_NONE:
		weston_surface_set_position(surface,
					    surface->geometry.x + sx,
					    surface->geometry.y + sy);
		break;
	default:
		;
	}

	/* surface stacking order, see also activate() */
	switch (surface_type) {
	case SHELL_SURFACE_BACKGROUND:
		/* background always visible, at the bottom */
		wl_list_insert(&shell->background_layer.surface_list,
			       &surface->layer_link);
		break;
	case SHELL_SURFACE_PANEL:
		/* panel always on top, hidden while locked */
		wl_list_insert(&shell->panel_layer.surface_list,
			       &surface->layer_link);
		break;
	case SHELL_SURFACE_LOCK:
		/* lock surface always visible, on top */
		wl_list_insert(&shell->lock_layer.surface_list,
			       &surface->layer_link);
		weston_compositor_wake(compositor);
		break;
	case SHELL_SURFACE_SCREENSAVER:
		/* If locked, show it. */
		if (shell->locked) {
			show_screensaver(shell, shsurf);
			compositor->idle_time = shell->screensaver.duration;
			weston_compositor_wake(compositor);
			if (!shell->lock_surface)
				compositor->state = WESTON_COMPOSITOR_IDLE;
		}
		break;
	case SHELL_SURFACE_POPUP:
	case SHELL_SURFACE_TRANSIENT:
		parent = shsurf->parent->surface;
		wl_list_insert(parent->layer_link.prev, &surface->layer_link);
		break;
	case SHELL_SURFACE_FULLSCREEN:
	case SHELL_SURFACE_NONE:
		break;
	default:
		wl_list_insert(&shell->toplevel_layer.surface_list,
			       &surface->layer_link); 
		break;
	}

	if (surface_type != SHELL_SURFACE_NONE) {
		weston_surface_assign_output(surface);
		if (surface_type == SHELL_SURFACE_MAXIMIZED)
			surface->output = shsurf->output;
	}

	switch (surface_type) {
	case SHELL_SURFACE_TOPLEVEL:
	case SHELL_SURFACE_TRANSIENT:
	case SHELL_SURFACE_FULLSCREEN:
	case SHELL_SURFACE_MAXIMIZED:
		if (!shell->locked)
			activate(shell, surface,
				 (struct weston_input_device *)
				 compositor->input_device);
		break;
	default:
		break;
	}

	if (surface_type == SHELL_SURFACE_TOPLEVEL)
		weston_zoom_run(surface, 0.8, 1.0, NULL, NULL);
}

static void
configure(struct desktop_shell *shell, struct weston_surface *surface,
	  GLfloat x, GLfloat y, int32_t width, int32_t height)
{
	enum shell_surface_type surface_type = SHELL_SURFACE_NONE;
	struct shell_surface *shsurf;

	shsurf = get_shell_surface(surface);
	if (shsurf)
		surface_type = shsurf->type;

	surface->geometry.x = x;
	surface->geometry.y = y;
	surface->geometry.width = width;
	surface->geometry.height = height;
	surface->geometry.dirty = 1;

	switch (surface_type) {
	case SHELL_SURFACE_SCREENSAVER:
		center_on_output(surface, shsurf->fullscreen_output);
		break;
	case SHELL_SURFACE_FULLSCREEN:
		shell_stack_fullscreen(shsurf);
		shell_configure_fullscreen(shsurf);
		break;
	case SHELL_SURFACE_MAXIMIZED:
		/* setting x, y and using configure to change that geometry */
		surface->geometry.x = surface->output->x;
		surface->geometry.y = surface->output->y +
			get_output_panel_height(shell,surface->output);
		break;
	case SHELL_SURFACE_TOPLEVEL:
		break;
	default:
		break;
	}

	/* XXX: would a fullscreen surface need the same handling? */
	if (surface->output) {
		weston_surface_assign_output(surface);

		if (surface_type == SHELL_SURFACE_SCREENSAVER)
			surface->output = shsurf->output;
		else if (surface_type == SHELL_SURFACE_MAXIMIZED)
			surface->output = shsurf->output;
	}
}

static void
shell_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
	struct shell_surface *shsurf = get_shell_surface(es);
	struct desktop_shell *shell = shsurf->shell;

	if (!weston_surface_is_mapped(es)) {
		map(shell, es, es->buffer->width, es->buffer->height, sx, sy);
	} else if (shsurf->force_configure || sx != 0 || sy != 0 ||
		   es->geometry.width != es->buffer->width ||
		   es->geometry.height != es->buffer->height) {
		GLfloat from_x, from_y;
		GLfloat to_x, to_y;

		weston_surface_to_global_float(es, 0, 0, &from_x, &from_y);
		weston_surface_to_global_float(es, sx, sy, &to_x, &to_y);
		configure(shell, es,
			  es->geometry.x + to_x - from_x,
			  es->geometry.y + to_y - from_y,
			  es->buffer->width, es->buffer->height);
		shsurf->force_configure = 0;
	}
}

static int launch_desktop_shell_process(struct desktop_shell *shell);

static void
desktop_shell_sigchld(struct weston_process *process, int status)
{
	uint32_t time;
	struct desktop_shell *shell =
		container_of(process, struct desktop_shell, child.process);

	shell->child.process.pid = 0;
	shell->child.client = NULL; /* already destroyed by wayland */

	/* if desktop-shell dies more than 5 times in 30 seconds, give up */
	time = weston_compositor_get_time();
	if (time - shell->child.deathstamp > 30000) {
		shell->child.deathstamp = time;
		shell->child.deathcount = 0;
	}

	shell->child.deathcount++;
	if (shell->child.deathcount > 5) {
		fprintf(stderr, "weston-desktop-shell died, giving up.\n");
		return;
	}

	fprintf(stderr, "weston-desktop-shell died, respawning...\n");
	launch_desktop_shell_process(shell);
}

static int
launch_desktop_shell_process(struct desktop_shell *shell)
{
	const char *shell_exe = LIBEXECDIR "/weston-desktop-shell";

	shell->child.client = weston_client_launch(shell->compositor,
						 &shell->child.process,
						 shell_exe,
						 desktop_shell_sigchld);

	if (!shell->child.client)
		return -1;
	return 0;
}

static void
bind_shell(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;

	wl_client_add_object(client, &wl_shell_interface,
			     &shell_implementation, id, shell);
}

static void
unbind_desktop_shell(struct wl_resource *resource)
{
	struct desktop_shell *shell = resource->data;

	if (shell->locked)
		resume_desktop(shell);

	shell->child.desktop_shell = NULL;
	shell->prepare_event_sent = false;
	free(resource);
}

static void
bind_desktop_shell(struct wl_client *client,
		   void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &desktop_shell_interface,
					&desktop_shell_implementation,
					id, shell);

	if (client == shell->child.client) {
		resource->destroy = unbind_desktop_shell;
		shell->child.desktop_shell = resource;
		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "permission to bind desktop_shell denied");
	wl_resource_destroy(resource);
}

static void
screensaver_set_surface(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *shell_surface_resource,
			struct wl_resource *output_resource)
{
	struct desktop_shell *shell = resource->data;
	struct shell_surface *surface = shell_surface_resource->data;
	struct weston_output *output = output_resource->data;

	if (reset_shell_surface_type(surface))
		return;

	surface->type = SHELL_SURFACE_SCREENSAVER;

	surface->fullscreen_output = output;
	surface->output = output;
	wl_list_insert(shell->screensaver.surfaces.prev, &surface->link);
}

static const struct screensaver_interface screensaver_implementation = {
	screensaver_set_surface
};

static void
unbind_screensaver(struct wl_resource *resource)
{
	struct desktop_shell *shell = resource->data;

	shell->screensaver.binding = NULL;
	free(resource);
}

static void
bind_screensaver(struct wl_client *client,
		 void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &screensaver_interface,
					&screensaver_implementation,
					id, shell);

	if (shell->screensaver.binding == NULL) {
		resource->destroy = unbind_screensaver;
		shell->screensaver.binding = resource;
		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "interface object already bound");
	wl_resource_destroy(resource);
}

struct switcher {
	struct desktop_shell *shell;
	struct weston_surface *current;
	struct wl_listener listener;
	struct wl_keyboard_grab grab;
};

static void
switcher_next(struct switcher *switcher)
{
	struct weston_compositor *compositor = switcher->shell->compositor;
	struct weston_surface *surface;
	struct weston_surface *first = NULL, *prev = NULL, *next = NULL;
	struct shell_surface *shsurf;

	wl_list_for_each(surface, &compositor->surface_list, link) {
		switch (get_shell_surface_type(surface)) {
		case SHELL_SURFACE_TOPLEVEL:
		case SHELL_SURFACE_FULLSCREEN:
		case SHELL_SURFACE_MAXIMIZED:
			if (first == NULL)
				first = surface;
			if (prev == switcher->current)
				next = surface;
			prev = surface;
			surface->alpha = 64;
			surface->geometry.dirty = 1;
			weston_surface_damage(surface);
			break;
		default:
			break;
		}

		if (is_black_surface(surface, NULL)) {
			surface->alpha = 64;
			surface->geometry.dirty = 1;
			weston_surface_damage(surface);
		}
	}

	if (next == NULL)
		next = first;

	if (next == NULL)
		return;

	wl_list_remove(&switcher->listener.link);
	wl_signal_add(&next->surface.resource.destroy_signal,
		      &switcher->listener);

	switcher->current = next;
	next->alpha = 255;

	shsurf = get_shell_surface(switcher->current);
	if (shsurf && shsurf->type ==SHELL_SURFACE_FULLSCREEN)
		shsurf->fullscreen.black_surface->alpha = 255;
}

static void
switcher_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct switcher *switcher =
		container_of(listener, struct switcher, listener);

	switcher_next(switcher);
}

static void
switcher_destroy(struct switcher *switcher, uint32_t time)
{
	struct weston_compositor *compositor = switcher->shell->compositor;
	struct weston_surface *surface;
	struct weston_input_device *device =
		(struct weston_input_device *) switcher->grab.input_device;

	wl_list_for_each(surface, &compositor->surface_list, link) {
		surface->alpha = 255;
		weston_surface_damage(surface);
	}

	if (switcher->current)
		activate(switcher->shell, switcher->current, device);
	wl_list_remove(&switcher->listener.link);
	wl_input_device_end_keyboard_grab(&device->input_device);
	free(switcher);
}

static void
switcher_key(struct wl_keyboard_grab *grab,
	     uint32_t time, uint32_t key, int32_t state)
{
	struct switcher *switcher = container_of(grab, struct switcher, grab);
	struct weston_input_device *device =
		(struct weston_input_device *) grab->input_device;

	if ((device->modifier_state & switcher->shell->binding_modifier) == 0) {
		switcher_destroy(switcher, time);
	} else if (key == KEY_TAB && state) {
		switcher_next(switcher);
	}
};

static const struct wl_keyboard_grab_interface switcher_grab = {
	switcher_key
};

static void
switcher_binding(struct wl_input_device *device, uint32_t time,
		 uint32_t key, uint32_t button, uint32_t axis,
		 int32_t state, void *data)
{
	struct desktop_shell *shell = data;
	struct switcher *switcher;

	switcher = malloc(sizeof *switcher);
	switcher->shell = shell;
	switcher->current = NULL;
	switcher->listener.notify = switcher_handle_surface_destroy;
	wl_list_init(&switcher->listener.link);

	switcher->grab.interface = &switcher_grab;
	wl_input_device_start_keyboard_grab(device, &switcher->grab);
	wl_input_device_set_keyboard_focus(device, NULL);
	switcher_next(switcher);
}

static void
backlight_binding(struct wl_input_device *device, uint32_t time,
		  uint32_t key, uint32_t button, uint32_t axis, int32_t state, void *data)
{
	struct weston_compositor *compositor = data;
	struct weston_output *output;
	long backlight_new = 0;

	/* TODO: we're limiting to simple use cases, where we assume just
	 * control on the primary display. We'd have to extend later if we
	 * ever get support for setting backlights on random desktop LCD
	 * panels though */
	output = get_default_output(compositor);
	if (!output)
		return;

	if (!output->set_backlight)
		return;

	if (key == KEY_F9 || key == KEY_BRIGHTNESSDOWN)
		backlight_new = output->backlight_current - 25;
	else if (key == KEY_F10 || key == KEY_BRIGHTNESSUP)
		backlight_new = output->backlight_current + 25;

	if (backlight_new < 5)
		backlight_new = 5;
	if (backlight_new > 255)
		backlight_new = 255;

	output->backlight_current = backlight_new;
	output->set_backlight(output, output->backlight_current);
}

static void
debug_repaint_binding(struct wl_input_device *device, uint32_t time,
		      uint32_t key, uint32_t button, uint32_t axis, int32_t state, void *data)
{
	struct desktop_shell *shell = data;
	struct weston_compositor *compositor = shell->compositor;
	struct weston_surface *surface;

	if (shell->debug_repaint_surface) {
		weston_surface_destroy(shell->debug_repaint_surface);
		shell->debug_repaint_surface = NULL;
	} else {
		surface = weston_surface_create(compositor);
		weston_surface_set_color(surface, 1.0, 0.0, 0.0, 0.2);
		weston_surface_configure(surface, 0, 0, 8192, 8192);
		wl_list_insert(&compositor->fade_layer.surface_list,
			       &surface->layer_link);
		weston_surface_assign_output(surface);
		pixman_region32_init(&surface->input);

		/* Here's the dirty little trick that makes the
		 * repaint debugging work: we force an
		 * update_transform first to update dependent state
		 * and clear the geometry.dirty bit.  Then we clear
		 * the surface damage so it only gets repainted
		 * piecewise as we repaint other things.  */

		weston_surface_update_transform(surface);
		pixman_region32_fini(&surface->damage);
		pixman_region32_init(&surface->damage);
		shell->debug_repaint_surface = surface;
	}
}

static void
shell_destroy(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, destroy_listener);

	if (shell->child.client)
		wl_client_destroy(shell->child.client);

	free(shell->screensaver.path);
	free(shell);
}

static void
shell_add_bindings(struct weston_compositor *ec, struct desktop_shell *shell)
{
	uint32_t mod;

	/* fixed bindings */
	weston_compositor_add_binding(ec, KEY_BACKSPACE, 0, 0,
				      MODIFIER_CTRL | MODIFIER_ALT,
				      terminate_binding, ec);
	weston_compositor_add_binding(ec, 0, BTN_LEFT, 0, 0,
				      click_to_activate_binding, shell);
	weston_compositor_add_binding(ec, 0, 0,
				      WL_INPUT_DEVICE_AXIS_VERTICAL_SCROLL,
				      MODIFIER_SUPER | MODIFIER_ALT,
				      surface_opacity_binding, NULL);
	weston_compositor_add_binding(ec, 0, 0,
				      WL_INPUT_DEVICE_AXIS_VERTICAL_SCROLL,
				      MODIFIER_SUPER, zoom_binding, NULL);

	/* configurable bindings */
	mod = shell->binding_modifier;
	weston_compositor_add_binding(ec, 0, BTN_LEFT, 0, mod,
				      move_binding, shell);
	weston_compositor_add_binding(ec, 0, BTN_MIDDLE, 0, mod,
				      resize_binding, shell);
	weston_compositor_add_binding(ec, 0, BTN_RIGHT, 0, mod,
				      rotate_binding, NULL);
	weston_compositor_add_binding(ec, KEY_TAB, 0, 0, mod,
				      switcher_binding, shell);
	weston_compositor_add_binding(ec, KEY_F9, 0, 0, mod,
				      backlight_binding, ec);
	weston_compositor_add_binding(ec, KEY_BRIGHTNESSDOWN, 0, 0, 0,
				      backlight_binding, ec);
	weston_compositor_add_binding(ec, KEY_F10, 0, 0, mod,
				      backlight_binding, ec);
	weston_compositor_add_binding(ec, KEY_BRIGHTNESSUP, 0, 0, 0,
				      backlight_binding, ec);
	weston_compositor_add_binding(ec, KEY_SPACE, 0, 0, mod,
				      debug_repaint_binding, shell);
}

int
shell_init(struct weston_compositor *ec);

WL_EXPORT int
shell_init(struct weston_compositor *ec)
{
	struct desktop_shell *shell;

	shell = malloc(sizeof *shell);
	if (shell == NULL)
		return -1;

	memset(shell, 0, sizeof *shell);
	shell->compositor = ec;

	shell->destroy_listener.notify = shell_destroy;
	wl_signal_add(&ec->destroy_signal, &shell->destroy_listener);
	shell->lock_listener.notify = lock;
	wl_signal_add(&ec->lock_signal, &shell->lock_listener);
	shell->unlock_listener.notify = unlock;
	wl_signal_add(&ec->unlock_signal, &shell->unlock_listener);
	ec->ping_handler = ping_handler;
	ec->shell_interface.create_shell_surface = create_shell_surface;
	ec->shell_interface.set_toplevel = set_toplevel;

	wl_list_init(&shell->backgrounds);
	wl_list_init(&shell->panels);
	wl_list_init(&shell->screensaver.surfaces);

	weston_layer_init(&shell->fullscreen_layer, &ec->cursor_layer.link);
	weston_layer_init(&shell->panel_layer, &shell->fullscreen_layer.link);
	weston_layer_init(&shell->toplevel_layer, &shell->panel_layer.link);
	weston_layer_init(&shell->background_layer,
			  &shell->toplevel_layer.link);
	wl_list_init(&shell->lock_layer.surface_list);

	shell_configuration(shell);

	if (wl_display_add_global(ec->wl_display, &wl_shell_interface,
				  shell, bind_shell) == NULL)
		return -1;

	if (wl_display_add_global(ec->wl_display,
				  &desktop_shell_interface,
				  shell, bind_desktop_shell) == NULL)
		return -1;

	if (wl_display_add_global(ec->wl_display, &screensaver_interface,
				  shell, bind_screensaver) == NULL)
		return -1;

	shell->child.deathstamp = weston_compositor_get_time();
	if (launch_desktop_shell_process(shell) != 0)
		return -1;

	shell_add_bindings(ec, shell);

	return 0;
}
