/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2011 Collabora, Ltd.
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

#include <wayland-server.h>
#include "compositor.h"
#include "desktop-shell-server-protocol.h"
#include "../shared/config-parser.h"

struct shell_surface;

struct wl_shell {
	struct weston_compositor *compositor;
	struct weston_shell shell;

	struct {
		struct weston_process process;
		struct wl_client *client;
		struct wl_resource *desktop_shell;
	} child;

	bool locked;
	bool prepare_event_sent;

	struct shell_surface *lock_surface;
	struct wl_listener lock_surface_listener;
	struct wl_list hidden_surface_list;

	struct wl_list backgrounds;
	struct wl_list panels;

	struct {
		const char *path;
		int duration;
		struct wl_resource *binding;
		struct wl_list surfaces;
		struct weston_process process;
	} screensaver;
};

enum shell_surface_type {
	SHELL_SURFACE_NONE,

	SHELL_SURFACE_PANEL,
	SHELL_SURFACE_BACKGROUND,
	SHELL_SURFACE_LOCK,
	SHELL_SURFACE_SCREENSAVER,

	SHELL_SURFACE_TOPLEVEL,
	SHELL_SURFACE_TRANSIENT,
	SHELL_SURFACE_FULLSCREEN
};

struct shell_surface {
	struct wl_resource resource;

	struct weston_surface *surface;
	struct wl_listener surface_destroy_listener;

	enum shell_surface_type type;
	int32_t saved_x, saved_y;

	struct weston_output *output;
	struct wl_list link;
};

struct weston_move_grab {
	struct wl_grab grab;
	struct weston_surface *surface;
	int32_t dx, dy;
};

static int
shell_configuration(struct wl_shell *shell)
{
	int ret;
	char *config_file;
	char *path = NULL;
	int duration = 60;

	struct config_key saver_keys[] = {
		{ "path",       CONFIG_KEY_STRING,  &path },
		{ "duration",   CONFIG_KEY_INTEGER, &duration },
	};

	struct config_section cs[] = {
		{ "screensaver", saver_keys, ARRAY_LENGTH(saver_keys), NULL },
	};

	config_file = config_file_path("wayland-desktop-shell.ini");
	ret = parse_config_file(config_file, cs, ARRAY_LENGTH(cs), shell);
	free(config_file);

	shell->screensaver.path = path;
	shell->screensaver.duration = duration;

	return ret;
	/* FIXME: free(shell->screensaver.path) on plugin fini */
}

static void
move_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	struct weston_move_grab *move = (struct weston_move_grab *) grab;
	struct weston_surface *es = move->surface;

	weston_surface_configure(es, x + move->dx, y + move->dy,
			       es->width, es->height);
}

static void
move_grab_button(struct wl_grab *grab,
		 uint32_t time, int32_t button, int32_t state)
{
}

static void
move_grab_end(struct wl_grab *grab, uint32_t time)
{
	free(grab);
}

static const struct wl_grab_interface move_grab_interface = {
	move_grab_motion,
	move_grab_button,
	move_grab_end
};

static int
weston_surface_move(struct weston_surface *es,
		  struct weston_input_device *wd, uint32_t time)
{
	struct weston_move_grab *move;

	move = malloc(sizeof *move);
	if (!move)
		return -1;

	move->grab.interface = &move_grab_interface;
	move->dx = es->x - wd->input_device.grab_x;
	move->dy = es->y - wd->input_device.grab_y;
	move->surface = es;

	if (wl_input_device_update_grab(&wd->input_device,
					&move->grab, &es->surface, time) < 0)
		return 0;

	wl_input_device_set_pointer_focus(&wd->input_device,
					  NULL, time, 0, 0, 0, 0);

	return 0;
}

static void
shell_surface_move(struct wl_client *client, struct wl_resource *resource,
		   struct wl_resource *input_resource, uint32_t time)
{
	struct weston_input_device *wd = input_resource->data;
	struct shell_surface *shsurf = resource->data;

	if (weston_surface_move(shsurf->surface, wd, time) < 0)
		wl_resource_post_no_memory(resource);
}

struct weston_resize_grab {
	struct wl_grab grab;
	uint32_t edges;
	int32_t dx, dy, width, height;
	struct shell_surface *shsurf;
};

