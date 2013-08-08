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

#include "config.h"

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
	struct wl_resource *resource;

	struct wl_listener lock_listener;
	struct wl_listener unlock_listener;
	struct wl_listener destroy_listener;

	struct weston_compositor *compositor;
	struct weston_process process;
	struct wl_client *client;

	struct weston_surface *surface;

	struct weston_surface *lockscreen_surface;
	struct wl_listener lockscreen_listener;
	struct weston_layer lockscreen_layer;

	struct weston_layer application_layer;

	struct weston_surface *home_surface;
	struct weston_layer homescreen_layer;

	struct weston_surface *switcher_surface;
	struct wl_listener switcher_listener;

	struct tablet_client *current_client;

	int state, previous_state;
	int long_press_active;
	struct wl_event_source *long_press_source;
};

struct tablet_client {
	struct wl_resource *resource;
	struct tablet_shell *shell;
	struct wl_client *client;
	struct weston_surface *surface;
	char *name;
};

static void
tablet_shell_destroy(struct wl_listener *listener, void *data);

static struct tablet_shell *
get_shell(struct weston_compositor *compositor)
{
	struct wl_listener *l;

	l = wl_signal_get(&compositor->destroy_signal, tablet_shell_destroy);
	if (l)
		return container_of(l, struct tablet_shell, destroy_listener);

	return NULL;
}

static void
tablet_shell_sigchld(struct weston_process *process, int status)
{
	struct tablet_shell *shell =
		container_of(process, struct tablet_shell, process);

	shell->process.pid = 0;

	weston_log("weston-tablet-shell crashed, exit code %d\n", status);
}

static void
tablet_shell_set_state(struct tablet_shell *shell, int state)
{
	static const char *states[] = {
		"STARTING", "LOCKED", "HOME", "SWITCHER", "TASK"
	};

	weston_log("switching to state %s (from %s)\n",
		states[state], states[shell->state]);
	shell->previous_state = shell->state;
	shell->state = state;
}

static void
tablet_shell_surface_configure(struct weston_surface *surface,
			       int32_t sx, int32_t sy, int32_t width, int32_t height)
{
	struct tablet_shell *shell = get_shell(surface->compositor);

	if (weston_surface_is_mapped(surface) || width == 0)
		return;

	weston_surface_configure(surface, 0, 0, width, height);

	if (surface == shell->lockscreen_surface) {
			wl_list_insert(&shell->lockscreen_layer.surface_list,
					&surface->layer_link);
	} else if (surface == shell->switcher_surface) {
		/* */
	} else if (surface == shell->home_surface) {
		if (shell->state == STATE_STARTING) {
	                /* homescreen always visible, at the bottom */
			wl_list_insert(&shell->homescreen_layer.surface_list,
					&surface->layer_link);

			tablet_shell_set_state(shell, STATE_LOCKED);
			shell->previous_state = STATE_HOME;
			tablet_shell_send_show_lockscreen(shell->resource);
		}
	} else if (shell->current_client &&
		   shell->current_client->surface != surface &&
		   shell->current_client->client == wl_resource_get_client(surface->resource)) {
		tablet_shell_set_state(shell, STATE_TASK);
		shell->current_client->surface = surface;
		weston_zoom_run(surface, 0.3, 1.0, NULL, NULL);
		wl_list_insert(&shell->application_layer.surface_list,
			       &surface->layer_link);
	}

	weston_surface_update_transform(surface);
}

static void
handle_lockscreen_surface_destroy(struct wl_listener *listener, void *data)
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
	struct tablet_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *es = wl_resource_get_user_data(surface_resource);

	weston_surface_set_position(es, 0, 0);
	shell->lockscreen_surface = es;
	shell->lockscreen_surface->configure = tablet_shell_surface_configure;
	shell->lockscreen_listener.notify = handle_lockscreen_surface_destroy;
	wl_signal_add(&es->destroy_signal, &shell->lockscreen_listener);
}

