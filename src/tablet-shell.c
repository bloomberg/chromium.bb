/*
 * Copyright © 2011 Intel Corporation
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

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>

#include "compositor.h"
#include "tablet-shell-server-protocol.h"

/*
 * TODO: Don't fade back from black until we've received a lockscreen
 * attachment.
 */

enum {
	STATE_STARTING,
	STATE_LOCKED,
	STATE_HOME,
	STATE_SWITCHER,
	STATE_TASK
};

struct tablet_shell {
	struct wl_resource resource;

	struct weston_shell shell;

	struct weston_compositor *compositor;
	struct weston_process process;
	struct weston_input_device *device;
	struct wl_client *client;

	struct weston_surface *surface;

	struct weston_surface *lockscreen_surface;
	struct wl_listener lockscreen_listener;

	struct weston_surface *home_surface;

	struct weston_surface *switcher_surface;
	struct wl_listener switcher_listener;

	struct tablet_client *current_client;

	int state, previous_state;
	int long_press_active;
	struct wl_event_source *long_press_source;
};

struct tablet_client {
	struct wl_resource resource;
	struct tablet_shell *shell;
	struct wl_client *client;
	struct weston_surface *surface;
	char *name;
};

static void
tablet_shell_sigchld(struct weston_process *process, int status)
{
	struct tablet_shell *shell =
		container_of(process, struct tablet_shell, process);

	shell->process.pid = 0;

	fprintf(stderr,
		"weston-tablet-shell crashed, exit code %d\n", status);
}

static void
tablet_shell_set_state(struct tablet_shell *shell, int state)
{
	static const char *states[] = {
		"STARTING", "LOCKED", "HOME", "SWITCHER", "TASK"
	};

	fprintf(stderr, "switching to state %s (from %s)\n",
		states[state], states[shell->state]);
	shell->previous_state = shell->state;
	shell->state = state;
}

static void
tablet_shell_map(struct weston_shell *base, struct weston_surface *surface,
		       int32_t width, int32_t height)
{
	struct tablet_shell *shell =
		container_of(base, struct tablet_shell, shell);

	surface->x = 0;
	surface->y = 0;

	if (surface == shell->lockscreen_surface) {
		/* */
	} else if (surface == shell->switcher_surface) {
		/* */
	} else if (surface == shell->home_surface) {
		if (shell->state == STATE_STARTING) {
			tablet_shell_set_state(shell, STATE_LOCKED);
			shell->previous_state = STATE_HOME;
			wl_resource_post_event(&shell->resource,
					       TABLET_SHELL_SHOW_LOCKSCREEN);
		}
	} else if (shell->current_client &&
		   shell->current_client->surface != surface &&
		   shell->current_client->client == surface->surface.resource.client) {
		tablet_shell_set_state(shell, STATE_TASK);
		shell->current_client->surface = surface;
		weston_zoom_run(surface, 0.3, 1.0, NULL, NULL);
	}

	wl_list_insert(&shell->compositor->surface_list, &surface->link);
	weston_surface_configure(surface, surface->x, surface->y, width, height);
}

static void
tablet_shell_configure(struct weston_shell *base,
			     struct weston_surface *surface,
			     int32_t x, int32_t y,
			     int32_t width, int32_t height)
{
	weston_surface_configure(surface, x, y, width, height);
}

static void
handle_lockscreen_surface_destroy(struct wl_listener *listener,
				  struct wl_resource *resource, uint32_t time)
{
	struct tablet_shell *shell =
		container_of(listener,
			     struct tablet_shell, lockscreen_listener);

	shell->lockscreen_surface = NULL;
	tablet_shell_set_state(shell, shell->previous_state);
}

static void
tablet_shell_set_lockscreen(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct weston_surface *es = surface_resource->data;

	es->x = 0;
	es->y = 0;
	shell->lockscreen_surface = es;
	shell->lockscreen_listener.func = handle_lockscreen_surface_destroy;
	wl_list_insert(es->surface.resource.destroy_listener_list.prev,
		       &shell->lockscreen_listener.link);
}

static void
handle_switcher_surface_destroy(struct wl_listener *listener,
				struct wl_resource *resource, uint32_t time)
{
	struct tablet_shell *shell =
		container_of(listener,
			     struct tablet_shell, switcher_listener);

	shell->switcher_surface = NULL;
	if (shell->state != STATE_LOCKED)
		tablet_shell_set_state(shell, shell->previous_state);
}

static void
tablet_shell_set_switcher(struct wl_client *client,
			  struct wl_resource *resource,
			  struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct weston_surface *es = surface_resource->data;

	/* FIXME: Switcher should be centered and the compositor
	 * should do the tinting of the background.  With the cache
	 * layer idea, we should be able to hit the framerate on the
	 * fade/zoom in. */
	shell->switcher_surface = es;
	shell->switcher_surface->x = 0;
	shell->switcher_surface->y = 0;

	shell->switcher_listener.func = handle_switcher_surface_destroy;
	wl_list_insert(es->surface.resource.destroy_listener_list.prev,
		       &shell->switcher_listener.link);
}