static void
resize_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	struct weston_resize_grab *resize = (struct weston_resize_grab *) grab;
	struct wl_input_device *device = grab->input_device;
	int32_t width, height;

	if (resize->edges & WL_SHELL_SURFACE_RESIZE_LEFT) {
		width = device->grab_x - x + resize->width;
	} else if (resize->edges & WL_SHELL_SURFACE_RESIZE_RIGHT) {
		width = x - device->grab_x + resize->width;
	} else {
		width = resize->width;
	}

	if (resize->edges & WL_SHELL_SURFACE_RESIZE_TOP) {
		height = device->grab_y - y + resize->height;
	} else if (resize->edges & WL_SHELL_SURFACE_RESIZE_BOTTOM) {
		height = y - device->grab_y + resize->height;
	} else {
		height = resize->height;
	}

	wl_resource_post_event(&resize->shsurf->resource,
			       WL_SHELL_SURFACE_CONFIGURE, time, resize->edges,
			       width, height);
}

static void
resize_grab_button(struct wl_grab *grab,
		   uint32_t time, int32_t button, int32_t state)
{
}

static void
resize_grab_end(struct wl_grab *grab, uint32_t time)
{
	free(grab);
}

static const struct wl_grab_interface resize_grab_interface = {
	resize_grab_motion,
	resize_grab_button,
	resize_grab_end
};

static int
weston_surface_resize(struct shell_surface *shsurf,
		    struct weston_input_device *wd,
		    uint32_t time, uint32_t edges)
{
	struct weston_resize_grab *resize;
	struct weston_surface *es = shsurf->surface;

	/* FIXME: Reject if fullscreen */

	resize = malloc(sizeof *resize);
	if (!resize)
		return -1;

	resize->grab.interface = &resize_grab_interface;
	resize->edges = edges;
	resize->dx = es->x - wd->input_device.grab_x;
	resize->dy = es->y - wd->input_device.grab_y;
	resize->width = es->width;
	resize->height = es->height;
	resize->shsurf = shsurf;

	if (edges == 0 || edges > 15 ||
	    (edges & 3) == 3 || (edges & 12) == 12)
		return 0;

	if (wl_input_device_update_grab(&wd->input_device,
					&resize->grab, &es->surface, time) < 0)
		return 0;

	wl_input_device_set_pointer_focus(&wd->input_device,
					  NULL, time, 0, 0, 0, 0);

	return 0;
}

static void
shell_surface_resize(struct wl_client *client, struct wl_resource *resource,
		     struct wl_resource *input_resource, uint32_t time,
		     uint32_t edges)
{
	struct weston_input_device *wd = input_resource->data;
	struct shell_surface *shsurf = resource->data;

	/* FIXME: Reject if fullscreen */

	if (weston_surface_resize(shsurf, wd, time, edges) < 0)
		wl_resource_post_no_memory(resource);
}

static int
reset_shell_surface_type(struct shell_surface *surface)
{
	switch (surface->type) {
	case SHELL_SURFACE_FULLSCREEN:
		surface->surface->x = surface->saved_x;
		surface->surface->y = surface->saved_y;
		surface->surface->fullscreen_output = NULL;
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
		break;
	}

	surface->type = SHELL_SURFACE_NONE;
	return 0;
}

static void
shell_surface_set_toplevel(struct wl_client *client,
			   struct wl_resource *resource)

{
	struct shell_surface *surface = resource->data;

	if (reset_shell_surface_type(surface))
		return;

	weston_surface_damage(surface->surface);
	surface->type = SHELL_SURFACE_TOPLEVEL;
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

	/* assign to parents output  */
	es->output = pes->output;
 
	es->x = pes->x + x;
	es->y = pes->y + y;

	weston_surface_damage(es);
	shsurf->type = SHELL_SURFACE_TRANSIENT;
}

static struct weston_output *
get_default_output(struct weston_compositor *compositor)
{
	return container_of(compositor->output_list.next,
			    struct weston_output, link);
}

static void
shell_surface_set_fullscreen(struct wl_client *client,
			     struct wl_resource *resource)

{
	struct shell_surface *shsurf = resource->data;
	struct weston_surface *es = shsurf->surface;
	struct weston_output *output;

	if (reset_shell_surface_type(shsurf))
		return;

	/* FIXME: Fullscreen on first output */
	/* FIXME: Handle output going away */
	output = get_default_output(es->compositor);
	es->output = output;