static void
handle_switcher_surface_destroy(struct wl_listener *listener, void *data)
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
	struct tablet_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *es = wl_resource_get_user_data(surface_resource);

	/* FIXME: Switcher should be centered and the compositor
	 * should do the tinting of the background.  With the cache
	 * layer idea, we should be able to hit the framerate on the
	 * fade/zoom in. */
	shell->switcher_surface = es;
	weston_surface_set_position(shell->switcher_surface, 0, 0);

	shell->switcher_listener.notify = handle_switcher_surface_destroy;
	wl_signal_add(&es->destroy_signal, &shell->switcher_listener);
}

static void
tablet_shell_set_homescreen(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = wl_resource_get_user_data(resource);

	shell->home_surface = wl_resource_get_user_data(surface_resource);
	shell->home_surface->configure = tablet_shell_surface_configure;

	weston_surface_set_position(shell->home_surface, 0, 0);
}

static void
minimize_zoom_done(struct weston_surface_animation *zoom, void *data)
{
	struct tablet_shell *shell = data;
	struct weston_compositor *compositor = shell->compositor;
	struct weston_seat *seat;

	wl_list_for_each(seat, &compositor->seat_list, link)
		weston_surface_activate(shell->home_surface, seat);
}

static void
tablet_shell_switch_to(struct tablet_shell *shell,
			     struct weston_surface *surface)
{
	struct weston_compositor *compositor = shell->compositor;
	struct weston_seat *seat;
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
		wl_list_for_each(seat, &compositor->seat_list, link)
			weston_surface_activate(surface, seat);
		tablet_shell_set_state(shell, STATE_TASK);
		weston_zoom_run(surface, 0.3, 1.0, NULL, NULL);
	}
}

static void
tablet_shell_show_grid(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *es = wl_resource_get_user_data(surface_resource);

	tablet_shell_switch_to(shell, es);
}

static void
tablet_shell_show_panels(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *surface_resource)
{
	struct tablet_shell *shell = wl_resource_get_user_data(resource);
	struct weston_surface *es = wl_resource_get_user_data(surface_resource);

	tablet_shell_switch_to(shell, es);
}

static void
destroy_tablet_client(struct wl_resource *resource)
{
	struct tablet_client *tablet_client =
		wl_resource_get_user_data(resource);

	free(tablet_client->name);
	free(tablet_client);
}