static void
tablet_shell_set_homescreen(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;

	shell->home_surface = surface_resource->data;
	shell->home_surface->x = 0;
	shell->home_surface->y = 0;
}

static void
minimize_zoom_done(struct weston_zoom *zoom, void *data)
{
	struct tablet_shell *shell = data;
	struct weston_compositor *compositor = shell->compositor;
	struct weston_input_device *device =
		(struct weston_input_device *) compositor->input_device;

	weston_surface_activate(shell->home_surface,
			      device, weston_compositor_get_time());
}

static void
tablet_shell_switch_to(struct tablet_shell *shell,
			     struct weston_surface *surface)
{
	struct weston_compositor *compositor = shell->compositor;
	struct weston_input_device *device =
		(struct weston_input_device *) compositor->input_device;
	struct weston_surface *current;

	if (shell->state == STATE_SWITCHER) {
		wl_list_remove(&shell->switcher_listener.link);
		shell->switcher_surface = NULL;
	};

	if (surface == shell->home_surface) {
		tablet_shell_set_state(shell, STATE_HOME);

		if (shell->current_client && shell->current_client->surface) {
			current = shell->current_client->surface;
			weston_zoom_run(current, 1.0, 0.3,
				      minimize_zoom_done, shell);
		}
	} else {
		fprintf(stderr, "switch to %p\n", surface);
		weston_surface_activate(surface, device,
				      weston_compositor_get_time());
		tablet_shell_set_state(shell, STATE_TASK);
		weston_zoom_run(surface, 0.3, 1.0, NULL, NULL);
	}
}

static void
tablet_shell_show_grid(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct weston_surface *es = surface_resource->data;

	tablet_shell_switch_to(shell, es);
}

static void
tablet_shell_show_panels(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = resource->data;
	struct weston_surface *es = surface_resource->data;

	tablet_shell_switch_to(shell, es);
}

static void
destroy_tablet_client(struct wl_resource *resource)
{
	struct tablet_client *tablet_client =
		container_of(resource, struct tablet_client, resource);

	free(tablet_client->name);
	free(tablet_client);
}

static void
tablet_client_destroy(struct wl_client *client,
		      struct wl_resource *resource)
{
	wl_resource_destroy(resource, weston_compositor_get_time());
}

static void
tablet_client_activate(struct wl_client *client, struct wl_resource *resource)
{
	struct tablet_client *tablet_client = resource->data;
	struct tablet_shell *shell = tablet_client->shell;

	shell->current_client = tablet_client;
	if (!tablet_client->surface)
		return;

	tablet_shell_switch_to(shell, tablet_client->surface);
}

static const struct tablet_client_interface tablet_client_implementation = {
	tablet_client_destroy,
	tablet_client_activate
};