	shsurf->saved_x = es->x;
	shsurf->saved_y = es->y;
	es->x = (output->current->width - es->width) / 2;
	es->y = (output->current->height - es->height) / 2;
	es->fullscreen_output = output;
	weston_surface_damage(es);
	shsurf->type = SHELL_SURFACE_FULLSCREEN;
}

static const struct wl_shell_surface_interface shell_surface_implementation = {
	shell_surface_move,
	shell_surface_resize,
	shell_surface_set_toplevel,
	shell_surface_set_transient,
	shell_surface_set_fullscreen
};

static void
destroy_shell_surface(struct wl_resource *resource)
{
	struct shell_surface *shsurf = resource->data;

	/* in case cleaning up a dead client destroys shell_surface first */
	if (shsurf->surface)
		wl_list_remove(&shsurf->surface_destroy_listener.link);

	wl_list_remove(&shsurf->link);
	free(shsurf);
}

static void
shell_handle_surface_destroy(struct wl_listener *listener,
			     struct wl_resource *resource, uint32_t time)
{
	struct shell_surface *shsurf = container_of(listener,
						    struct shell_surface,
						    surface_destroy_listener);

	shsurf->surface = NULL;
	wl_resource_destroy(&shsurf->resource, time);
}

static struct shell_surface *
get_shell_surface(struct weston_surface *surface)
{
	struct wl_list *lst = &surface->surface.resource.destroy_listener_list;
	struct wl_listener *listener;

	/* search the destroy listener list for our callback */
	wl_list_for_each(listener, lst, link) {
		if (listener->func == shell_handle_surface_destroy) {
			return container_of(listener, struct shell_surface,
					    surface_destroy_listener);
		}
	}

	return NULL;
}

static void
shell_get_shell_surface(struct wl_client *client,
			struct wl_resource *resource,
			uint32_t id,
			struct wl_resource *surface_resource)
{
	struct weston_surface *surface = surface_resource->data;
	struct shell_surface *shsurf;

	if (get_shell_surface(surface)) {
		wl_resource_post_error(surface_resource,
			WL_DISPLAY_ERROR_INVALID_OBJECT,
			"wl_shell::get_shell_surface already requested");
		return;
	}

	shsurf = calloc(1, sizeof *shsurf);
	if (!shsurf) {
		wl_resource_post_no_memory(resource);
		return;
	}

	shsurf->resource.destroy = destroy_shell_surface;
	shsurf->resource.object.id = id;
	shsurf->resource.object.interface = &wl_shell_surface_interface;
	shsurf->resource.object.implementation =
		(void (**)(void)) &shell_surface_implementation;
	shsurf->resource.data = shsurf;

	shsurf->surface = surface;
	shsurf->surface_destroy_listener.func = shell_handle_surface_destroy;
	wl_list_insert(surface->surface.resource.destroy_listener_list.prev,
		       &shsurf->surface_destroy_listener.link);

	/* init link so its safe to always remove it in destroy_shell_surface */
	wl_list_init(&shsurf->link);

	shsurf->type = SHELL_SURFACE_NONE;

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
launch_screensaver(struct wl_shell *shell)
{
	if (shell->screensaver.binding)
		return;

	if (!shell->screensaver.path)
		return;

	weston_client_launch(shell->compositor,
			   &shell->screensaver.process,
			   shell->screensaver.path,
			   handle_screensaver_sigchld);
}

static void
terminate_screensaver(struct wl_shell *shell)
{
	if (shell->screensaver.process.pid == 0)
		return;

	kill(shell->screensaver.process.pid, SIGTERM);
}

static void
show_screensaver(struct wl_shell *shell, struct shell_surface *surface)
{
	struct wl_list *list;

	if (shell->lock_surface)
		list = &shell->lock_surface->surface->link;
	else
		list = &shell->compositor->surface_list;

	wl_list_remove(&surface->surface->link);
	wl_list_insert(list, &surface->surface->link);
	weston_surface_configure(surface->surface,
			       surface->surface->x,
			       surface->surface->y,
			       surface->surface->width,
			       surface->surface->height);
	surface->surface->output = surface->output;
}

static void
hide_screensaver(struct wl_shell *shell, struct shell_surface *surface)
{
	wl_list_remove(&surface->surface->link);
	wl_list_init(&surface->surface->link);
	surface->surface->output = NULL;
}

static void
desktop_shell_set_background(struct wl_client *client,
			     struct wl_resource *resource,
			     struct wl_resource *output_resource,
			     struct wl_resource *surface_resource)
{
	struct wl_shell *shell = resource->data;
	struct shell_surface *shsurf = surface_resource->data;
	struct weston_surface *surface = shsurf->surface;
	struct shell_surface *priv;