static void
tablet_client_destroy(struct wl_client *client,
		      struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
tablet_client_activate(struct wl_client *client, struct wl_resource *resource)
{
	struct tablet_client *tablet_client = wl_resource_get_user_data(resource);
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
	struct tablet_shell *shell = wl_resource_get_user_data(resource);
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

	tablet_client->resource =
		wl_resource_create(client, &tablet_client_interface, 1, id);
	wl_resource_set_implementation(tablet_client->resource,
				       &tablet_client_implementation,
				       tablet_client, destroy_tablet_client);

	tablet_client->surface = NULL;
	shell->current_client = tablet_client;

	weston_log("created client %p, id %d, name %s, fd %d\n",
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
		tablet_shell_send_hide_switcher(shell->resource);
		break;
	default:
		tablet_shell_send_show_switcher(shell->resource);
		tablet_shell_set_state(shell, STATE_SWITCHER);
		break;
	}
}

static void
tablet_shell_lock(struct wl_listener *listener, void *data)
{
	struct tablet_shell *shell =
		container_of(listener, struct tablet_shell, lock_listener);

	if (shell->state == STATE_LOCKED)
		return;
	if (shell->state == STATE_SWITCHER)
		tablet_shell_send_hide_switcher(shell->resource);

	tablet_shell_send_show_lockscreen(shell->resource);
	tablet_shell_set_state(shell, STATE_LOCKED);
}

static void
tablet_shell_unlock(struct wl_listener *listener, void *data)
{
	struct tablet_shell *shell =
		container_of(listener, struct tablet_shell, unlock_listener);

	tablet_shell_set_state(shell, STATE_HOME);
}

static void
go_home(struct tablet_shell *shell, struct weston_seat *seat)
{
	if (shell->state == STATE_SWITCHER)
		tablet_shell_send_hide_switcher(shell->resource);

	weston_surface_activate(shell->home_surface, seat);

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
menu_key_binding(struct weston_seat *seat, uint32_t time, uint32_t key, void *data)
{
	struct tablet_shell *shell = data;

	if (shell->state == STATE_LOCKED)
		return;

	toggle_switcher(shell);
}

static void
home_key_binding(struct weston_seat *seat, uint32_t time, uint32_t key, void *data)
{
	struct tablet_shell *shell = data;

	if (shell->state == STATE_LOCKED)
		return;

	if (1) {
		wl_event_source_timer_update(shell->long_press_source, 500);
		shell->long_press_active = 1;
	} else if (shell->long_press_active) {
		/* This code has never been run ... */
		wl_event_source_timer_update(shell->long_press_source, 0);
		shell->long_press_active = 0;

		switch (shell->state) {
		case STATE_HOME:
		case STATE_SWITCHER:
			toggle_switcher(shell);
			break;
		default:
			go_home(shell, (struct weston_seat *) seat);
			break;
		}
	}
}

static void
destroy_tablet_shell(struct wl_resource *resource)
{
}

static void
bind_tablet_shell(struct wl_client *client, void *data, uint32_t version,
		  uint32_t id)
{
	struct tablet_shell *shell = data;

	if (shell->client != client)
		/* Throw an error or just let the client fail when it
		 * tries to access the object?. */
		return;

	shell->resource =
		wl_resource_create(client, &tablet_shell_interface, 1, id);
	wl_resource_set_implementation(shell->resource,
				       &tablet_shell_implementation,
				       shell, destroy_tablet_shell);
}

static void
tablet_shell_destroy(struct wl_listener *listener, void *data)
{
	struct tablet_shell *shell =
		container_of(listener, struct tablet_shell, destroy_listener);

	if (shell->home_surface)
		shell->home_surface->configure = NULL;

	if (shell->lockscreen_surface)
		shell->lockscreen_surface->configure = NULL;

	wl_event_source_remove(shell->long_press_source);
	free(shell);
}

WL_EXPORT int
module_init(struct weston_compositor *compositor,
	    int *argc, char *argv[])
{
	struct tablet_shell *shell;
	struct wl_event_loop *loop;

	shell = zalloc(sizeof *shell);
	if (shell == NULL)
		return -1;

	shell->compositor = compositor;

	shell->destroy_listener.notify = tablet_shell_destroy;
	wl_signal_add(&compositor->destroy_signal, &shell->destroy_listener);
	shell->lock_listener.notify = tablet_shell_lock;
	wl_signal_add(&compositor->idle_signal, &shell->lock_listener);
	shell->unlock_listener.notify = tablet_shell_unlock;
	wl_signal_add(&compositor->wake_signal, &shell->unlock_listener);

	/* FIXME: This will make the object available to all clients. */
	wl_global_create(compositor->wl_display, &tablet_shell_interface, 1,
			 shell, bind_tablet_shell);

	loop = wl_display_get_event_loop(compositor->wl_display);
	shell->long_press_source =
		wl_event_loop_add_timer(loop, long_press_handler, shell);

	weston_compositor_add_key_binding(compositor, KEY_LEFTMETA, 0,
					  home_key_binding, shell);
	weston_compositor_add_key_binding(compositor, KEY_RIGHTMETA, 0,
					  home_key_binding, shell);
	weston_compositor_add_key_binding(compositor, KEY_LEFTMETA,
					  MODIFIER_SUPER, home_key_binding,
					  shell);
	weston_compositor_add_key_binding(compositor, KEY_RIGHTMETA,
					  MODIFIER_SUPER, home_key_binding,
					  shell);
	weston_compositor_add_key_binding(compositor, KEY_COMPOSE, 0,
					  menu_key_binding, shell);

	weston_layer_init(&shell->homescreen_layer,
			  &compositor->cursor_layer.link);
	weston_layer_init(&shell->application_layer,
			  &compositor->cursor_layer.link);
	weston_layer_init(&shell->lockscreen_layer,
			  &compositor->cursor_layer.link);
	launch_ux_daemon(shell);

	tablet_shell_set_state(shell, STATE_STARTING);

	return 0;
}