static void
tablet_shell_create_client(struct wl_client *client,
			   struct wl_resource *resource,
			   uint32_t id, const char *name, int fd)
{
	struct tablet_shell *shell = resource->data;
	struct weston_compositor *compositor = shell->compositor;
	struct tablet_client *tablet_client;

	tablet_client = malloc(sizeof *tablet_client);
	if (tablet_client == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	tablet_client->client = wl_client_create(compositor->wl_display, fd);
	tablet_client->shell = shell;
	tablet_client->name = strdup(name);

	tablet_client->resource.destroy = destroy_tablet_client;
	tablet_client->resource.object.id = id;
	tablet_client->resource.object.interface =
		&tablet_client_interface;
	tablet_client->resource.object.implementation =
		(void (**)(void)) &tablet_client_implementation;

	wl_client_add_resource(client, &tablet_client->resource);
	tablet_client->surface = NULL;
	shell->current_client = tablet_client;

	fprintf(stderr, "created client %p, id %d, name %s, fd %d\n",
		tablet_client->client, id, name, fd);
}

static const struct tablet_shell_interface tablet_shell_implementation = {
	tablet_shell_set_lockscreen,
	tablet_shell_set_switcher,
	tablet_shell_set_homescreen,
	tablet_shell_show_grid,
	tablet_shell_show_panels,
	tablet_shell_create_client
};

static void
launch_ux_daemon(struct tablet_shell *shell)
{
	const char *shell_exe = LIBEXECDIR "/weston-tablet-shell";

	shell->client = weston_client_launch(shell->compositor,
					   &shell->process,
					   shell_exe, tablet_shell_sigchld);
}

static void
toggle_switcher(struct tablet_shell *shell)
{
	switch (shell->state) {
	case STATE_SWITCHER:
		wl_resource_post_event(&shell->resource,
				     TABLET_SHELL_HIDE_SWITCHER);
		break;
	default:
		wl_resource_post_event(&shell->resource,
				       TABLET_SHELL_SHOW_SWITCHER);
		tablet_shell_set_state(shell, STATE_SWITCHER);
		break;
	}
}

static void
tablet_shell_lock(struct weston_shell *base)
{
	struct tablet_shell *shell =
		container_of(base, struct tablet_shell, shell);

	if (shell->state == STATE_LOCKED)
		return;
	if (shell->state == STATE_SWITCHER)
		wl_resource_post_event(&shell->resource,
				       TABLET_SHELL_HIDE_SWITCHER);

	wl_resource_post_event(&shell->resource,
			       TABLET_SHELL_SHOW_LOCKSCREEN);
	
	tablet_shell_set_state(shell, STATE_LOCKED);
}

static void
tablet_shell_unlock(struct weston_shell *base)
{
	struct tablet_shell *shell =
		container_of(base, struct tablet_shell, shell);

	weston_compositor_wake(shell->compositor);
}

static void
go_home(struct tablet_shell *shell)
{
	struct weston_input_device *device =
		(struct weston_input_device *) shell->compositor->input_device;

	if (shell->state == STATE_SWITCHER)
		wl_resource_post_event(&shell->resource,
				       TABLET_SHELL_HIDE_SWITCHER);

	weston_surface_activate(shell->home_surface, device,
			      weston_compositor_get_time());

	tablet_shell_set_state(shell, STATE_HOME);
}

static int
long_press_handler(void *data)
{
	struct tablet_shell *shell = data;

	shell->long_press_active = 0;
	toggle_switcher(shell);

	return 1;
}

static void
menu_key_binding(struct wl_input_device *device, uint32_t time,
		 uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct tablet_shell *shell = data;

	if (shell->state == STATE_LOCKED)
		return;

	if (state)
		toggle_switcher(shell);
}

static void
home_key_binding(struct wl_input_device *device, uint32_t time,
		 uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct tablet_shell *shell = data;

	if (shell->state == STATE_LOCKED)
		return;

	shell->device = (struct weston_input_device *) device;

	if (state) {
		wl_event_source_timer_update(shell->long_press_source, 500);
		shell->long_press_active = 1;
	} else if (shell->long_press_active) {
		wl_event_source_timer_update(shell->long_press_source, 0);
		shell->long_press_active = 0;

		switch (shell->state) {
		case STATE_HOME:
		case STATE_SWITCHER:
			toggle_switcher(shell);
			break;
		default:
			go_home(shell);
			break;
		}
	}
}

static void
destroy_tablet_shell(struct wl_resource *resource)
{
}

static void
bind_shell(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct tablet_shell *shell = data;

	if (shell->client != client)
		/* Throw an error or just let the client fail when it
		 * tries to access the object?. */
		return;

	shell->resource.object.id = id;
	shell->resource.object.interface = &tablet_shell_interface;
	shell->resource.object.implementation =
		(void (**)(void)) &tablet_shell_implementation;
	shell->resource.client = client;
	shell->resource.data = shell;
	shell->resource.destroy = destroy_tablet_shell;

	wl_client_add_resource(client, &shell->resource);
}

static void
tablet_shell_destroy(struct weston_shell *base)
{
	struct tablet_shell *shell =
		container_of(base, struct tablet_shell, shell);

	wl_event_source_remove(shell->long_press_source);
	free(shell);
}

void
shell_init(struct weston_compositor *compositor);

WL_EXPORT void
shell_init(struct weston_compositor *compositor)
{
	struct tablet_shell *shell;
	struct wl_event_loop *loop;

	shell = malloc(sizeof *shell);
	if (shell == NULL)
		return;

	memset(shell, 0, sizeof *shell);
	shell->compositor = compositor;

	/* FIXME: This will make the object available to all clients. */
	wl_display_add_global(compositor->wl_display,
			      &tablet_shell_interface, shell, bind_shell);

	loop = wl_display_get_event_loop(compositor->wl_display);
	shell->long_press_source =
		wl_event_loop_add_timer(loop, long_press_handler, shell);

	weston_compositor_add_binding(compositor, KEY_LEFTMETA, 0, 0,
				    home_key_binding, shell);
	weston_compositor_add_binding(compositor, KEY_RIGHTMETA, 0, 0,
				    home_key_binding, shell);
	weston_compositor_add_binding(compositor, KEY_LEFTMETA, 0,
				    MODIFIER_SUPER, home_key_binding, shell);
	weston_compositor_add_binding(compositor, KEY_RIGHTMETA, 0,
				    MODIFIER_SUPER, home_key_binding, shell);
 	weston_compositor_add_binding(compositor, KEY_COMPOSE, 0, 0,
				    menu_key_binding, shell);

	compositor->shell = &shell->shell;

	shell->shell.lock = tablet_shell_lock;
	shell->shell.unlock = tablet_shell_unlock;
	shell->shell.map = tablet_shell_map;
	shell->shell.configure = tablet_shell_configure;
	shell->shell.destroy = tablet_shell_destroy;

	launch_ux_daemon(shell);

	tablet_shell_set_state(shell, STATE_STARTING);
}