	if (reset_shell_surface_type(shsurf))
		return;

	wl_list_for_each(priv, &shell->backgrounds, link) {
		if (priv->output == output_resource->data) {
			priv->surface->output = NULL;
			wl_list_remove(&priv->surface->link);
			wl_list_remove(&priv->link);
			break;
		}
	}

	shsurf->type = SHELL_SURFACE_BACKGROUND;
	shsurf->output = output_resource->data;

	wl_list_insert(&shell->backgrounds, &shsurf->link);

	surface->x = shsurf->output->x;
	surface->y = shsurf->output->y;

	wl_resource_post_event(resource,
			       DESKTOP_SHELL_CONFIGURE,
			       weston_compositor_get_time(), 0, surface_resource,
			       shsurf->output->current->width,
			       shsurf->output->current->height);
}

static void
desktop_shell_set_panel(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *output_resource,
			struct wl_resource *surface_resource)
{
	struct wl_shell *shell = resource->data;
	struct shell_surface *shsurf = surface_resource->data;
	struct weston_surface *surface = shsurf->surface;
	struct shell_surface *priv;

	if (reset_shell_surface_type(shsurf))
		return;

	wl_list_for_each(priv, &shell->panels, link) {
		if (priv->output == output_resource->data) {
			priv->surface->output = NULL;
			wl_list_remove(&priv->surface->link);
			wl_list_remove(&priv->link);
			break;
		}
	}

	shsurf->type = SHELL_SURFACE_PANEL;
	shsurf->output = output_resource->data;

	wl_list_insert(&shell->panels, &shsurf->link);

	surface->x = shsurf->output->x;
	surface->y = shsurf->output->y;

	wl_resource_post_event(resource,
			       DESKTOP_SHELL_CONFIGURE,
			       weston_compositor_get_time(), 0, surface_resource,
			       shsurf->output->current->width,
			       shsurf->output->current->height);
}

static void
handle_lock_surface_destroy(struct wl_listener *listener,
			    struct wl_resource *resource, uint32_t time)
{
	struct wl_shell *shell =
		container_of(listener, struct wl_shell, lock_surface_listener);

	fprintf(stderr, "lock surface gone\n");
	shell->lock_surface = NULL;
}

static void
desktop_shell_set_lock_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource)
{
	struct wl_shell *shell = resource->data;
	struct shell_surface *surface = surface_resource->data;

	if (reset_shell_surface_type(surface))
		return;

	shell->prepare_event_sent = false;

	if (!shell->locked)
		return;

	shell->lock_surface = surface;

	shell->lock_surface_listener.func = handle_lock_surface_destroy;
	wl_list_insert(&surface_resource->destroy_listener_list,
		       &shell->lock_surface_listener.link);

	shell->lock_surface->type = SHELL_SURFACE_LOCK;
}

static void
resume_desktop(struct wl_shell *shell)
{
	struct weston_surface *surface;
	struct wl_list *list;
	struct shell_surface *tmp;

	wl_list_for_each(tmp, &shell->screensaver.surfaces, link)
		hide_screensaver(shell, tmp);

	terminate_screensaver(shell);

	wl_list_for_each(surface, &shell->hidden_surface_list, link)
		weston_surface_configure(surface, surface->x, surface->y,
				       surface->width, surface->height);

	if (wl_list_empty(&shell->backgrounds)) {
		list = &shell->compositor->surface_list;
	} else {
		struct shell_surface *background;
		background = container_of(shell->backgrounds.prev,
					  struct shell_surface, link);
		list = background->surface->link.prev;
	}

	if (!wl_list_empty(&shell->hidden_surface_list))
		wl_list_insert_list(list, &shell->hidden_surface_list);
	wl_list_init(&shell->hidden_surface_list);

	shell->locked = false;
	weston_compositor_repick(shell->compositor);
	shell->compositor->idle_time = shell->compositor->option_idle_time;
	weston_compositor_wake(shell->compositor);
}

static void
desktop_shell_unlock(struct wl_client *client,
		     struct wl_resource *resource)
{
	struct wl_shell *shell = resource->data;

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
	     uint32_t key, uint32_t button, uint32_t state, void *data)
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

	weston_surface_move(surface, (struct weston_input_device *) device, time);
}

static void
resize_binding(struct wl_input_device *device, uint32_t time,
	       uint32_t key, uint32_t button, uint32_t state, void *data)
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

	x = device->grab_x - surface->x;
	y = device->grab_y - surface->y;

	if (x < surface->width / 3)
		edges |= WL_SHELL_SURFACE_RESIZE_LEFT;
	else if (x < 2 * surface->width / 3)
		edges |= 0;
	else
		edges |= WL_SHELL_SURFACE_RESIZE_RIGHT;

	if (y < surface->height / 3)
		edges |= WL_SHELL_SURFACE_RESIZE_TOP;
	else if (y < 2 * surface->height / 3)
		edges |= 0;
	else
		edges |= WL_SHELL_SURFACE_RESIZE_BOTTOM;

	weston_surface_resize(shsurf, (struct weston_input_device *) device,
			    time, edges);
}

static void
terminate_binding(struct wl_input_device *device, uint32_t time,
		  uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct weston_compositor *compositor = data;

	if (state)
		wl_display_terminate(compositor->wl_display);
}

static void
activate(struct weston_shell *base, struct weston_surface *es,
	 struct weston_input_device *device, uint32_t time)
{
	struct wl_shell *shell = container_of(base, struct wl_shell, shell);
	struct weston_compositor *compositor = shell->compositor;

	weston_surface_activate(es, device, time);

	if (compositor->wxs)
		weston_xserver_surface_activate(es);

	switch (get_shell_surface_type(es)) {
	case SHELL_SURFACE_BACKGROUND:
		/* put background back to bottom */
		wl_list_remove(&es->link);
		wl_list_insert(compositor->surface_list.prev, &es->link);
		break;
	case SHELL_SURFACE_PANEL:
		/* already put on top */
		break;
	case SHELL_SURFACE_SCREENSAVER:
		/* always below lock surface */
		if (shell->lock_surface) {
			wl_list_remove(&es->link);
			wl_list_insert(&shell->lock_surface->surface->link,
				       &es->link);
		}
		break;
	default:
		if (!shell->locked) {
			/* bring panel back to top */
			struct shell_surface *panel;
			wl_list_for_each(panel, &shell->panels, link) {
				wl_list_remove(&panel->surface->link);
				wl_list_insert(&compositor->surface_list,
					       &panel->surface->link);
			}
		}
	}
}

static void
click_to_activate_binding(struct wl_input_device *device,
                         uint32_t time, uint32_t key,
			  uint32_t button, uint32_t state, void *data)
{
	struct weston_input_device *wd = (struct weston_input_device *) device;
	struct weston_compositor *compositor = data;
	struct weston_surface *focus;

	focus = (struct weston_surface *) device->pointer_focus;
	if (state && focus && device->grab == NULL)
		activate(compositor->shell, focus, wd, time);
}

static void
lock(struct weston_shell *base)
{
	struct wl_shell *shell = container_of(base, struct wl_shell, shell);
	struct wl_list *surface_list = &shell->compositor->surface_list;
	struct weston_surface *cur;
	struct weston_surface *tmp;
	struct weston_input_device *device;
	struct shell_surface *shsurf;
	uint32_t time;

	if (shell->locked)
		return;

	shell->locked = true;

	/* Move all surfaces from compositor's list to our hidden list,
	 * except the background. This way nothing else can show or
	 * receive input events while we are locked. */

	if (!wl_list_empty(&shell->hidden_surface_list)) {
		fprintf(stderr,
		"%s: Assertion failed: hidden_surface_list is not empty.\n",
								__func__);
	}

	wl_list_for_each_safe(cur, tmp, surface_list, link) {
		/* skip input device sprites, cur->surface is uninitialised */
		if (cur->surface.resource.client == NULL)
			continue;

		if (get_shell_surface_type(cur) == SHELL_SURFACE_BACKGROUND)
			continue;

		cur->output = NULL;
		wl_list_remove(&cur->link);
		wl_list_insert(shell->hidden_surface_list.prev, &cur->link);
	}

	launch_screensaver(shell);

	wl_list_for_each(shsurf, &shell->screensaver.surfaces, link)
		show_screensaver(shell, shsurf);

	if (!wl_list_empty(&shell->screensaver.surfaces)) {
		shell->compositor->idle_time = shell->screensaver.duration;
		weston_compositor_wake(shell->compositor);
		shell->compositor->state = WESTON_COMPOSITOR_IDLE;
	}

	/* reset pointer foci */
	weston_compositor_repick(shell->compositor);

	/* reset keyboard foci */
	time = weston_compositor_get_time();
	wl_list_for_each(device, &shell->compositor->input_device_list, link) {
		wl_input_device_set_keyboard_focus(&device->input_device,
						   NULL, time);
	}

	/* TODO: disable bindings that should not work while locked. */

	/* All this must be undone in resume_desktop(). */
}

static void
unlock(struct weston_shell *base)
{
	struct wl_shell *shell = container_of(base, struct wl_shell, shell);

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

	wl_resource_post_event(shell->child.desktop_shell,
			       DESKTOP_SHELL_PREPARE_LOCK_SURFACE);
	shell->prepare_event_sent = true;
}

static void
center_on_output(struct weston_surface *surface, struct weston_output *output)
{
	struct weston_mode *mode = output->current;

	surface->x = output->x + (mode->width - surface->width) / 2;
	surface->y = output->y + (mode->height - surface->height) / 2;
}

static void
map(struct weston_shell *base,
    struct weston_surface *surface, int32_t width, int32_t height)
{
	struct wl_shell *shell = container_of(base, struct wl_shell, shell);
	struct weston_compositor *compositor = shell->compositor;
	struct wl_list *list;
	struct shell_surface *shsurf;
	enum shell_surface_type surface_type = SHELL_SURFACE_NONE;
	int do_configure;

	shsurf = get_shell_surface(surface);
	if (shsurf)
		surface_type = shsurf->type;

	if (shell->locked) {
		list = &shell->hidden_surface_list;
		do_configure = 0;
	} else {
		list = &compositor->surface_list;
		do_configure = 1;
	}

	surface->width = width;
	surface->height = height;

	/* initial positioning, see also configure() */
	switch (surface_type) {
	case SHELL_SURFACE_TOPLEVEL:
		surface->x = 10 + random() % 400;
		surface->y = 10 + random() % 400;
		break;
	case SHELL_SURFACE_SCREENSAVER:
	case SHELL_SURFACE_FULLSCREEN:
		center_on_output(surface, surface->fullscreen_output);
		break;
	case SHELL_SURFACE_LOCK:
		center_on_output(surface, get_default_output(compositor));
		break;
	default:
		;
	}

	/* surface stacking order, see also activate() */
	switch (surface_type) {
	case SHELL_SURFACE_BACKGROUND:
		/* background always visible, at the bottom */
		wl_list_insert(compositor->surface_list.prev, &surface->link);
		do_configure = 1;
		break;
	case SHELL_SURFACE_PANEL:
		/* panel always on top, hidden while locked */
		wl_list_insert(list, &surface->link);
		break;
	case SHELL_SURFACE_LOCK:
		/* lock surface always visible, on top */
		wl_list_insert(&compositor->surface_list, &surface->link);

		weston_compositor_repick(compositor);
		weston_compositor_wake(compositor);
		do_configure = 1;
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
		do_configure = 0;
		break;
	default:
		/* everything else just below the panel */
		if (!wl_list_empty(&shell->panels)) {
			struct shell_surface *panel =
				container_of(shell->panels.prev,
					     struct shell_surface, link);
			wl_list_insert(&panel->surface->link, &surface->link);
		} else {
			wl_list_insert(list, &surface->link);
		}
	}

	if (do_configure)
		weston_surface_configure(surface,
				       surface->x, surface->y, width, height);

	switch (surface_type) {
	case SHELL_SURFACE_TOPLEVEL:
	case SHELL_SURFACE_TRANSIENT:
	case SHELL_SURFACE_FULLSCREEN:
		if (!shell->locked)
			activate(base, surface,
				 (struct weston_input_device *)
					compositor->input_device,
				 weston_compositor_get_time());
		break;
	default:
		break;
	}

	if (surface_type == SHELL_SURFACE_TOPLEVEL)
		weston_zoom_run(surface, 0.8, 1.0, NULL, NULL);
}

static void
configure(struct weston_shell *base, struct weston_surface *surface,
	  int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wl_shell *shell = container_of(base, struct wl_shell, shell);
	int do_configure = !shell->locked;
	enum shell_surface_type surface_type = SHELL_SURFACE_NONE;
	struct shell_surface *shsurf;

	shsurf = get_shell_surface(surface);
	if (shsurf)
		surface_type = shsurf->type;

	surface->width = width;
	surface->height = height;

	switch (surface_type) {
	case SHELL_SURFACE_SCREENSAVER:
		do_configure = !do_configure;
		/* fall through */
	case SHELL_SURFACE_FULLSCREEN:
		center_on_output(surface, surface->fullscreen_output);
		break;
	default:
		break;
	}

	/*
	 * weston_surface_configure() will assign an output, which means
	 * the surface is supposed to be in compositor->surface_list.
	 * Be careful with that, and make sure we stay on the right output.
	 * XXX: would a fullscreen surface need the same handling?
	 */
	if (do_configure) {
		weston_surface_configure(surface, x, y, width, height);

		if (surface_type == SHELL_SURFACE_SCREENSAVER)
			surface->output = shsurf->output;
	}
}

static void
desktop_shell_sigchld(struct weston_process *process, int status)
{
	struct wl_shell *shell =
		container_of(process, struct wl_shell, child.process);

	shell->child.process.pid = 0;
	shell->child.client = NULL; /* already destroyed by wayland */
}

static int
launch_desktop_shell_process(struct wl_shell *shell)
{
	const char *shell_exe = LIBEXECDIR "/wayland-desktop-shell";

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
	struct wl_shell *shell = data;

	wl_client_add_object(client, &wl_shell_interface,
			     &shell_implementation, id, shell);
}

static void
unbind_desktop_shell(struct wl_resource *resource)
{
	struct wl_shell *shell = resource->data;

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
	struct wl_shell *shell = data;
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
	wl_resource_destroy(resource, 0);
}

static void
screensaver_set_surface(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *shell_surface_resource,
			struct wl_resource *output_resource)
{
	struct wl_shell *shell = resource->data;
	struct shell_surface *surface = shell_surface_resource->data;
	struct weston_output *output = output_resource->data;

	if (reset_shell_surface_type(surface))
		return;

	surface->type = SHELL_SURFACE_SCREENSAVER;

	surface->surface->fullscreen_output = output;
	surface->output = output;
	wl_list_insert(shell->screensaver.surfaces.prev, &surface->link);
}

static const struct screensaver_interface screensaver_implementation = {
	screensaver_set_surface
};

static void
unbind_screensaver(struct wl_resource *resource)
{
	struct wl_shell *shell = resource->data;

	shell->screensaver.binding = NULL;
	free(resource);
}

static void
bind_screensaver(struct wl_client *client,
		 void *data, uint32_t version, uint32_t id)
{
	struct wl_shell *shell = data;
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
	wl_resource_destroy(resource, 0);
}

int
shell_init(struct weston_compositor *ec);

WL_EXPORT int
shell_init(struct weston_compositor *ec)
{
	struct wl_shell *shell;

	shell = malloc(sizeof *shell);
	if (shell == NULL)
		return -1;

	memset(shell, 0, sizeof *shell);
	shell->compositor = ec;
	shell->shell.lock = lock;
	shell->shell.unlock = unlock;
	shell->shell.map = map;
	shell->shell.configure = configure;

	wl_list_init(&shell->hidden_surface_list);
	wl_list_init(&shell->backgrounds);
	wl_list_init(&shell->panels);
	wl_list_init(&shell->screensaver.surfaces);

	if (shell_configuration(shell) < 0)
		return -1;

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

	if (launch_desktop_shell_process(shell) != 0)
		return -1;

	weston_compositor_add_binding(ec, 0, BTN_LEFT, MODIFIER_SUPER,
				    move_binding, shell);
	weston_compositor_add_binding(ec, 0, BTN_MIDDLE, MODIFIER_SUPER,
				    resize_binding, shell);
	weston_compositor_add_binding(ec, KEY_BACKSPACE, 0,
				    MODIFIER_CTRL | MODIFIER_ALT,
				    terminate_binding, ec);
	weston_compositor_add_binding(ec, 0, BTN_LEFT, 0,
				    click_to_activate_binding, ec);




	ec->shell = &shell->shell;

	return 0;
}
