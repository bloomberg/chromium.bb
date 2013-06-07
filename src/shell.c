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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <assert.h>
#include <signal.h>
#include <math.h>
#include <sys/types.h>

#include <wayland-server.h>
#include "compositor.h"
#include "desktop-shell-server-protocol.h"
#include "input-method-server-protocol.h"
#include "workspaces-server-protocol.h"
#include "../shared/config-parser.h"

#define DEFAULT_NUM_WORKSPACES 1
#define DEFAULT_WORKSPACE_CHANGE_ANIMATION_LENGTH 200

enum animation_type {
	ANIMATION_NONE,

	ANIMATION_ZOOM,
	ANIMATION_FADE
};

enum fade_type {
	FADE_IN,
	FADE_OUT
};

struct focus_state {
	struct weston_seat *seat;
	struct workspace *ws;
	struct weston_surface *keyboard_focus;
	struct wl_list link;
	struct wl_listener seat_destroy_listener;
	struct wl_listener surface_destroy_listener;
};

struct workspace {
	struct weston_layer layer;

	struct wl_list focus_list;
	struct wl_listener seat_destroyed_listener;
};

struct input_panel_surface {
	struct wl_resource resource;

	struct desktop_shell *shell;

	struct wl_list link;
	struct weston_surface *surface;
	struct wl_listener surface_destroy_listener;

	struct weston_output *output;
	uint32_t panel;
};

struct desktop_shell {
	struct weston_compositor *compositor;

	struct wl_listener idle_listener;
	struct wl_listener wake_listener;
	struct wl_listener destroy_listener;
	struct wl_listener show_input_panel_listener;
	struct wl_listener hide_input_panel_listener;
	struct wl_listener update_input_panel_listener;

	struct weston_layer fullscreen_layer;
	struct weston_layer panel_layer;
	struct weston_layer background_layer;
	struct weston_layer lock_layer;
	struct weston_layer input_panel_layer;

	struct wl_listener pointer_focus_listener;
	struct weston_surface *grab_surface;

	struct {
		struct weston_process process;
		struct wl_client *client;
		struct wl_resource *desktop_shell;

		unsigned deathcount;
		uint32_t deathstamp;
	} child;

	bool locked;
	bool showing_input_panels;
	bool prepare_event_sent;

	struct {
		struct weston_surface *surface;
		pixman_box32_t cursor_rectangle;
	} text_input;

	struct weston_surface *lock_surface;
	struct wl_listener lock_surface_listener;

	struct {
		struct wl_array array;
		unsigned int current;
		unsigned int num;

		struct wl_list client_list;

		struct weston_animation animation;
		struct wl_list anim_sticky_list;
		int anim_dir;
		uint32_t anim_timestamp;
		double anim_current;
		struct workspace *anim_from;
		struct workspace *anim_to;
	} workspaces;

	struct {
		char *path;
		int duration;
		struct wl_resource *binding;
		struct weston_process process;
		struct wl_event_source *timer;
	} screensaver;

	struct {
		struct wl_resource *binding;
		struct wl_list surfaces;
	} input_panel;

	struct {
		struct weston_surface *surface;
		struct weston_surface_animation *animation;
		enum fade_type type;
		struct wl_event_source *startup_timer;
	} fade;

	uint32_t binding_modifier;
	enum animation_type win_animation_type;
};

enum shell_surface_type {
	SHELL_SURFACE_NONE,
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
	struct weston_surface *parent;
	struct desktop_shell *shell;

	enum shell_surface_type type, next_type;
	char *title, *class;
	int32_t saved_x, saved_y;
	bool saved_position_valid;
	bool saved_rotation_valid;
	int unresponsive;

	struct {
		struct weston_transform transform;
		struct weston_matrix rotation;
	} rotation;

	struct {
		struct wl_list grab_link;
		int32_t x, y;
		struct shell_seat *shseat;
		uint32_t serial;
	} popup;

	struct {
		int32_t x, y;
		uint32_t flags;
	} transient;

	struct {
		enum wl_shell_surface_fullscreen_method type;
		struct weston_transform transform; /* matrix from x, y */
		uint32_t framerate;
		struct weston_surface *black_surface;
	} fullscreen;

	struct ping_timer *ping_timer;

	struct weston_transform workspace_transform;

	struct weston_output *fullscreen_output;
	struct weston_output *output;
	struct wl_list link;

	const struct weston_shell_client *client;
};

struct shell_grab {
	struct weston_pointer_grab grab;
	struct shell_surface *shsurf;
	struct wl_listener shsurf_destroy_listener;
	struct weston_pointer *pointer;
};

struct weston_move_grab {
	struct shell_grab base;
	wl_fixed_t dx, dy;
};

struct rotate_grab {
	struct shell_grab base;
	struct weston_matrix rotation;
	struct {
		float x;
		float y;
	} center;
};

struct shell_seat {
	struct weston_seat *seat;
	struct wl_listener seat_destroy_listener;

	struct {
		struct weston_pointer_grab grab;
		struct wl_list surfaces_list;
		struct wl_client *client;
		int32_t initial_up;
	} popup_grab;
};

static void
activate(struct desktop_shell *shell, struct weston_surface *es,
	 struct weston_seat *seat);

static struct workspace *
get_current_workspace(struct desktop_shell *shell);

static struct shell_surface *
get_shell_surface(struct weston_surface *surface);

static struct desktop_shell *
shell_surface_get_shell(struct shell_surface *shsurf);

static void
surface_rotate(struct shell_surface *surface, struct weston_seat *seat);

static void
shell_fade_startup(struct desktop_shell *shell);

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
popup_grab_end(struct weston_pointer *pointer);

static void
shell_grab_start(struct shell_grab *grab,
		 const struct weston_pointer_grab_interface *interface,
		 struct shell_surface *shsurf,
		 struct weston_pointer *pointer,
		 enum desktop_shell_cursor cursor)
{
	struct desktop_shell *shell = shsurf->shell;

	popup_grab_end(pointer);

	grab->grab.interface = interface;
	grab->shsurf = shsurf;
	grab->shsurf_destroy_listener.notify = destroy_shell_grab_shsurf;
	wl_signal_add(&shsurf->resource.destroy_signal,
		      &grab->shsurf_destroy_listener);

	grab->pointer = pointer;

	weston_pointer_start_grab(pointer, &grab->grab);
	desktop_shell_send_grab_cursor(shell->child.desktop_shell, cursor);
	weston_pointer_set_focus(pointer, shell->grab_surface,
				 wl_fixed_from_int(0), wl_fixed_from_int(0));
}

static void
shell_grab_end(struct shell_grab *grab)
{
	if (grab->shsurf)
		wl_list_remove(&grab->shsurf_destroy_listener.link);

	weston_pointer_end_grab(grab->pointer);
}

static void
center_on_output(struct weston_surface *surface,
		 struct weston_output *output);

static enum weston_keyboard_modifier
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

static enum animation_type
get_animation_type(char *animation)
{
	if (!animation)
		return ANIMATION_NONE;

	if (!strcmp("zoom", animation))
		return ANIMATION_ZOOM;
	else if (!strcmp("fade", animation))
		return ANIMATION_FADE;
	else
		return ANIMATION_NONE;
}

static void
shell_configuration(struct desktop_shell *shell)
{
	struct weston_config_section *section;
	int duration;
	char *s;

	section = weston_config_get_section(shell->compositor->config,
					    "screensaver", NULL, NULL);
	weston_config_section_get_string(section,
					 "path", &shell->screensaver.path, NULL);
	weston_config_section_get_int(section, "duration", &duration, 60);
	shell->screensaver.duration = duration * 1000;

	section = weston_config_get_section(shell->compositor->config,
					    "shell", NULL, NULL);
	weston_config_section_get_string(section,
					 "binding-modifier", &s, "super");
	shell->binding_modifier = get_modifier(s);
	weston_config_section_get_string(section, "animation", &s, "none");
	shell->win_animation_type = get_animation_type(s);
	weston_config_section_get_uint(section, "num-workspaces",
				       &shell->workspaces.num,
				       DEFAULT_NUM_WORKSPACES);
}

static void
focus_state_destroy(struct focus_state *state)
{
	wl_list_remove(&state->seat_destroy_listener.link);
	wl_list_remove(&state->surface_destroy_listener.link);
	free(state);
}

static void
focus_state_seat_destroy(struct wl_listener *listener, void *data)
{
	struct focus_state *state = container_of(listener,
						 struct focus_state,
						 seat_destroy_listener);

	wl_list_remove(&state->link);
	focus_state_destroy(state);
}

static void
focus_state_surface_destroy(struct wl_listener *listener, void *data)
{
	struct focus_state *state = container_of(listener,
						 struct focus_state,
						 surface_destroy_listener);
	struct desktop_shell *shell;
	struct weston_surface *main_surface;
	struct weston_surface *surface, *next;

	main_surface = weston_surface_get_main_surface(state->keyboard_focus);

	next = NULL;
	wl_list_for_each(surface, &state->ws->layer.surface_list, layer_link) {
		if (surface == main_surface)
			continue;

		next = surface;
		break;
	}

	/* if the focus was a sub-surface, activate its main surface */
	if (main_surface != state->keyboard_focus)
		next = main_surface;

	if (next) {
		shell = state->seat->compositor->shell_interface.shell;
		activate(shell, next, state->seat);
	} else {
		wl_list_remove(&state->link);
		focus_state_destroy(state);
	}
}

static struct focus_state *
focus_state_create(struct weston_seat *seat, struct workspace *ws)
{
	struct focus_state *state;

	state = malloc(sizeof *state);
	if (state == NULL)
		return NULL;

	state->ws = ws;
	state->seat = seat;
	wl_list_insert(&ws->focus_list, &state->link);

	state->seat_destroy_listener.notify = focus_state_seat_destroy;
	state->surface_destroy_listener.notify = focus_state_surface_destroy;
	wl_signal_add(&seat->destroy_signal,
		      &state->seat_destroy_listener);
	wl_list_init(&state->surface_destroy_listener.link);

	return state;
}

static struct focus_state *
ensure_focus_state(struct desktop_shell *shell, struct weston_seat *seat)
{
	struct workspace *ws = get_current_workspace(shell);
	struct focus_state *state;

	wl_list_for_each(state, &ws->focus_list, link)
		if (state->seat == seat)
			break;

	if (&state->link == &ws->focus_list)
		state = focus_state_create(seat, ws);

	return state;
}

static void
restore_focus_state(struct desktop_shell *shell, struct workspace *ws)
{
	struct focus_state *state, *next;
	struct weston_surface *surface;

	wl_list_for_each_safe(state, next, &ws->focus_list, link) {
		surface = state->keyboard_focus;

		weston_keyboard_set_focus(state->seat->keyboard, surface);
	}
}

static void
replace_focus_state(struct desktop_shell *shell, struct workspace *ws,
		    struct weston_seat *seat)
{
	struct focus_state *state;
	struct weston_surface *surface;

	wl_list_for_each(state, &ws->focus_list, link) {
		if (state->seat == seat) {
			surface = seat->keyboard->focus;
			state->keyboard_focus =
				(struct weston_surface *) surface;
			return;
		}
	}
}

static void
drop_focus_state(struct desktop_shell *shell, struct workspace *ws,
		 struct weston_surface *surface)
{
	struct focus_state *state;

	wl_list_for_each(state, &ws->focus_list, link)
		if (state->keyboard_focus == surface)
			state->keyboard_focus = NULL;
}

static void
workspace_destroy(struct workspace *ws)
{
	struct focus_state *state, *next;

	wl_list_for_each_safe(state, next, &ws->focus_list, link)
		focus_state_destroy(state);

	free(ws);
}

static void
seat_destroyed(struct wl_listener *listener, void *data)
{
	struct weston_seat *seat = data;
	struct focus_state *state, *next;
	struct workspace *ws = container_of(listener,
					    struct workspace,
					    seat_destroyed_listener);

	wl_list_for_each_safe(state, next, &ws->focus_list, link)
		if (state->seat == seat)
			wl_list_remove(&state->link);
}

static struct workspace *
workspace_create(void)
{
	struct workspace *ws = malloc(sizeof *ws);
	if (ws == NULL)
		return NULL;

	weston_layer_init(&ws->layer, NULL);

	wl_list_init(&ws->focus_list);
	wl_list_init(&ws->seat_destroyed_listener.link);
	ws->seat_destroyed_listener.notify = seat_destroyed;

	return ws;
}

static int
workspace_is_empty(struct workspace *ws)
{
	return wl_list_empty(&ws->layer.surface_list);
}

static struct workspace *
get_workspace(struct desktop_shell *shell, unsigned int index)
{
	struct workspace **pws = shell->workspaces.array.data;
	assert(index < shell->workspaces.num);
	pws += index;
	return *pws;
}

static struct workspace *
get_current_workspace(struct desktop_shell *shell)
{
	return get_workspace(shell, shell->workspaces.current);
}

static void
activate_workspace(struct desktop_shell *shell, unsigned int index)
{
	struct workspace *ws;

	ws = get_workspace(shell, index);
	wl_list_insert(&shell->panel_layer.link, &ws->layer.link);

	shell->workspaces.current = index;
}

static unsigned int
get_output_height(struct weston_output *output)
{
	return abs(output->region.extents.y1 - output->region.extents.y2);
}

static void
surface_translate(struct weston_surface *surface, double d)
{
	struct shell_surface *shsurf = get_shell_surface(surface);
	struct weston_transform *transform;

	transform = &shsurf->workspace_transform;
	if (wl_list_empty(&transform->link))
		wl_list_insert(surface->geometry.transformation_list.prev,
			       &shsurf->workspace_transform.link);

	weston_matrix_init(&shsurf->workspace_transform.matrix);
	weston_matrix_translate(&shsurf->workspace_transform.matrix,
				0.0, d, 0.0);
	weston_surface_geometry_dirty(surface);
}

static void
workspace_translate_out(struct workspace *ws, double fraction)
{
	struct weston_surface *surface;
	unsigned int height;
	double d;

	wl_list_for_each(surface, &ws->layer.surface_list, layer_link) {
		height = get_output_height(surface->output);
		d = height * fraction;

		surface_translate(surface, d);
	}
}

static void
workspace_translate_in(struct workspace *ws, double fraction)
{
	struct weston_surface *surface;
	unsigned int height;
	double d;

	wl_list_for_each(surface, &ws->layer.surface_list, layer_link) {
		height = get_output_height(surface->output);

		if (fraction > 0)
			d = -(height - height * fraction);
		else
			d = height + height * fraction;

		surface_translate(surface, d);
	}
}

static void
broadcast_current_workspace_state(struct desktop_shell *shell)
{
	struct wl_resource *resource;

	wl_list_for_each(resource, &shell->workspaces.client_list, link)
		workspace_manager_send_state(resource,
					     shell->workspaces.current,
					     shell->workspaces.num);
}

static void
reverse_workspace_change_animation(struct desktop_shell *shell,
				   unsigned int index,
				   struct workspace *from,
				   struct workspace *to)
{
	shell->workspaces.current = index;

	shell->workspaces.anim_to = to;
	shell->workspaces.anim_from = from;
	shell->workspaces.anim_dir = -1 * shell->workspaces.anim_dir;
	shell->workspaces.anim_timestamp = 0;

	weston_compositor_schedule_repaint(shell->compositor);
}

static void
workspace_deactivate_transforms(struct workspace *ws)
{
	struct weston_surface *surface;
	struct shell_surface *shsurf;

	wl_list_for_each(surface, &ws->layer.surface_list, layer_link) {
		shsurf = get_shell_surface(surface);
		if (!wl_list_empty(&shsurf->workspace_transform.link)) {
			wl_list_remove(&shsurf->workspace_transform.link);
			wl_list_init(&shsurf->workspace_transform.link);
		}
		weston_surface_geometry_dirty(surface);
	}
}

static void
finish_workspace_change_animation(struct desktop_shell *shell,
				  struct workspace *from,
				  struct workspace *to)
{
	weston_compositor_schedule_repaint(shell->compositor);

	wl_list_remove(&shell->workspaces.animation.link);
	workspace_deactivate_transforms(from);
	workspace_deactivate_transforms(to);
	shell->workspaces.anim_to = NULL;

	wl_list_remove(&shell->workspaces.anim_from->layer.link);
}

static void
animate_workspace_change_frame(struct weston_animation *animation,
			       struct weston_output *output, uint32_t msecs)
{
	struct desktop_shell *shell =
		container_of(animation, struct desktop_shell,
			     workspaces.animation);
	struct workspace *from = shell->workspaces.anim_from;
	struct workspace *to = shell->workspaces.anim_to;
	uint32_t t;
	double x, y;

	if (workspace_is_empty(from) && workspace_is_empty(to)) {
		finish_workspace_change_animation(shell, from, to);
		return;
	}

	if (shell->workspaces.anim_timestamp == 0) {
		if (shell->workspaces.anim_current == 0.0)
			shell->workspaces.anim_timestamp = msecs;
		else
			shell->workspaces.anim_timestamp =
				msecs -
				/* Invers of movement function 'y' below. */
				(asin(1.0 - shell->workspaces.anim_current) *
				 DEFAULT_WORKSPACE_CHANGE_ANIMATION_LENGTH *
				 M_2_PI);
	}

	t = msecs - shell->workspaces.anim_timestamp;

	/*
	 * x = [0, π/2]
	 * y(x) = sin(x)
	 */
	x = t * (1.0/DEFAULT_WORKSPACE_CHANGE_ANIMATION_LENGTH) * M_PI_2;
	y = sin(x);

	if (t < DEFAULT_WORKSPACE_CHANGE_ANIMATION_LENGTH) {
		weston_compositor_schedule_repaint(shell->compositor);

		workspace_translate_out(from, shell->workspaces.anim_dir * y);
		workspace_translate_in(to, shell->workspaces.anim_dir * y);
		shell->workspaces.anim_current = y;

		weston_compositor_schedule_repaint(shell->compositor);
	}
	else
		finish_workspace_change_animation(shell, from, to);
}

static void
animate_workspace_change(struct desktop_shell *shell,
			 unsigned int index,
			 struct workspace *from,
			 struct workspace *to)
{
	struct weston_output *output;

	int dir;

	if (index > shell->workspaces.current)
		dir = -1;
	else
		dir = 1;

	shell->workspaces.current = index;

	shell->workspaces.anim_dir = dir;
	shell->workspaces.anim_from = from;
	shell->workspaces.anim_to = to;
	shell->workspaces.anim_current = 0.0;
	shell->workspaces.anim_timestamp = 0;

	output = container_of(shell->compositor->output_list.next,
			      struct weston_output, link);
	wl_list_insert(&output->animation_list,
		       &shell->workspaces.animation.link);

	wl_list_insert(from->layer.link.prev, &to->layer.link);

	workspace_translate_in(to, 0);

	restore_focus_state(shell, to);

	weston_compositor_schedule_repaint(shell->compositor);
}

static void
update_workspace(struct desktop_shell *shell, unsigned int index,
		 struct workspace *from, struct workspace *to)
{
	shell->workspaces.current = index;
	wl_list_insert(&from->layer.link, &to->layer.link);
	wl_list_remove(&from->layer.link);
}

static void
change_workspace(struct desktop_shell *shell, unsigned int index)
{
	struct workspace *from;
	struct workspace *to;

	if (index == shell->workspaces.current)
		return;

	/* Don't change workspace when there is any fullscreen surfaces. */
	if (!wl_list_empty(&shell->fullscreen_layer.surface_list))
		return;

	from = get_current_workspace(shell);
	to = get_workspace(shell, index);

	if (shell->workspaces.anim_from == to &&
	    shell->workspaces.anim_to == from) {
		restore_focus_state(shell, to);
		reverse_workspace_change_animation(shell, index, from, to);
		broadcast_current_workspace_state(shell);
		return;
	}

	if (shell->workspaces.anim_to != NULL)
		finish_workspace_change_animation(shell,
						  shell->workspaces.anim_from,
						  shell->workspaces.anim_to);

	restore_focus_state(shell, to);

	if (workspace_is_empty(to) && workspace_is_empty(from))
		update_workspace(shell, index, from, to);
	else
		animate_workspace_change(shell, index, from, to);

	broadcast_current_workspace_state(shell);
}

static bool
workspace_has_only(struct workspace *ws, struct weston_surface *surface)
{
	struct wl_list *list = &ws->layer.surface_list;
	struct wl_list *e;

	if (wl_list_empty(list))
		return false;

	e = list->next;

	if (e->next != list)
		return false;

	return container_of(e, struct weston_surface, layer_link) == surface;
}

static void
move_surface_to_workspace(struct desktop_shell *shell,
			  struct weston_surface *surface,
			  uint32_t workspace)
{
	struct workspace *from;
	struct workspace *to;
	struct weston_seat *seat;
	struct weston_surface *focus;

	assert(weston_surface_get_main_surface(surface) == surface);

	if (workspace == shell->workspaces.current)
		return;

	if (workspace >= shell->workspaces.num)
		workspace = shell->workspaces.num - 1;

	from = get_current_workspace(shell);
	to = get_workspace(shell, workspace);

	wl_list_remove(&surface->layer_link);
	wl_list_insert(&to->layer.surface_list, &surface->layer_link);

	drop_focus_state(shell, from, surface);
	wl_list_for_each(seat, &shell->compositor->seat_list, link) {
		if (!seat->keyboard)
			continue;

		focus = weston_surface_get_main_surface(seat->keyboard->focus);
		if (focus == surface)
			weston_keyboard_set_focus(seat->keyboard, NULL);
	}

	weston_surface_damage_below(surface);
}

static void
take_surface_to_workspace_by_seat(struct desktop_shell *shell,
				  struct weston_seat *seat,
				  unsigned int index)
{
	struct weston_surface *surface;
	struct shell_surface *shsurf;
	struct workspace *from;
	struct workspace *to;
	struct focus_state *state;

	surface = weston_surface_get_main_surface(seat->keyboard->focus);
	if (surface == NULL ||
	    index == shell->workspaces.current)
		return;

	from = get_current_workspace(shell);
	to = get_workspace(shell, index);

	wl_list_remove(&surface->layer_link);
	wl_list_insert(&to->layer.surface_list, &surface->layer_link);

	replace_focus_state(shell, to, seat);
	drop_focus_state(shell, from, surface);

	if (shell->workspaces.anim_from == to &&
	    shell->workspaces.anim_to == from) {
		wl_list_remove(&to->layer.link);
		wl_list_insert(from->layer.link.prev, &to->layer.link);

		reverse_workspace_change_animation(shell, index, from, to);
		broadcast_current_workspace_state(shell);

		return;
	}

	if (shell->workspaces.anim_to != NULL)
		finish_workspace_change_animation(shell,
						  shell->workspaces.anim_from,
						  shell->workspaces.anim_to);

	if (workspace_is_empty(from) &&
	    workspace_has_only(to, surface))
		update_workspace(shell, index, from, to);
	else {
		shsurf = get_shell_surface(surface);
		if (wl_list_empty(&shsurf->workspace_transform.link))
			wl_list_insert(&shell->workspaces.anim_sticky_list,
				       &shsurf->workspace_transform.link);

		animate_workspace_change(shell, index, from, to);
	}

	broadcast_current_workspace_state(shell);

	state = ensure_focus_state(shell, seat);
	if (state != NULL)
		state->keyboard_focus = surface;
}

static void
workspace_manager_move_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource,
			       uint32_t workspace)
{
	struct desktop_shell *shell = resource->data;
	struct weston_surface *surface =
		(struct weston_surface *) surface_resource;
	struct weston_surface *main_surface;

	main_surface = weston_surface_get_main_surface(surface);
	move_surface_to_workspace(shell, main_surface, workspace);
}

static const struct workspace_manager_interface workspace_manager_implementation = {
	workspace_manager_move_surface,
};

static void
unbind_resource(struct wl_resource *resource)
{
	wl_list_remove(&resource->link);
	free(resource);
}

static void
bind_workspace_manager(struct wl_client *client,
		       void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &workspace_manager_interface,
					&workspace_manager_implementation,
					id, shell);

	if (resource == NULL) {
		weston_log("couldn't add workspace manager object");
		return;
	}

	resource->destroy = unbind_resource;
	wl_list_insert(&shell->workspaces.client_list, &resource->link);

	workspace_manager_send_state(resource,
				     shell->workspaces.current,
				     shell->workspaces.num);
}

static void
noop_grab_focus(struct weston_pointer_grab *grab)
{
}

static void
move_grab_motion(struct weston_pointer_grab *grab, uint32_t time)
{
	struct weston_move_grab *move = (struct weston_move_grab *) grab;
	struct weston_pointer *pointer = grab->pointer;
	struct shell_surface *shsurf = move->base.shsurf;
	struct weston_surface *es;
	int dx = wl_fixed_to_int(pointer->x + move->dx);
	int dy = wl_fixed_to_int(pointer->y + move->dy);

	if (!shsurf)
		return;

	es = shsurf->surface;

	weston_surface_configure(es, dx, dy,
				 es->geometry.width, es->geometry.height);

	weston_compositor_schedule_repaint(es->compositor);
}

static void
move_grab_button(struct weston_pointer_grab *grab,
		 uint32_t time, uint32_t button, uint32_t state_w)
{
	struct shell_grab *shell_grab = container_of(grab, struct shell_grab,
						    grab);
	struct weston_pointer *pointer = grab->pointer;
	enum wl_pointer_button_state state = state_w;

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		shell_grab_end(shell_grab);
		free(grab);
	}
}

static const struct weston_pointer_grab_interface move_grab_interface = {
	noop_grab_focus,
	move_grab_motion,
	move_grab_button,
};

static int
surface_move(struct shell_surface *shsurf, struct weston_seat *seat)
{
	struct weston_move_grab *move;

	if (!shsurf)
		return -1;

	if (shsurf->type == SHELL_SURFACE_FULLSCREEN)
		return 0;

	move = malloc(sizeof *move);
	if (!move)
		return -1;

	move->dx = wl_fixed_from_double(shsurf->surface->geometry.x) -
			seat->pointer->grab_x;
	move->dy = wl_fixed_from_double(shsurf->surface->geometry.y) -
			seat->pointer->grab_y;

	shell_grab_start(&move->base, &move_grab_interface, shsurf,
			 seat->pointer, DESKTOP_SHELL_CURSOR_MOVE);

	return 0;
}

static void
shell_surface_move(struct wl_client *client, struct wl_resource *resource,
		   struct wl_resource *seat_resource, uint32_t serial)
{
	struct weston_seat *seat = seat_resource->data;
	struct shell_surface *shsurf = resource->data;
	struct weston_surface *surface;

	surface = weston_surface_get_main_surface(seat->pointer->focus);
	if (seat->pointer->button_count == 0 ||
	    seat->pointer->grab_serial != serial ||
	    surface != shsurf->surface)
		return;

	if (surface_move(shsurf, seat) < 0)
		wl_resource_post_no_memory(resource);
}

struct weston_resize_grab {
	struct shell_grab base;
	uint32_t edges;
	int32_t width, height;
};

static void
resize_grab_motion(struct weston_pointer_grab *grab, uint32_t time)
{
	struct weston_resize_grab *resize = (struct weston_resize_grab *) grab;
	struct weston_pointer *pointer = grab->pointer;
	struct shell_surface *shsurf = resize->base.shsurf;
	int32_t width, height;
	wl_fixed_t from_x, from_y;
	wl_fixed_t to_x, to_y;

	if (!shsurf)
		return;

	weston_surface_from_global_fixed(shsurf->surface,
				         pointer->grab_x, pointer->grab_y,
				         &from_x, &from_y);
	weston_surface_from_global_fixed(shsurf->surface,
				         pointer->x, pointer->y, &to_x, &to_y);

	width = resize->width;
	if (resize->edges & WL_SHELL_SURFACE_RESIZE_LEFT) {
		width += wl_fixed_to_int(from_x - to_x);
	} else if (resize->edges & WL_SHELL_SURFACE_RESIZE_RIGHT) {
		width += wl_fixed_to_int(to_x - from_x);
	}

	height = resize->height;
	if (resize->edges & WL_SHELL_SURFACE_RESIZE_TOP) {
		height += wl_fixed_to_int(from_y - to_y);
	} else if (resize->edges & WL_SHELL_SURFACE_RESIZE_BOTTOM) {
		height += wl_fixed_to_int(to_y - from_y);
	}

	shsurf->client->send_configure(shsurf->surface,
				       resize->edges, width, height);
}

static void
send_configure(struct weston_surface *surface,
	       uint32_t edges, int32_t width, int32_t height)
{
	struct shell_surface *shsurf = get_shell_surface(surface);

	wl_shell_surface_send_configure(&shsurf->resource,
					edges, width, height);
}

static const struct weston_shell_client shell_client = {
	send_configure
};

static void
resize_grab_button(struct weston_pointer_grab *grab,
		   uint32_t time, uint32_t button, uint32_t state_w)
{
	struct weston_resize_grab *resize = (struct weston_resize_grab *) grab;
	struct weston_pointer *pointer = grab->pointer;
	enum wl_pointer_button_state state = state_w;

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		shell_grab_end(&resize->base);
		free(grab);
	}
}

static const struct weston_pointer_grab_interface resize_grab_interface = {
	noop_grab_focus,
	resize_grab_motion,
	resize_grab_button,
};

/*
 * Returns the bounding box of a surface and all its sub-surfaces,
 * in the surface coordinates system. */
static void
surface_subsurfaces_boundingbox(struct weston_surface *surface, int32_t *x,
				int32_t *y, int32_t *w, int32_t *h) {
	pixman_region32_t region;
	pixman_box32_t *box;
	struct weston_subsurface *subsurface;

	pixman_region32_init_rect(&region, 0, 0,
	                          surface->geometry.width,
	                          surface->geometry.height);

	wl_list_for_each(subsurface, &surface->subsurface_list, parent_link) {
		pixman_region32_union_rect(&region, &region,
		                           subsurface->position.x,
		                           subsurface->position.y,
		                           subsurface->surface->geometry.width,
		                           subsurface->surface->geometry.height);
	}

	box = pixman_region32_extents(&region);
	if (x)
		*x = box->x1;
	if (y)
		*y = box->y1;
	if (w)
		*w = box->x2 - box->x1;
	if (h)
		*h = box->y2 - box->y1;

	pixman_region32_fini(&region);
}

static int
surface_resize(struct shell_surface *shsurf,
	       struct weston_seat *seat, uint32_t edges)
{
	struct weston_resize_grab *resize;

	if (shsurf->type == SHELL_SURFACE_FULLSCREEN ||
	    shsurf->type == SHELL_SURFACE_MAXIMIZED)
		return 0;

	if (edges == 0 || edges > 15 ||
	    (edges & 3) == 3 || (edges & 12) == 12)
		return 0;

	resize = malloc(sizeof *resize);
	if (!resize)
		return -1;

	resize->edges = edges;
	surface_subsurfaces_boundingbox(shsurf->surface, NULL, NULL,
	                                &resize->width, &resize->height);

	shell_grab_start(&resize->base, &resize_grab_interface, shsurf,
			 seat->pointer, edges);

	return 0;
}

static void
shell_surface_resize(struct wl_client *client, struct wl_resource *resource,
		     struct wl_resource *seat_resource, uint32_t serial,
		     uint32_t edges)
{
	struct weston_seat *seat = seat_resource->data;
	struct shell_surface *shsurf = resource->data;
	struct weston_surface *surface;

	if (shsurf->type == SHELL_SURFACE_FULLSCREEN)
		return;

	surface = weston_surface_get_main_surface(seat->pointer->focus);
	if (seat->pointer->button_count == 0 ||
	    seat->pointer->grab_serial != serial ||
	    surface != shsurf->surface)
		return;

	if (surface_resize(shsurf, seat, edges) < 0)
		wl_resource_post_no_memory(resource);
}

static void
busy_cursor_grab_focus(struct weston_pointer_grab *base)
{
	struct shell_grab *grab = (struct shell_grab *) base;
	struct weston_pointer *pointer = base->pointer;
	struct weston_surface *surface;
	wl_fixed_t sx, sy;

	surface = weston_compositor_pick_surface(pointer->seat->compositor,
						 pointer->x, pointer->y,
						 &sx, &sy);

	if (!grab->shsurf || grab->shsurf->surface != surface) {
		shell_grab_end(grab);
		free(grab);
	}
}

static void
busy_cursor_grab_motion(struct weston_pointer_grab *grab, uint32_t time)
{
}

static void
busy_cursor_grab_button(struct weston_pointer_grab *base,
			uint32_t time, uint32_t button, uint32_t state)
{
	struct shell_grab *grab = (struct shell_grab *) base;
	struct shell_surface *shsurf = grab->shsurf;
	struct weston_seat *seat = grab->grab.pointer->seat;

	if (shsurf && button == BTN_LEFT && state) {
		activate(shsurf->shell, shsurf->surface, seat);
		surface_move(shsurf, seat);
	} else if (shsurf && button == BTN_RIGHT && state) {
		activate(shsurf->shell, shsurf->surface, seat);
		surface_rotate(shsurf, seat);
	}
}

static const struct weston_pointer_grab_interface busy_cursor_grab_interface = {
	busy_cursor_grab_focus,
	busy_cursor_grab_motion,
	busy_cursor_grab_button,
};

static void
set_busy_cursor(struct shell_surface *shsurf, struct weston_pointer *pointer)
{
	struct shell_grab *grab;

	grab = malloc(sizeof *grab);
	if (!grab)
		return;

	shell_grab_start(grab, &busy_cursor_grab_interface, shsurf, pointer,
			 DESKTOP_SHELL_CURSOR_BUSY);
}

static void
end_busy_cursor(struct shell_surface *shsurf, struct weston_pointer *pointer)
{
	struct shell_grab *grab = (struct shell_grab *) pointer->grab;

	if (grab->grab.interface == &busy_cursor_grab_interface &&
	    grab->shsurf == shsurf) {
		shell_grab_end(grab);
		free(grab);
	}
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
	struct weston_seat *seat;

	/* Client is not responding */
	shsurf->unresponsive = 1;

	wl_list_for_each(seat, &shsurf->surface->compositor->seat_list, link)
		if (seat->pointer->focus == shsurf->surface)
			set_busy_cursor(shsurf, seat->pointer);

	return 1;
}

static void
ping_handler(struct weston_surface *surface, uint32_t serial)
{
	struct shell_surface *shsurf = get_shell_surface(surface);
	struct wl_event_loop *loop;
	int ping_timeout = 200;

	if (!shsurf)
		return;
	if (!shsurf->resource.client)
		return;

	if (shsurf->surface == shsurf->shell->grab_surface)
		return;

	if (!shsurf->ping_timer) {
		shsurf->ping_timer = malloc(sizeof *shsurf->ping_timer);
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
handle_pointer_focus(struct wl_listener *listener, void *data)
{
	struct weston_pointer *pointer = data;
	struct weston_surface *surface =
		(struct weston_surface *) pointer->focus;
	struct weston_compositor *compositor;
	struct shell_surface *shsurf;
	uint32_t serial;

	if (!surface)
		return;

	compositor = surface->compositor;
	shsurf = get_shell_surface(surface);

	if (shsurf && shsurf->unresponsive) {
		set_busy_cursor(shsurf, pointer);
	} else {
		serial = wl_display_next_serial(compositor->wl_display);
		ping_handler(surface, serial);
	}
}

static void
create_pointer_focus_listener(struct weston_seat *seat)
{
	struct wl_listener *listener;

	if (!seat->pointer)
		return;

	listener = malloc(sizeof *listener);
	listener->notify = handle_pointer_focus;
	wl_signal_add(&seat->pointer->focus_signal, listener);
}

static void
shell_surface_pong(struct wl_client *client, struct wl_resource *resource,
							uint32_t serial)
{
	struct shell_surface *shsurf = resource->data;
	struct weston_seat *seat;
	struct weston_compositor *ec = shsurf->surface->compositor;

	if (shsurf->ping_timer == NULL)
		/* Just ignore unsolicited pong. */
		return;

	if (shsurf->ping_timer->serial == serial) {
		shsurf->unresponsive = 0;
		wl_list_for_each(seat, &ec->seat_list, link) {
			if(seat->pointer)
				end_busy_cursor(shsurf, seat->pointer);
		}
		ping_timer_destroy(shsurf);
	}
}

static void
shell_surface_set_title(struct wl_client *client,
			struct wl_resource *resource, const char *title)
{
	struct shell_surface *shsurf = resource->data;

	free(shsurf->title);
	shsurf->title = strdup(title);
}

static void
shell_surface_set_class(struct wl_client *client,
			struct wl_resource *resource, const char *class)
{
	struct shell_surface *shsurf = resource->data;

	free(shsurf->class);
	shsurf->class = strdup(class);
}

static struct weston_output *
get_default_output(struct weston_compositor *compositor)
{
	return container_of(compositor->output_list.next,
			    struct weston_output, link);
}

static void
restore_output_mode(struct weston_output *output)
{
	if (output->current != output->origin ||
	    (int32_t)output->scale != output->origin_scale)
		weston_output_switch_mode(output,
					  output->origin,
					  output->origin_scale);
}

static void
restore_all_output_modes(struct weston_compositor *compositor)
{
	struct weston_output *output;

	wl_list_for_each(output, &compositor->output_list, link)
		restore_output_mode(output);
}

static void
shell_unset_fullscreen(struct shell_surface *shsurf)
{
	struct workspace *ws;
	/* undo all fullscreen things here */
	if (shsurf->fullscreen.type == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER &&
	    shell_surface_is_top_fullscreen(shsurf)) {
		restore_output_mode(shsurf->fullscreen_output);
	}
	shsurf->fullscreen.type = WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT;
	shsurf->fullscreen.framerate = 0;
	wl_list_remove(&shsurf->fullscreen.transform.link);
	wl_list_init(&shsurf->fullscreen.transform.link);
	if (shsurf->fullscreen.black_surface)
		weston_surface_destroy(shsurf->fullscreen.black_surface);
	shsurf->fullscreen.black_surface = NULL;
	shsurf->fullscreen_output = NULL;
	weston_surface_set_position(shsurf->surface,
				    shsurf->saved_x, shsurf->saved_y);
	if (shsurf->saved_rotation_valid) {
		wl_list_insert(&shsurf->surface->geometry.transformation_list,
        	               &shsurf->rotation.transform.link);
		shsurf->saved_rotation_valid = false;
	}

	ws = get_current_workspace(shsurf->shell);
	wl_list_remove(&shsurf->surface->layer_link);
	wl_list_insert(&ws->layer.surface_list, &shsurf->surface->layer_link);
}

static void
shell_unset_maximized(struct shell_surface *shsurf)
{
	struct workspace *ws;
	/* undo all maximized things here */
	shsurf->output = get_default_output(shsurf->surface->compositor);
	weston_surface_set_position(shsurf->surface,
				    shsurf->saved_x,
				    shsurf->saved_y);

	if (shsurf->saved_rotation_valid) {
		wl_list_insert(&shsurf->surface->geometry.transformation_list,
						   &shsurf->rotation.transform.link);
		shsurf->saved_rotation_valid = false;
	}

	ws = get_current_workspace(shsurf->shell);
	wl_list_remove(&shsurf->surface->layer_link);
	wl_list_insert(&ws->layer.surface_list, &shsurf->surface->layer_link);
}

static int
reset_shell_surface_type(struct shell_surface *surface)
{
	switch (surface->type) {
	case SHELL_SURFACE_FULLSCREEN:
		shell_unset_fullscreen(surface);
		break;
	case SHELL_SURFACE_MAXIMIZED:
		shell_unset_maximized(surface);
		break;
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
set_surface_type(struct shell_surface *shsurf)
{
	struct weston_surface *surface = shsurf->surface;
	struct weston_surface *pes = shsurf->parent;

	reset_shell_surface_type(shsurf);

	shsurf->type = shsurf->next_type;
	shsurf->next_type = SHELL_SURFACE_NONE;

	switch (shsurf->type) {
	case SHELL_SURFACE_TOPLEVEL:
		break;
	case SHELL_SURFACE_TRANSIENT:
		weston_surface_set_position(surface,
				pes->geometry.x + shsurf->transient.x,
				pes->geometry.y + shsurf->transient.y);
		break;

	case SHELL_SURFACE_MAXIMIZED:
	case SHELL_SURFACE_FULLSCREEN:
		shsurf->saved_x = surface->geometry.x;
		shsurf->saved_y = surface->geometry.y;
		shsurf->saved_position_valid = true;

		if (!wl_list_empty(&shsurf->rotation.transform.link)) {
			wl_list_remove(&shsurf->rotation.transform.link);
			wl_list_init(&shsurf->rotation.transform.link);
			weston_surface_geometry_dirty(shsurf->surface);
			shsurf->saved_rotation_valid = true;
		}
		break;

	default:
		break;
	}
}

static void
set_toplevel(struct shell_surface *shsurf)
{
       shsurf->next_type = SHELL_SURFACE_TOPLEVEL;
}

static void
shell_surface_set_toplevel(struct wl_client *client,
			   struct wl_resource *resource)
{
	struct shell_surface *surface = resource->data;

	set_toplevel(surface);
}

static void
set_transient(struct shell_surface *shsurf,
	      struct weston_surface *parent, int x, int y, uint32_t flags)
{
	/* assign to parents output */
	shsurf->parent = parent;
	shsurf->transient.x = x;
	shsurf->transient.y = y;
	shsurf->transient.flags = flags;
	shsurf->next_type = SHELL_SURFACE_TRANSIENT;
}

static void
shell_surface_set_transient(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *parent_resource,
			    int x, int y, uint32_t flags)
{
	struct shell_surface *shsurf = resource->data;
	struct weston_surface *parent = parent_resource->data;

	set_transient(shsurf, parent, x, y, flags);
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
	struct weston_surface *surface;
	int panel_height = 0;

	if (!output)
		return 0;

	wl_list_for_each(surface, &shell->panel_layer.surface_list, layer_link) {
		if (surface->output == output) {
			panel_height = surface->geometry.height;
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
	else if (es->output)
		shsurf->output = es->output;
	else
		shsurf->output = get_default_output(es->compositor);

	shell = shell_surface_get_shell(shsurf);
	panel_height = get_output_panel_height(shell, shsurf->output);
	edges = WL_SHELL_SURFACE_RESIZE_TOP|WL_SHELL_SURFACE_RESIZE_LEFT;

	shsurf->client->send_configure(shsurf->surface, edges,
				       shsurf->output->width,
				       shsurf->output->height - panel_height);

	shsurf->next_type = SHELL_SURFACE_MAXIMIZED;
}

static void
black_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height);

static struct weston_surface *
create_black_surface(struct weston_compositor *ec,
		     struct weston_surface *fs_surface,
		     float x, float y, int w, int h)
{
	struct weston_surface *surface = NULL;

	surface = weston_surface_create(ec);
	if (surface == NULL) {
		weston_log("no memory\n");
		return NULL;
	}

	surface->configure = black_surface_configure;
	surface->configure_private = fs_surface;
	weston_surface_configure(surface, x, y, w, h);
	weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1);
	pixman_region32_fini(&surface->opaque);
	pixman_region32_init_rect(&surface->opaque, 0, 0, w, h);
	pixman_region32_fini(&surface->input);
	pixman_region32_init_rect(&surface->input, 0, 0, w, h);

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
	float scale, output_aspect, surface_aspect, x, y;
	int32_t surf_x, surf_y, surf_width, surf_height;

	if (shsurf->fullscreen.type != WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER)
		restore_output_mode(output);

	if (!shsurf->fullscreen.black_surface)
		shsurf->fullscreen.black_surface =
			create_black_surface(surface->compositor,
					     surface,
					     output->x, output->y,
					     output->width,
					     output->height);

	wl_list_remove(&shsurf->fullscreen.black_surface->layer_link);
	wl_list_insert(&surface->layer_link,
		       &shsurf->fullscreen.black_surface->layer_link);
	shsurf->fullscreen.black_surface->output = output;

	surface_subsurfaces_boundingbox(surface, &surf_x, &surf_y,
	                                &surf_width, &surf_height);

	switch (shsurf->fullscreen.type) {
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT:
		if (surface->buffer_ref.buffer)
			center_on_output(surface, shsurf->fullscreen_output);
		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE:
		/* 1:1 mapping between surface and output dimensions */
		if (output->width == surf_width &&
			output->height == surf_height) {
			weston_surface_set_position(surface, output->x - surf_x,
			                                     output->y - surf_y);
			break;
		}

		matrix = &shsurf->fullscreen.transform.matrix;
		weston_matrix_init(matrix);

		output_aspect = (float) output->width /
			(float) output->height;
		surface_aspect = (float) surface->geometry.width /
			(float) surface->geometry.height;
		if (output_aspect < surface_aspect)
			scale = (float) output->width /
				(float) surf_width;
		else
			scale = (float) output->height /
				(float) surf_height;

		weston_matrix_scale(matrix, scale, scale, 1);
		wl_list_remove(&shsurf->fullscreen.transform.link);
		wl_list_insert(&surface->geometry.transformation_list,
			       &shsurf->fullscreen.transform.link);
		x = output->x + (output->width - surf_width * scale) / 2 - surf_x;
		y = output->y + (output->height - surf_height * scale) / 2 - surf_y;
		weston_surface_set_position(surface, x, y);

		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER:
		if (shell_surface_is_top_fullscreen(shsurf)) {
			struct weston_mode mode = {0,
				surf_width * surface->buffer_scale,
				surf_height * surface->buffer_scale,
				shsurf->fullscreen.framerate};

			if (weston_output_switch_mode(output, &mode, surface->buffer_scale) == 0) {
				weston_surface_set_position(surface,
							    output->x - surf_x,
							    output->y - surf_y);
				weston_surface_configure(shsurf->fullscreen.black_surface,
					                 output->x - surf_x,
					                 output->y - surf_y,
							 output->width,
							 output->height);
				break;
			} else {
				restore_output_mode(output);
				center_on_output(surface, output);
			}
		}
		break;
	case WL_SHELL_SURFACE_FULLSCREEN_METHOD_FILL:
		center_on_output(surface, output);
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
					     output->width,
					     output->height);

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
set_fullscreen(struct shell_surface *shsurf,
	       uint32_t method,
	       uint32_t framerate,
	       struct weston_output *output)
{
	struct weston_surface *es = shsurf->surface;

	if (output)
		shsurf->output = output;
	else if (es->output)
		shsurf->output = es->output;
	else
		shsurf->output = get_default_output(es->compositor);

	shsurf->fullscreen_output = shsurf->output;
	shsurf->fullscreen.type = method;
	shsurf->fullscreen.framerate = framerate;
	shsurf->next_type = SHELL_SURFACE_FULLSCREEN;

	shsurf->client->send_configure(shsurf->surface, 0,
				       shsurf->output->width,
				       shsurf->output->height);
}

static void
shell_surface_set_fullscreen(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t method,
			     uint32_t framerate,
			     struct wl_resource *output_resource)
{
	struct shell_surface *shsurf = resource->data;
	struct weston_output *output;

	if (output_resource)
		output = output_resource->data;
	else
		output = NULL;

	set_fullscreen(shsurf, method, framerate, output);
}

static const struct weston_pointer_grab_interface popup_grab_interface;

static void
destroy_shell_seat(struct wl_listener *listener, void *data)
{
	struct shell_seat *shseat =
		container_of(listener,
			     struct shell_seat, seat_destroy_listener);
	struct shell_surface *shsurf, *prev = NULL;

	if (shseat->popup_grab.grab.interface == &popup_grab_interface) {
		weston_pointer_end_grab(shseat->popup_grab.grab.pointer);
		shseat->popup_grab.client = NULL;

		wl_list_for_each(shsurf, &shseat->popup_grab.surfaces_list, popup.grab_link) {
			shsurf->popup.shseat = NULL;
			if (prev) {
				wl_list_init(&prev->popup.grab_link);
			}
			prev = shsurf;
		}
		wl_list_init(&prev->popup.grab_link);
	}

	wl_list_remove(&shseat->seat_destroy_listener.link);
	free(shseat);
}

static struct shell_seat *
create_shell_seat(struct weston_seat *seat)
{
	struct shell_seat *shseat;

	shseat = calloc(1, sizeof *shseat);
	if (!shseat) {
		weston_log("no memory to allocate shell seat\n");
		return NULL;
	}

	shseat->seat = seat;
	wl_list_init(&shseat->popup_grab.surfaces_list);

	shseat->seat_destroy_listener.notify = destroy_shell_seat;
	wl_signal_add(&seat->destroy_signal,
	              &shseat->seat_destroy_listener);

	return shseat;
}

static struct shell_seat *
get_shell_seat(struct weston_seat *seat)
{
	struct wl_listener *listener;

	listener = wl_signal_get(&seat->destroy_signal, destroy_shell_seat);
	if (listener == NULL)
		return create_shell_seat(seat);

	return container_of(listener,
			    struct shell_seat, seat_destroy_listener);
}

static void
popup_grab_focus(struct weston_pointer_grab *grab)
{
	struct weston_pointer *pointer = grab->pointer;
	struct weston_surface *surface;
	struct shell_seat *shseat =
	    container_of(grab, struct shell_seat, popup_grab.grab);
	struct wl_client *client = shseat->popup_grab.client;
	wl_fixed_t sx, sy;

	surface = weston_compositor_pick_surface(pointer->seat->compositor,
						 pointer->x, pointer->y,
						 &sx, &sy);

	if (surface && wl_resource_get_client(surface->resource) == client) {
		weston_pointer_set_focus(pointer, surface, sx, sy);
	} else {
		weston_pointer_set_focus(pointer, NULL,
					 wl_fixed_from_int(0),
					 wl_fixed_from_int(0));
	}
}

static void
popup_grab_motion(struct weston_pointer_grab *grab, uint32_t time)
{
	struct weston_pointer *pointer = grab->pointer;
	wl_fixed_t sx, sy;

	if (pointer->focus_resource) {
		weston_surface_from_global_fixed(pointer->focus,
						 pointer->x, pointer->y,
						 &sx, &sy);
		wl_pointer_send_motion(pointer->focus_resource, time, sx, sy);
	}
}

static void
popup_grab_button(struct weston_pointer_grab *grab,
		  uint32_t time, uint32_t button, uint32_t state_w)
{
	struct wl_resource *resource;
	struct shell_seat *shseat =
	    container_of(grab, struct shell_seat, popup_grab.grab);
	struct wl_display *display;
	enum wl_pointer_button_state state = state_w;
	uint32_t serial;

	resource = grab->pointer->focus_resource;
	if (resource) {
		display = wl_client_get_display(resource->client);
		serial = wl_display_get_serial(display);
		wl_pointer_send_button(resource, serial, time, button, state);
	} else if (state == WL_POINTER_BUTTON_STATE_RELEASED &&
		   (shseat->popup_grab.initial_up ||
		    time - shseat->seat->pointer->grab_time > 500)) {
		popup_grab_end(grab->pointer);
	}

	if (state == WL_POINTER_BUTTON_STATE_RELEASED)
		shseat->popup_grab.initial_up = 1;
}

static const struct weston_pointer_grab_interface popup_grab_interface = {
	popup_grab_focus,
	popup_grab_motion,
	popup_grab_button,
};

static void
popup_grab_end(struct weston_pointer *pointer)
{
	struct weston_pointer_grab *grab = pointer->grab;
	struct shell_seat *shseat =
	    container_of(grab, struct shell_seat, popup_grab.grab);
	struct shell_surface *shsurf;
	struct shell_surface *prev = NULL;

	if (pointer->grab->interface == &popup_grab_interface) {
		weston_pointer_end_grab(grab->pointer);
		shseat->popup_grab.client = NULL;
		shseat->popup_grab.grab.interface = NULL;
		assert(!wl_list_empty(&shseat->popup_grab.surfaces_list));
		/* Send the popup_done event to all the popups open */
		wl_list_for_each(shsurf, &shseat->popup_grab.surfaces_list, popup.grab_link) {
			wl_shell_surface_send_popup_done(&shsurf->resource);
			shsurf->popup.shseat = NULL;
			if (prev) {
				wl_list_init(&prev->popup.grab_link);
			}
			prev = shsurf;
		}
		wl_list_init(&prev->popup.grab_link);
		wl_list_init(&shseat->popup_grab.surfaces_list);
	}
}

static void
add_popup_grab(struct shell_surface *shsurf, struct shell_seat *shseat)
{
	struct weston_seat *seat = shseat->seat;

	if (wl_list_empty(&shseat->popup_grab.surfaces_list)) {
		shseat->popup_grab.client = shsurf->resource.client;
		shseat->popup_grab.grab.interface = &popup_grab_interface;
		/* We must make sure here that this popup was opened after
		 * a mouse press, and not just by moving around with other
		 * popups already open. */
		if (shseat->seat->pointer->button_count > 0)
			shseat->popup_grab.initial_up = 0;

		weston_pointer_start_grab(seat->pointer, &shseat->popup_grab.grab);
	}
	wl_list_insert(&shseat->popup_grab.surfaces_list, &shsurf->popup.grab_link);
}

static void
remove_popup_grab(struct shell_surface *shsurf)
{
	struct shell_seat *shseat = shsurf->popup.shseat;

	wl_list_remove(&shsurf->popup.grab_link);
	wl_list_init(&shsurf->popup.grab_link);
	if (wl_list_empty(&shseat->popup_grab.surfaces_list)) {
		weston_pointer_end_grab(shseat->popup_grab.grab.pointer);
		shseat->popup_grab.grab.interface = NULL;
	}
}

static void
shell_map_popup(struct shell_surface *shsurf)
{
	struct shell_seat *shseat = shsurf->popup.shseat;
	struct weston_surface *es = shsurf->surface;
	struct weston_surface *parent = shsurf->parent;

	es->output = parent->output;

	weston_surface_set_transform_parent(es, parent);
	weston_surface_set_position(es, shsurf->popup.x, shsurf->popup.y);
	weston_surface_update_transform(es);

	if (shseat->seat->pointer->grab_serial == shsurf->popup.serial) {
		add_popup_grab(shsurf, shseat);
	} else {
		wl_shell_surface_send_popup_done(&shsurf->resource);
		shseat->popup_grab.client = NULL;
	}
}

static void
shell_surface_set_popup(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *seat_resource,
			uint32_t serial,
			struct wl_resource *parent_resource,
			int32_t x, int32_t y, uint32_t flags)
{
	struct shell_surface *shsurf = resource->data;

	shsurf->type = SHELL_SURFACE_POPUP;
	shsurf->parent = parent_resource->data;
	shsurf->popup.shseat = get_shell_seat(seat_resource->data);
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
	shell_surface_set_maximized,
	shell_surface_set_title,
	shell_surface_set_class
};

static void
destroy_shell_surface(struct shell_surface *shsurf)
{
	if (!wl_list_empty(&shsurf->popup.grab_link)) {
		remove_popup_grab(shsurf);
	}

	if (shsurf->fullscreen.type == WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER &&
	    shell_surface_is_top_fullscreen(shsurf))
		restore_output_mode (shsurf->fullscreen_output);

	if (shsurf->fullscreen.black_surface)
		weston_surface_destroy(shsurf->fullscreen.black_surface);

	/* As destroy_resource() use wl_list_for_each_safe(),
	 * we can always remove the listener.
	 */
	wl_list_remove(&shsurf->surface_destroy_listener.link);
	shsurf->surface->configure = NULL;
	ping_timer_destroy(shsurf);
	free(shsurf->title);

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

	if (shsurf->resource.client) {
		wl_resource_destroy(&shsurf->resource);
	} else {
		wl_signal_emit(&shsurf->resource.destroy_signal,
			       &shsurf->resource);
		destroy_shell_surface(shsurf);
	}
}

static void
shell_surface_configure(struct weston_surface *, int32_t, int32_t, int32_t, int32_t);

static struct shell_surface *
get_shell_surface(struct weston_surface *surface)
{
	if (surface->configure == shell_surface_configure)
		return surface->configure_private;
	else
		return NULL;
}

static 	struct shell_surface *
create_shell_surface(void *shell, struct weston_surface *surface,
		     const struct weston_shell_client *client)
{
	struct shell_surface *shsurf;

	if (surface->configure) {
		weston_log("surface->configure already set\n");
		return NULL;
	}

	shsurf = calloc(1, sizeof *shsurf);
	if (!shsurf) {
		weston_log("no memory to allocate shell surface\n");
		return NULL;
	}

	surface->configure = shell_surface_configure;
	surface->configure_private = shsurf;

	shsurf->shell = (struct desktop_shell *) shell;
	shsurf->unresponsive = 0;
	shsurf->saved_position_valid = false;
	shsurf->saved_rotation_valid = false;
	shsurf->surface = surface;
	shsurf->fullscreen.type = WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT;
	shsurf->fullscreen.framerate = 0;
	shsurf->fullscreen.black_surface = NULL;
	shsurf->ping_timer = NULL;
	wl_list_init(&shsurf->fullscreen.transform.link);

	wl_signal_init(&shsurf->resource.destroy_signal);
	shsurf->surface_destroy_listener.notify = shell_handle_surface_destroy;
	wl_signal_add(&surface->destroy_signal,
		      &shsurf->surface_destroy_listener);

	/* init link so its safe to always remove it in destroy_shell_surface */
	wl_list_init(&shsurf->link);
	wl_list_init(&shsurf->popup.grab_link);

	/* empty when not in use */
	wl_list_init(&shsurf->rotation.transform.link);
	weston_matrix_init(&shsurf->rotation.rotation);

	wl_list_init(&shsurf->workspace_transform.link);

	shsurf->type = SHELL_SURFACE_NONE;
	shsurf->next_type = SHELL_SURFACE_NONE;

	shsurf->client = client;

	return shsurf;
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

	shsurf = create_shell_surface(shell, surface, &shell_client);
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
shell_fade(struct desktop_shell *shell, enum fade_type type);

static int
screensaver_timeout(void *data)
{
	struct desktop_shell *shell = data;

	shell_fade(shell, FADE_OUT);

	return 1;
}

static void
handle_screensaver_sigchld(struct weston_process *proc, int status)
{
	struct desktop_shell *shell =
		container_of(proc, struct desktop_shell, screensaver.process);

	proc->pid = 0;

	if (shell->locked)
		weston_compositor_sleep(shell->compositor);
}

static void
launch_screensaver(struct desktop_shell *shell)
{
	if (shell->screensaver.binding)
		return;

	if (!shell->screensaver.path) {
		weston_compositor_sleep(shell->compositor);
		return;
	}

	if (shell->screensaver.process.pid != 0) {
		weston_log("old screensaver still running\n");
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
configure_static_surface(struct weston_surface *es, struct weston_layer *layer, int32_t width, int32_t height)
{
	struct weston_surface *s, *next;

	if (width == 0)
		return;

	wl_list_for_each_safe(s, next, &layer->surface_list, layer_link) {
		if (s->output == es->output && s != es) {
			weston_surface_unmap(s);
			s->configure = NULL;
		}
	}

	weston_surface_configure(es, es->output->x, es->output->y, width, height);

	if (wl_list_empty(&es->layer_link)) {
		wl_list_insert(&layer->surface_list, &es->layer_link);
		weston_compositor_schedule_repaint(es->compositor);
	}
}

static void
background_configure(struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
	struct desktop_shell *shell = es->configure_private;

	configure_static_surface(es, &shell->background_layer, width, height);
}

static void
desktop_shell_set_background(struct wl_client *client,
			     struct wl_resource *resource,
			     struct wl_resource *output_resource,
			     struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = resource->data;
	struct weston_surface *surface = surface_resource->data;

	if (surface->configure) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface role already assigned");
		return;
	}

	surface->configure = background_configure;
	surface->configure_private = shell;
	surface->output = output_resource->data;
	desktop_shell_send_configure(resource, 0,
				     surface_resource,
				     surface->output->width,
				     surface->output->height);
}

static void
panel_configure(struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
	struct desktop_shell *shell = es->configure_private;

	configure_static_surface(es, &shell->panel_layer, width, height);
}

static void
desktop_shell_set_panel(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *output_resource,
			struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = resource->data;
	struct weston_surface *surface = surface_resource->data;

	if (surface->configure) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface role already assigned");
		return;
	}

	surface->configure = panel_configure;
	surface->configure_private = shell;
	surface->output = output_resource->data;
	desktop_shell_send_configure(resource, 0,
				     surface_resource,
				     surface->output->width,
				     surface->output->height);
}

static void
lock_surface_configure(struct weston_surface *surface, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
	struct desktop_shell *shell = surface->configure_private;

	if (width == 0)
		return;

	surface->geometry.width = width;
	surface->geometry.height = height;
	center_on_output(surface, get_default_output(shell->compositor));

	if (!weston_surface_is_mapped(surface)) {
		wl_list_insert(&shell->lock_layer.surface_list,
			       &surface->layer_link);
		weston_surface_update_transform(surface);
		shell_fade(shell, FADE_IN);
	}
}

static void
handle_lock_surface_destroy(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
	    container_of(listener, struct desktop_shell, lock_surface_listener);

	weston_log("lock surface gone\n");
	shell->lock_surface = NULL;
}

static void
desktop_shell_set_lock_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = resource->data;
	struct weston_surface *surface = surface_resource->data;

	shell->prepare_event_sent = false;

	if (!shell->locked)
		return;

	shell->lock_surface = surface;

	shell->lock_surface_listener.notify = handle_lock_surface_destroy;
	wl_signal_add(&surface_resource->destroy_signal,
		      &shell->lock_surface_listener);

	surface->configure = lock_surface_configure;
	surface->configure_private = shell;
}

static void
resume_desktop(struct desktop_shell *shell)
{
	struct workspace *ws = get_current_workspace(shell);

	terminate_screensaver(shell);

	wl_list_remove(&shell->lock_layer.link);
	wl_list_insert(&shell->compositor->cursor_layer.link,
		       &shell->fullscreen_layer.link);
	wl_list_insert(&shell->fullscreen_layer.link,
		       &shell->panel_layer.link);
	if (shell->showing_input_panels) {
		wl_list_insert(&shell->panel_layer.link,
			       &shell->input_panel_layer.link);
		wl_list_insert(&shell->input_panel_layer.link,
			       &ws->layer.link);
	} else {
		wl_list_insert(&shell->panel_layer.link, &ws->layer.link);
	}

	restore_focus_state(shell, get_current_workspace(shell));

	shell->locked = false;
	shell_fade(shell, FADE_IN);
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

static void
desktop_shell_set_grab_surface(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = resource->data;

	shell->grab_surface = surface_resource->data;
}

static void
desktop_shell_desktop_ready(struct wl_client *client,
			    struct wl_resource *resource)
{
	struct desktop_shell *shell = resource->data;

	shell_fade_startup(shell);
}

static const struct desktop_shell_interface desktop_shell_implementation = {
	desktop_shell_set_background,
	desktop_shell_set_panel,
	desktop_shell_set_lock_surface,
	desktop_shell_unlock,
	desktop_shell_set_grab_surface,
	desktop_shell_desktop_ready
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
move_binding(struct weston_seat *seat, uint32_t time, uint32_t button, void *data)
{
	struct weston_surface *focus =
		(struct weston_surface *) seat->pointer->focus;
	struct weston_surface *surface;
	struct shell_surface *shsurf;

	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (shsurf == NULL || shsurf->type == SHELL_SURFACE_FULLSCREEN ||
	    shsurf->type == SHELL_SURFACE_MAXIMIZED)
		return;

	surface_move(shsurf, (struct weston_seat *) seat);
}

static void
resize_binding(struct weston_seat *seat, uint32_t time, uint32_t button, void *data)
{
	struct weston_surface *focus =
		(struct weston_surface *) seat->pointer->focus;
	struct weston_surface *surface;
	uint32_t edges = 0;
	int32_t x, y;
	struct shell_surface *shsurf;

	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (!shsurf || shsurf->type == SHELL_SURFACE_FULLSCREEN ||
	    shsurf->type == SHELL_SURFACE_MAXIMIZED)
		return;

	weston_surface_from_global(surface,
				   wl_fixed_to_int(seat->pointer->grab_x),
				   wl_fixed_to_int(seat->pointer->grab_y),
				   &x, &y);

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

	surface_resize(shsurf, (struct weston_seat *) seat, edges);
}

static void
surface_opacity_binding(struct weston_seat *seat, uint32_t time, uint32_t axis,
			wl_fixed_t value, void *data)
{
	float step = 0.005;
	struct shell_surface *shsurf;
	struct weston_surface *focus =
		(struct weston_surface *) seat->pointer->focus;
	struct weston_surface *surface;

	/* XXX: broken for windows containing sub-surfaces */
	surface = weston_surface_get_main_surface(focus);
	if (surface == NULL)
		return;

	shsurf = get_shell_surface(surface);
	if (!shsurf)
		return;

	surface->alpha -= wl_fixed_to_double(value) * step;

	if (surface->alpha > 1.0)
		surface->alpha = 1.0;
	if (surface->alpha < step)
		surface->alpha = step;

	weston_surface_geometry_dirty(surface);
	weston_surface_damage(surface);
}

static void
do_zoom(struct weston_seat *seat, uint32_t time, uint32_t key, uint32_t axis,
	wl_fixed_t value)
{
	struct weston_seat *ws = (struct weston_seat *) seat;
	struct weston_compositor *compositor = ws->compositor;
	struct weston_output *output;
	float increment;

	wl_list_for_each(output, &compositor->output_list, link) {
		if (pixman_region32_contains_point(&output->region,
						   wl_fixed_to_double(seat->pointer->x),
						   wl_fixed_to_double(seat->pointer->y),
						   NULL)) {
			if (key == KEY_PAGEUP)
				increment = output->zoom.increment;
			else if (key == KEY_PAGEDOWN)
				increment = -output->zoom.increment;
			else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
				/* For every pixel zoom 20th of a step */
				increment = output->zoom.increment *
					    -wl_fixed_to_double(value) / 20.0;
			else
				increment = 0;

			output->zoom.level += increment;

			if (output->zoom.level < 0.0)
				output->zoom.level = 0.0;
			else if (output->zoom.level > output->zoom.max_level)
				output->zoom.level = output->zoom.max_level;
			else if (!output->zoom.active) {
				output->zoom.active = 1;
				output->disable_planes++;
			}

			output->zoom.spring_z.target = output->zoom.level;

			weston_output_update_zoom(output, output->zoom.type);
		}
	}
}

static void
zoom_axis_binding(struct weston_seat *seat, uint32_t time, uint32_t axis,
		  wl_fixed_t value, void *data)
{
	do_zoom(seat, time, 0, axis, value);
}

static void
zoom_key_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		 void *data)
{
	do_zoom(seat, time, key, 0, 0);
}

static void
terminate_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		  void *data)
{
	struct weston_compositor *compositor = data;

	wl_display_terminate(compositor->wl_display);
}

static void
rotate_grab_motion(struct weston_pointer_grab *grab, uint32_t time)
{
	struct rotate_grab *rotate =
		container_of(grab, struct rotate_grab, base.grab);
	struct weston_pointer *pointer = grab->pointer;
	struct shell_surface *shsurf = rotate->base.shsurf;
	struct weston_surface *surface;
	float cx, cy, dx, dy, cposx, cposy, dposx, dposy, r;

	if (!shsurf)
		return;

	surface = shsurf->surface;

	cx = 0.5f * surface->geometry.width;
	cy = 0.5f * surface->geometry.height;

	dx = wl_fixed_to_double(pointer->x) - rotate->center.x;
	dy = wl_fixed_to_double(pointer->y) - rotate->center.y;
	r = sqrtf(dx * dx + dy * dy);

	wl_list_remove(&shsurf->rotation.transform.link);
	weston_surface_geometry_dirty(shsurf->surface);

	if (r > 20.0f) {
		struct weston_matrix *matrix =
			&shsurf->rotation.transform.matrix;

		weston_matrix_init(&rotate->rotation);
		weston_matrix_rotate_xy(&rotate->rotation, dx / r, dy / r);

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
rotate_grab_button(struct weston_pointer_grab *grab,
		   uint32_t time, uint32_t button, uint32_t state_w)
{
	struct rotate_grab *rotate =
		container_of(grab, struct rotate_grab, base.grab);
	struct weston_pointer *pointer = grab->pointer;
	struct shell_surface *shsurf = rotate->base.shsurf;
	enum wl_pointer_button_state state = state_w;

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		if (shsurf)
			weston_matrix_multiply(&shsurf->rotation.rotation,
					       &rotate->rotation);
		shell_grab_end(&rotate->base);
		free(rotate);
	}
}

static const struct weston_pointer_grab_interface rotate_grab_interface = {
	noop_grab_focus,
	rotate_grab_motion,
	rotate_grab_button,
};

static void
surface_rotate(struct shell_surface *surface, struct weston_seat *seat)
{
	struct rotate_grab *rotate;
	float dx, dy;
	float r;

	rotate = malloc(sizeof *rotate);
	if (!rotate)
		return;

	weston_surface_to_global_float(surface->surface,
				       surface->surface->geometry.width / 2,
				       surface->surface->geometry.height / 2,
				       &rotate->center.x, &rotate->center.y);

	dx = wl_fixed_to_double(seat->pointer->x) - rotate->center.x;
	dy = wl_fixed_to_double(seat->pointer->y) - rotate->center.y;
	r = sqrtf(dx * dx + dy * dy);
	if (r > 20.0f) {
		struct weston_matrix inverse;

		weston_matrix_init(&inverse);
		weston_matrix_rotate_xy(&inverse, dx / r, -dy / r);
		weston_matrix_multiply(&surface->rotation.rotation, &inverse);

		weston_matrix_init(&rotate->rotation);
		weston_matrix_rotate_xy(&rotate->rotation, dx / r, dy / r);
	} else {
		weston_matrix_init(&surface->rotation.rotation);
		weston_matrix_init(&rotate->rotation);
	}

	shell_grab_start(&rotate->base, &rotate_grab_interface, surface,
			 seat->pointer, DESKTOP_SHELL_CURSOR_ARROW);
}

static void
rotate_binding(struct weston_seat *seat, uint32_t time, uint32_t button,
	       void *data)
{
	struct weston_surface *focus =
		(struct weston_surface *) seat->pointer->focus;
	struct weston_surface *base_surface;
	struct shell_surface *surface;

	base_surface = weston_surface_get_main_surface(focus);
	if (base_surface == NULL)
		return;

	surface = get_shell_surface(base_surface);
	if (!surface || surface->type == SHELL_SURFACE_FULLSCREEN ||
	    surface->type == SHELL_SURFACE_MAXIMIZED)
		return;

	surface_rotate(surface, seat);
}

static void
lower_fullscreen_layer(struct desktop_shell *shell)
{
	struct workspace *ws;
	struct weston_surface *surface, *prev;

	ws = get_current_workspace(shell);
	wl_list_for_each_reverse_safe(surface, prev,
				      &shell->fullscreen_layer.surface_list,
				      layer_link)
		weston_surface_restack(surface, &ws->layer.surface_list);
}

static void
activate(struct desktop_shell *shell, struct weston_surface *es,
	 struct weston_seat *seat)
{
	struct weston_surface *main_surface;
	struct focus_state *state;
	struct workspace *ws;

	main_surface = weston_surface_get_main_surface(es);

	weston_surface_activate(es, seat);

	state = ensure_focus_state(shell, seat);
	if (state == NULL)
		return;

	state->keyboard_focus = es;
	wl_list_remove(&state->surface_destroy_listener.link);
	wl_signal_add(&es->destroy_signal, &state->surface_destroy_listener);

	switch (get_shell_surface_type(main_surface)) {
	case SHELL_SURFACE_FULLSCREEN:
		/* should on top of panels */
		shell_stack_fullscreen(get_shell_surface(main_surface));
		shell_configure_fullscreen(get_shell_surface(main_surface));
		break;
	default:
		restore_all_output_modes(shell->compositor);
		ws = get_current_workspace(shell);
		weston_surface_restack(main_surface, &ws->layer.surface_list);
		break;
	}
}

/* no-op func for checking black surface */
static void
black_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
}

static bool
is_black_surface (struct weston_surface *es, struct weston_surface **fs_surface)
{
	if (es->configure == black_surface_configure) {
		if (fs_surface)
			*fs_surface = (struct weston_surface *)es->configure_private;
		return true;
	}
	return false;
}

static void
click_to_activate_binding(struct weston_seat *seat, uint32_t time, uint32_t button,
			  void *data)
{
	struct weston_seat *ws = (struct weston_seat *) seat;
	struct desktop_shell *shell = data;
	struct weston_surface *focus;
	struct weston_surface *main_surface;

	focus = (struct weston_surface *) seat->pointer->focus;
	if (!focus)
		return;

	if (is_black_surface(focus, &main_surface))
		focus = main_surface;

	main_surface = weston_surface_get_main_surface(focus);
	if (get_shell_surface_type(main_surface) == SHELL_SURFACE_NONE)
		return;

	if (seat->pointer->grab == &seat->pointer->default_grab)
		activate(shell, focus, ws);
}

static void
lock(struct desktop_shell *shell)
{
	struct workspace *ws = get_current_workspace(shell);

	if (shell->locked) {
		weston_compositor_sleep(shell->compositor);
		return;
	}

	shell->locked = true;

	/* Hide all surfaces by removing the fullscreen, panel and
	 * toplevel layers.  This way nothing else can show or receive
	 * input events while we are locked. */

	wl_list_remove(&shell->panel_layer.link);
	wl_list_remove(&shell->fullscreen_layer.link);
	if (shell->showing_input_panels)
		wl_list_remove(&shell->input_panel_layer.link);
	wl_list_remove(&ws->layer.link);
	wl_list_insert(&shell->compositor->cursor_layer.link,
		       &shell->lock_layer.link);

	launch_screensaver(shell);

	/* TODO: disable bindings that should not work while locked. */

	/* All this must be undone in resume_desktop(). */
}

static void
unlock(struct desktop_shell *shell)
{
	if (!shell->locked || shell->lock_surface) {
		shell_fade(shell, FADE_IN);
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
shell_fade_done(struct weston_surface_animation *animation, void *data)
{
	struct desktop_shell *shell = data;

	shell->fade.animation = NULL;

	switch (shell->fade.type) {
	case FADE_IN:
		weston_surface_destroy(shell->fade.surface);
		shell->fade.surface = NULL;
		break;
	case FADE_OUT:
		lock(shell);
		break;
	}
}

static struct weston_surface *
shell_fade_create_surface(struct desktop_shell *shell)
{
	struct weston_compositor *compositor = shell->compositor;
	struct weston_surface *surface;

	surface = weston_surface_create(compositor);
	if (!surface)
		return NULL;

	weston_surface_configure(surface, 0, 0, 8192, 8192);
	weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1.0);
	wl_list_insert(&compositor->fade_layer.surface_list,
		       &surface->layer_link);
	pixman_region32_init(&surface->input);

	return surface;
}

static void
shell_fade(struct desktop_shell *shell, enum fade_type type)
{
	float tint;

	switch (type) {
	case FADE_IN:
		tint = 0.0;
		break;
	case FADE_OUT:
		tint = 1.0;
		break;
	default:
		weston_log("shell: invalid fade type\n");
		return;
	}

	shell->fade.type = type;

	if (shell->fade.surface == NULL) {
		shell->fade.surface = shell_fade_create_surface(shell);
		if (!shell->fade.surface)
			return;

		shell->fade.surface->alpha = 1.0 - tint;
		weston_surface_update_transform(shell->fade.surface);
	}

	if (shell->fade.animation)
		weston_fade_update(shell->fade.animation,
				   shell->fade.surface->alpha, tint, 30.0);
	else
		shell->fade.animation =
			weston_fade_run(shell->fade.surface,
					1.0 - tint, tint, 30.0,
					shell_fade_done, shell);
}

static void
do_shell_fade_startup(void *data)
{
	struct desktop_shell *shell = data;

	shell_fade(shell, FADE_IN);
}

static void
shell_fade_startup(struct desktop_shell *shell)
{
	struct wl_event_loop *loop;

	if (!shell->fade.startup_timer)
		return;

	wl_event_source_remove(shell->fade.startup_timer);
	shell->fade.startup_timer = NULL;

	loop = wl_display_get_event_loop(shell->compositor->wl_display);
	wl_event_loop_add_idle(loop, do_shell_fade_startup, shell);
}

static int
fade_startup_timeout(void *data)
{
	struct desktop_shell *shell = data;

	shell_fade_startup(shell);
	return 0;
}

static void
shell_fade_init(struct desktop_shell *shell)
{
	/* Make compositor output all black, and wait for the desktop-shell
	 * client to signal it is ready, then fade in. The timer triggers a
	 * fade-in, in case the desktop-shell client takes too long.
	 */

	struct wl_event_loop *loop;

	if (shell->fade.surface != NULL) {
		weston_log("%s: warning: fade surface already exists\n",
			   __func__);
		return;
	}

	shell->fade.surface = shell_fade_create_surface(shell);
	if (!shell->fade.surface)
		return;

	weston_surface_update_transform(shell->fade.surface);
	weston_surface_damage(shell->fade.surface);

	loop = wl_display_get_event_loop(shell->compositor->wl_display);
	shell->fade.startup_timer =
		wl_event_loop_add_timer(loop, fade_startup_timeout, shell);
	wl_event_source_timer_update(shell->fade.startup_timer, 15000);
}

static void
idle_handler(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, idle_listener);

	shell_fade(shell, FADE_OUT);
	/* lock() is called from shell_fade_done() */
}

static void
wake_handler(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, wake_listener);

	unlock(shell);
}

static void
show_input_panels(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell,
			     show_input_panel_listener);
	struct input_panel_surface *surface, *next;
	struct weston_surface *ws;

	shell->text_input.surface = (struct weston_surface*)data;

	if (shell->showing_input_panels)
		return;

	shell->showing_input_panels = true;

	if (!shell->locked)
		wl_list_insert(&shell->panel_layer.link,
			       &shell->input_panel_layer.link);

	wl_list_for_each_safe(surface, next,
			      &shell->input_panel.surfaces, link) {
		ws = surface->surface;
		if (!ws->buffer_ref.buffer)
			continue;
		wl_list_insert(&shell->input_panel_layer.surface_list,
			       &ws->layer_link);
		weston_surface_geometry_dirty(ws);
		weston_surface_update_transform(ws);
		weston_surface_damage(ws);
		weston_slide_run(ws, ws->geometry.height, 0, NULL, NULL);
	}
}

static void
hide_input_panels(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell,
			     hide_input_panel_listener);
	struct weston_surface *surface, *next;

	if (!shell->showing_input_panels)
		return;

	shell->showing_input_panels = false;

	if (!shell->locked)
		wl_list_remove(&shell->input_panel_layer.link);

	wl_list_for_each_safe(surface, next,
			      &shell->input_panel_layer.surface_list, layer_link)
		weston_surface_unmap(surface);
}

static void
update_input_panels(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell,
			     update_input_panel_listener);

	memcpy(&shell->text_input.cursor_rectangle, data, sizeof(pixman_box32_t));
}

static void
center_on_output(struct weston_surface *surface, struct weston_output *output)
{
	int32_t surf_x, surf_y, width, height;
	float x, y;

	surface_subsurfaces_boundingbox(surface, &surf_x, &surf_y, &width, &height);

	x = output->x + (output->width - width) / 2 - surf_x / 2;
	y = output->y + (output->height - height) / 2 - surf_y / 2;

	weston_surface_configure(surface, x, y, width, height);
}

static void
weston_surface_set_initial_position (struct weston_surface *surface,
				     struct desktop_shell *shell)
{
	struct weston_compositor *compositor = shell->compositor;
	int ix = 0, iy = 0;
	int range_x, range_y;
	int dx, dy, x, y, panel_height;
	struct weston_output *output, *target_output = NULL;
	struct weston_seat *seat;

	/* As a heuristic place the new window on the same output as the
	 * pointer. Falling back to the output containing 0, 0.
	 *
	 * TODO: Do something clever for touch too?
	 */
	wl_list_for_each(seat, &compositor->seat_list, link) {
		if (seat->pointer) {
			ix = wl_fixed_to_int(seat->pointer->x);
			iy = wl_fixed_to_int(seat->pointer->y);
			break;
		}
	}

	wl_list_for_each(output, &compositor->output_list, link) {
		if (pixman_region32_contains_point(&output->region, ix, iy, NULL)) {
			target_output = output;
			break;
		}
	}

	if (!target_output) {
		weston_surface_set_position(surface, 10 + random() % 400,
					   10 + random() % 400);
		return;
	}

	/* Valid range within output where the surface will still be onscreen.
	 * If this is negative it means that the surface is bigger than
	 * output.
	 */
	panel_height = get_output_panel_height(shell, target_output);
	range_x = target_output->width - surface->geometry.width;
	range_y = (target_output->height - panel_height) -
		  surface->geometry.height;

	if (range_x > 0)
		dx = random() % range_x;
	else
		dx = 0;

	if (range_y > 0)
		dy = panel_height + random() % range_y;
	else
		dy = panel_height;

	x = target_output->x + dx;
	y = target_output->y + dy;

	weston_surface_set_position (surface, x, y);
}

static void
map(struct desktop_shell *shell, struct weston_surface *surface,
    int32_t width, int32_t height, int32_t sx, int32_t sy)
{
	struct weston_compositor *compositor = shell->compositor;
	struct shell_surface *shsurf = get_shell_surface(surface);
	enum shell_surface_type surface_type = shsurf->type;
	struct weston_surface *parent;
	struct weston_seat *seat;
	struct workspace *ws;
	int panel_height = 0;
	int32_t surf_x, surf_y;

	surface->geometry.width = width;
	surface->geometry.height = height;
	weston_surface_geometry_dirty(surface);

	/* initial positioning, see also configure() */
	switch (surface_type) {
	case SHELL_SURFACE_TOPLEVEL:
		weston_surface_set_initial_position(surface, shell);
		break;
	case SHELL_SURFACE_FULLSCREEN:
		center_on_output(surface, shsurf->fullscreen_output);
		shell_map_fullscreen(shsurf);
		break;
	case SHELL_SURFACE_MAXIMIZED:
		/* use surface configure to set the geometry */
		panel_height = get_output_panel_height(shell,surface->output);
		surface_subsurfaces_boundingbox(shsurf->surface, &surf_x, &surf_y,
		                                                 NULL, NULL);
		weston_surface_set_position(surface, shsurf->output->x - surf_x,
		                            shsurf->output->y + panel_height - surf_y);
		break;
	case SHELL_SURFACE_POPUP:
		shell_map_popup(shsurf);
		break;
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
	case SHELL_SURFACE_POPUP:
	case SHELL_SURFACE_TRANSIENT:
		parent = shsurf->parent;
		wl_list_insert(parent->layer_link.prev, &surface->layer_link);
		break;
	case SHELL_SURFACE_FULLSCREEN:
	case SHELL_SURFACE_NONE:
		break;
	default:
		ws = get_current_workspace(shell);
		wl_list_insert(&ws->layer.surface_list, &surface->layer_link);
		break;
	}

	if (surface_type != SHELL_SURFACE_NONE) {
		weston_surface_update_transform(surface);
		if (surface_type == SHELL_SURFACE_MAXIMIZED)
			surface->output = shsurf->output;
	}

	switch (surface_type) {
	case SHELL_SURFACE_TRANSIENT:
		if (shsurf->transient.flags ==
				WL_SHELL_SURFACE_TRANSIENT_INACTIVE)
			break;
	case SHELL_SURFACE_TOPLEVEL:
	case SHELL_SURFACE_FULLSCREEN:
	case SHELL_SURFACE_MAXIMIZED:
		if (!shell->locked) {
			wl_list_for_each(seat, &compositor->seat_list, link)
				activate(shell, surface, seat);
		}
		break;
	default:
		break;
	}

	if (surface_type == SHELL_SURFACE_TOPLEVEL)
	{
		switch (shell->win_animation_type) {
		case ANIMATION_FADE:
			weston_fade_run(surface, 0.0, 1.0, 200.0, NULL, NULL);
			break;
		case ANIMATION_ZOOM:
			weston_zoom_run(surface, 0.8, 1.0, NULL, NULL);
			break;
		default:
			break;
		}
	}
}

static void
configure(struct desktop_shell *shell, struct weston_surface *surface,
	  float x, float y, int32_t width, int32_t height)
{
	enum shell_surface_type surface_type = SHELL_SURFACE_NONE;
	struct shell_surface *shsurf;
	int32_t surf_x, surf_y;

	shsurf = get_shell_surface(surface);
	if (shsurf)
		surface_type = shsurf->type;

	weston_surface_configure(surface, x, y, width, height);

	switch (surface_type) {
	case SHELL_SURFACE_FULLSCREEN:
		shell_stack_fullscreen(shsurf);
		shell_configure_fullscreen(shsurf);
		break;
	case SHELL_SURFACE_MAXIMIZED:
		/* setting x, y and using configure to change that geometry */
		surface_subsurfaces_boundingbox(shsurf->surface, &surf_x, &surf_y,
		                                                 NULL, NULL);
		surface->geometry.x = surface->output->x - surf_x;
		surface->geometry.y = surface->output->y +
		get_output_panel_height(shell,surface->output) - surf_y;
		break;
	case SHELL_SURFACE_TOPLEVEL:
		break;
	default:
		break;
	}

	/* XXX: would a fullscreen surface need the same handling? */
	if (surface->output) {
		weston_surface_update_transform(surface);

		if (surface_type == SHELL_SURFACE_MAXIMIZED)
			surface->output = shsurf->output;
	}
}

static void
shell_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
	struct shell_surface *shsurf = get_shell_surface(es);
	struct desktop_shell *shell = shsurf->shell;

	int type_changed = 0;

	if (!weston_surface_is_mapped(es) && !wl_list_empty(&shsurf->popup.grab_link)) {
		remove_popup_grab(shsurf);
	}

	if (width == 0)
		return;

	if (shsurf->next_type != SHELL_SURFACE_NONE &&
	    shsurf->type != shsurf->next_type) {
		set_surface_type(shsurf);
		type_changed = 1;
	}

	if (!weston_surface_is_mapped(es)) {
		map(shell, es, width, height, sx, sy);
	} else if (type_changed || sx != 0 || sy != 0 ||
		   es->geometry.width != width ||
		   es->geometry.height != height) {
		float from_x, from_y;
		float to_x, to_y;

		weston_surface_to_global_float(es, 0, 0, &from_x, &from_y);
		weston_surface_to_global_float(es, sx, sy, &to_x, &to_y);
		configure(shell, es,
			  es->geometry.x + to_x - from_x,
			  es->geometry.y + to_y - from_y,
			  width, height);
	}
}

static void launch_desktop_shell_process(void *data);

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
		weston_log("weston-desktop-shell died, giving up.\n");
		return;
	}

	weston_log("weston-desktop-shell died, respawning...\n");
	launch_desktop_shell_process(shell);
	shell_fade_startup(shell);
}

static void
launch_desktop_shell_process(void *data)
{
	struct desktop_shell *shell = data;
	const char *shell_exe = LIBEXECDIR "/weston-desktop-shell";

	shell->child.client = weston_client_launch(shell->compositor,
						 &shell->child.process,
						 shell_exe,
						 desktop_shell_sigchld);

	if (!shell->child.client)
		weston_log("not able to start %s\n", shell_exe);
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

		if (version < 2)
			shell_fade_startup(shell);

		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "permission to bind desktop_shell denied");
	wl_resource_destroy(resource);
}

static void
screensaver_configure(struct weston_surface *surface, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
	struct desktop_shell *shell = surface->configure_private;

	if (width == 0)
		return;

	/* XXX: starting weston-screensaver beforehand does not work */
	if (!shell->locked)
		return;

	center_on_output(surface, surface->output);

	if (wl_list_empty(&surface->layer_link)) {
		wl_list_insert(shell->lock_layer.surface_list.prev,
			       &surface->layer_link);
		weston_surface_update_transform(surface);
		wl_event_source_timer_update(shell->screensaver.timer,
					     shell->screensaver.duration);
		shell_fade(shell, FADE_IN);
	}
}

static void
screensaver_set_surface(struct wl_client *client,
			struct wl_resource *resource,
			struct wl_resource *surface_resource,
			struct wl_resource *output_resource)
{
	struct desktop_shell *shell = resource->data;
	struct weston_surface *surface = surface_resource->data;
	struct weston_output *output = output_resource->data;

	surface->configure = screensaver_configure;
	surface->configure_private = shell;
	surface->output = output;
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

static void
input_panel_configure(struct weston_surface *surface, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
	struct input_panel_surface *ip_surface = surface->configure_private;
	struct desktop_shell *shell = ip_surface->shell;
	struct weston_mode *mode;
	float x, y;
	uint32_t show_surface = 0;

	if (width == 0)
		return;

	if (!weston_surface_is_mapped(surface)) {
		if (!shell->showing_input_panels)
			return;

		show_surface = 1;
	}

	fprintf(stderr, "%s panel: %d, output: %p\n", __FUNCTION__, ip_surface->panel, ip_surface->output);

	if (ip_surface->panel) {
		x = shell->text_input.surface->geometry.x + shell->text_input.cursor_rectangle.x2;
		y = shell->text_input.surface->geometry.y + shell->text_input.cursor_rectangle.y2;
	} else {
		mode = ip_surface->output->current;

		x = ip_surface->output->x + (mode->width - width) / 2;
		y = ip_surface->output->y + mode->height - height;
	}

	weston_surface_configure(surface,
				 x, y,
				 width, height);

	if (show_surface) {
		wl_list_insert(&shell->input_panel_layer.surface_list,
			       &surface->layer_link);
		weston_surface_update_transform(surface);
		weston_surface_damage(surface);
		weston_slide_run(surface, surface->geometry.height, 0, NULL, NULL);
	}
}

static void
destroy_input_panel_surface(struct input_panel_surface *input_panel_surface)
{
	wl_list_remove(&input_panel_surface->surface_destroy_listener.link);
	wl_list_remove(&input_panel_surface->link);

	input_panel_surface->surface->configure = NULL;

	free(input_panel_surface);
}

static struct input_panel_surface *
get_input_panel_surface(struct weston_surface *surface)
{
	if (surface->configure == input_panel_configure) {
		return surface->configure_private;
	} else {
		return NULL;
	}
}

static void
input_panel_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct input_panel_surface *ipsurface = container_of(listener,
							     struct input_panel_surface,
							     surface_destroy_listener);

	if (ipsurface->resource.client) {
		wl_resource_destroy(&ipsurface->resource);
	} else {
		wl_signal_emit(&ipsurface->resource.destroy_signal,
			       &ipsurface->resource);
		destroy_input_panel_surface(ipsurface);
	}
}
static struct input_panel_surface *
create_input_panel_surface(struct desktop_shell *shell,
			   struct weston_surface *surface)
{
	struct input_panel_surface *input_panel_surface;

	input_panel_surface = calloc(1, sizeof *input_panel_surface);
	if (!input_panel_surface)
		return NULL;

	surface->configure = input_panel_configure;
	surface->configure_private = input_panel_surface;

	input_panel_surface->shell = shell;

	input_panel_surface->surface = surface;

	wl_signal_init(&input_panel_surface->resource.destroy_signal);
	input_panel_surface->surface_destroy_listener.notify = input_panel_handle_surface_destroy;
	wl_signal_add(&surface->destroy_signal,
		      &input_panel_surface->surface_destroy_listener);

	wl_list_init(&input_panel_surface->link);

	return input_panel_surface;
}

static void
input_panel_surface_set_toplevel(struct wl_client *client,
				 struct wl_resource *resource,
				 struct wl_resource *output_resource,
				 uint32_t position)
{
	struct input_panel_surface *input_panel_surface = resource->data;
	struct desktop_shell *shell = input_panel_surface->shell;

	wl_list_insert(&shell->input_panel.surfaces,
		       &input_panel_surface->link);

	input_panel_surface->output = output_resource->data;
	input_panel_surface->panel = 0;
}

static void
input_panel_surface_set_overlay_panel(struct wl_client *client,
				      struct wl_resource *resource)
{
	struct input_panel_surface *input_panel_surface = resource->data;
	struct desktop_shell *shell = input_panel_surface->shell;

	wl_list_insert(&shell->input_panel.surfaces,
		       &input_panel_surface->link);

	input_panel_surface->panel = 1;
}

static const struct wl_input_panel_surface_interface input_panel_surface_implementation = {
	input_panel_surface_set_toplevel,
	input_panel_surface_set_overlay_panel
};

static void
destroy_input_panel_surface_resource(struct wl_resource *resource)
{
	struct input_panel_surface *ipsurf = resource->data;

	destroy_input_panel_surface(ipsurf);
}

static void
input_panel_get_input_panel_surface(struct wl_client *client,
				    struct wl_resource *resource,
				    uint32_t id,
				    struct wl_resource *surface_resource)
{
	struct weston_surface *surface = surface_resource->data;
	struct desktop_shell *shell = resource->data;
	struct input_panel_surface *ipsurf;

	if (get_input_panel_surface(surface)) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "wl_input_panel::get_input_panel_surface already requested");
		return;
	}

	ipsurf = create_input_panel_surface(shell, surface);
	if (!ipsurf) {
		wl_resource_post_error(surface_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface->configure already set");
		return;
	}

	ipsurf->resource.destroy = destroy_input_panel_surface_resource;
	ipsurf->resource.object.id = id;
	ipsurf->resource.object.interface = &wl_input_panel_surface_interface;
	ipsurf->resource.object.implementation =
		(void (**)(void)) &input_panel_surface_implementation;
	ipsurf->resource.data = ipsurf;

	wl_client_add_resource(client, &ipsurf->resource);
}

static const struct wl_input_panel_interface input_panel_implementation = {
	input_panel_get_input_panel_surface
};

static void
unbind_input_panel(struct wl_resource *resource)
{
	struct desktop_shell *shell = resource->data;

	shell->input_panel.binding = NULL;
	free(resource);
}

static void
bind_input_panel(struct wl_client *client,
	      void *data, uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &wl_input_panel_interface,
					&input_panel_implementation,
					id, shell);

	if (shell->input_panel.binding == NULL) {
		resource->destroy = unbind_input_panel;
		shell->input_panel.binding = resource;
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
	struct weston_keyboard_grab grab;
};

static void
switcher_next(struct switcher *switcher)
{
	struct weston_surface *surface;
	struct weston_surface *first = NULL, *prev = NULL, *next = NULL;
	struct shell_surface *shsurf;
	struct workspace *ws = get_current_workspace(switcher->shell);

	wl_list_for_each(surface, &ws->layer.surface_list, layer_link) {
		switch (get_shell_surface_type(surface)) {
		case SHELL_SURFACE_TOPLEVEL:
		case SHELL_SURFACE_FULLSCREEN:
		case SHELL_SURFACE_MAXIMIZED:
			if (first == NULL)
				first = surface;
			if (prev == switcher->current)
				next = surface;
			prev = surface;
			surface->alpha = 0.25;
			weston_surface_geometry_dirty(surface);
			weston_surface_damage(surface);
			break;
		default:
			break;
		}

		if (is_black_surface(surface, NULL)) {
			surface->alpha = 0.25;
			weston_surface_geometry_dirty(surface);
			weston_surface_damage(surface);
		}
	}

	if (next == NULL)
		next = first;

	if (next == NULL)
		return;

	wl_list_remove(&switcher->listener.link);
	wl_signal_add(&next->destroy_signal, &switcher->listener);

	switcher->current = next;
	next->alpha = 1.0;

	shsurf = get_shell_surface(switcher->current);
	if (shsurf && shsurf->type ==SHELL_SURFACE_FULLSCREEN)
		shsurf->fullscreen.black_surface->alpha = 1.0;
}

static void
switcher_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct switcher *switcher =
		container_of(listener, struct switcher, listener);

	switcher_next(switcher);
}

static void
switcher_destroy(struct switcher *switcher)
{
	struct weston_surface *surface;
	struct weston_keyboard *keyboard = switcher->grab.keyboard;
	struct workspace *ws = get_current_workspace(switcher->shell);

	wl_list_for_each(surface, &ws->layer.surface_list, layer_link) {
		surface->alpha = 1.0;
		weston_surface_damage(surface);
	}

	if (switcher->current)
		activate(switcher->shell, switcher->current,
			 (struct weston_seat *) keyboard->seat);
	wl_list_remove(&switcher->listener.link);
	weston_keyboard_end_grab(keyboard);
	if (keyboard->input_method_resource)
		keyboard->grab = &keyboard->input_method_grab;
	free(switcher);
}

static void
switcher_key(struct weston_keyboard_grab *grab,
	     uint32_t time, uint32_t key, uint32_t state_w)
{
	struct switcher *switcher = container_of(grab, struct switcher, grab);
	enum wl_keyboard_key_state state = state_w;

	if (key == KEY_TAB && state == WL_KEYBOARD_KEY_STATE_PRESSED)
		switcher_next(switcher);
}

static void
switcher_modifier(struct weston_keyboard_grab *grab, uint32_t serial,
		  uint32_t mods_depressed, uint32_t mods_latched,
		  uint32_t mods_locked, uint32_t group)
{
	struct switcher *switcher = container_of(grab, struct switcher, grab);
	struct weston_seat *seat = (struct weston_seat *) grab->keyboard->seat;

	if ((seat->modifier_state & switcher->shell->binding_modifier) == 0)
		switcher_destroy(switcher);
}

static const struct weston_keyboard_grab_interface switcher_grab = {
	switcher_key,
	switcher_modifier,
};

static void
switcher_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		 void *data)
{
	struct desktop_shell *shell = data;
	struct switcher *switcher;

	switcher = malloc(sizeof *switcher);
	switcher->shell = shell;
	switcher->current = NULL;
	switcher->listener.notify = switcher_handle_surface_destroy;
	wl_list_init(&switcher->listener.link);

	restore_all_output_modes(shell->compositor);
	lower_fullscreen_layer(switcher->shell);
	switcher->grab.interface = &switcher_grab;
	weston_keyboard_start_grab(seat->keyboard, &switcher->grab);
	weston_keyboard_set_focus(seat->keyboard, NULL);
	switcher_next(switcher);
}

static void
backlight_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		  void *data)
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

struct debug_binding_grab {
	struct weston_keyboard_grab grab;
	struct weston_seat *seat;
	uint32_t key[2];
	int key_released[2];
};

static void
debug_binding_key(struct weston_keyboard_grab *grab, uint32_t time,
		  uint32_t key, uint32_t state)
{
	struct debug_binding_grab *db = (struct debug_binding_grab *) grab;
	struct wl_resource *resource;
	struct wl_display *display;
	uint32_t serial;
	int send = 0, terminate = 0;
	int check_binding = 1;
	int i;

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		/* Do not run bindings on key releases */
		check_binding = 0;

		for (i = 0; i < 2; i++)
			if (key == db->key[i])
				db->key_released[i] = 1;

		if (db->key_released[0] && db->key_released[1]) {
			/* All key releases been swalled so end the grab */
			terminate = 1;
		} else if (key != db->key[0] && key != db->key[1]) {
			/* Should not swallow release of other keys */
			send = 1;
		}
	} else if (key == db->key[0] && !db->key_released[0]) {
		/* Do not check bindings for the first press of the binding
		 * key. This allows it to be used as a debug shortcut.
		 * We still need to swallow this event. */
		check_binding = 0;
	} else if (db->key[1]) {
		/* If we already ran a binding don't process another one since
		 * we can't keep track of all the binding keys that were
		 * pressed in order to swallow the release events. */
		send = 1;
		check_binding = 0;
	}

	if (check_binding) {
		struct weston_compositor *ec = db->seat->compositor;

		if (weston_compositor_run_debug_binding(ec, db->seat, time,
							key, state)) {
			/* We ran a binding so swallow the press and keep the
			 * grab to swallow the released too. */
			send = 0;
			terminate = 0;
			db->key[1] = key;
		} else {
			/* Terminate the grab since the key pressed is not a
			 * debug binding key. */
			send = 1;
			terminate = 1;
		}
	}

	if (send) {
		resource = grab->keyboard->focus_resource;

		if (resource) {
			display = wl_client_get_display(resource->client);
			serial = wl_display_next_serial(display);
			wl_keyboard_send_key(resource, serial, time, key, state);
		}
	}

	if (terminate) {
		weston_keyboard_end_grab(grab->keyboard);
		if (grab->keyboard->input_method_resource)
			grab->keyboard->grab = &grab->keyboard->input_method_grab;
		free(db);
	}
}

static void
debug_binding_modifiers(struct weston_keyboard_grab *grab, uint32_t serial,
			uint32_t mods_depressed, uint32_t mods_latched,
			uint32_t mods_locked, uint32_t group)
{
	struct wl_resource *resource;

	resource = grab->keyboard->focus_resource;
	if (!resource)
		return;

	wl_keyboard_send_modifiers(resource, serial, mods_depressed,
				   mods_latched, mods_locked, group);
}

struct weston_keyboard_grab_interface debug_binding_keyboard_grab = {
	debug_binding_key,
	debug_binding_modifiers
};

static void
debug_binding(struct weston_seat *seat, uint32_t time, uint32_t key, void *data)
{
	struct debug_binding_grab *grab;

	grab = calloc(1, sizeof *grab);
	if (!grab)
		return;

	grab->seat = (struct weston_seat *) seat;
	grab->key[0] = key;
	grab->grab.interface = &debug_binding_keyboard_grab;
	weston_keyboard_start_grab(seat->keyboard, &grab->grab);
}

static void
force_kill_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		   void *data)
{
	struct weston_surface *focus_surface;
	struct wl_client *client;
	struct desktop_shell *shell = data;
	struct weston_compositor *compositor = shell->compositor;
	pid_t pid;

	focus_surface = seat->keyboard->focus;
	if (!focus_surface)
		return;

	wl_signal_emit(&compositor->kill_signal, focus_surface);

	client = wl_resource_get_client(focus_surface->resource);
	wl_client_get_credentials(client, &pid, NULL, NULL);

	/* Skip clients that we launched ourselves (the credentials of
	 * the socketpair is ours) */
	if (pid == getpid())
		return;

	kill(pid, SIGKILL);
}

static void
workspace_up_binding(struct weston_seat *seat, uint32_t time,
		     uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index = shell->workspaces.current;

	if (shell->locked)
		return;
	if (new_index != 0)
		new_index--;

	change_workspace(shell, new_index);
}

static void
workspace_down_binding(struct weston_seat *seat, uint32_t time,
		       uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index = shell->workspaces.current;

	if (shell->locked)
		return;
	if (new_index < shell->workspaces.num - 1)
		new_index++;

	change_workspace(shell, new_index);
}

static void
workspace_f_binding(struct weston_seat *seat, uint32_t time,
		    uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index;

	if (shell->locked)
		return;
	new_index = key - KEY_F1;
	if (new_index >= shell->workspaces.num)
		new_index = shell->workspaces.num - 1;

	change_workspace(shell, new_index);
}

static void
workspace_move_surface_up_binding(struct weston_seat *seat, uint32_t time,
				  uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index = shell->workspaces.current;

	if (shell->locked)
		return;

	if (new_index != 0)
		new_index--;

	take_surface_to_workspace_by_seat(shell, seat, new_index);
}

static void
workspace_move_surface_down_binding(struct weston_seat *seat, uint32_t time,
				    uint32_t key, void *data)
{
	struct desktop_shell *shell = data;
	unsigned int new_index = shell->workspaces.current;

	if (shell->locked)
		return;

	if (new_index < shell->workspaces.num - 1)
		new_index++;

	take_surface_to_workspace_by_seat(shell, seat, new_index);
}

static void
shell_destroy(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell =
		container_of(listener, struct desktop_shell, destroy_listener);
	struct workspace **ws;

	if (shell->child.client)
		wl_client_destroy(shell->child.client);

	wl_list_remove(&shell->idle_listener.link);
	wl_list_remove(&shell->wake_listener.link);
	wl_list_remove(&shell->show_input_panel_listener.link);
	wl_list_remove(&shell->hide_input_panel_listener.link);

	wl_array_for_each(ws, &shell->workspaces.array)
		workspace_destroy(*ws);
	wl_array_release(&shell->workspaces.array);

	free(shell->screensaver.path);
	free(shell);
}

static void
shell_add_bindings(struct weston_compositor *ec, struct desktop_shell *shell)
{
	uint32_t mod;
	int i, num_workspace_bindings;

	/* fixed bindings */
	weston_compositor_add_key_binding(ec, KEY_BACKSPACE,
				          MODIFIER_CTRL | MODIFIER_ALT,
				          terminate_binding, ec);
	weston_compositor_add_button_binding(ec, BTN_LEFT, 0,
					     click_to_activate_binding,
					     shell);
	weston_compositor_add_axis_binding(ec, WL_POINTER_AXIS_VERTICAL_SCROLL,
				           MODIFIER_SUPER | MODIFIER_ALT,
				           surface_opacity_binding, NULL);
	weston_compositor_add_axis_binding(ec, WL_POINTER_AXIS_VERTICAL_SCROLL,
					   MODIFIER_SUPER, zoom_axis_binding,
					   NULL);

	/* configurable bindings */
	mod = shell->binding_modifier;
	weston_compositor_add_key_binding(ec, KEY_PAGEUP, mod,
					  zoom_key_binding, NULL);
	weston_compositor_add_key_binding(ec, KEY_PAGEDOWN, mod,
					  zoom_key_binding, NULL);
	weston_compositor_add_button_binding(ec, BTN_LEFT, mod, move_binding,
					     shell);
	weston_compositor_add_button_binding(ec, BTN_MIDDLE, mod,
					     resize_binding, shell);

	if (ec->capabilities & WESTON_CAP_ROTATION_ANY)
		weston_compositor_add_button_binding(ec, BTN_RIGHT, mod,
						     rotate_binding, NULL);

	weston_compositor_add_key_binding(ec, KEY_TAB, mod, switcher_binding,
					  shell);
	weston_compositor_add_key_binding(ec, KEY_F9, mod, backlight_binding,
					  ec);
	weston_compositor_add_key_binding(ec, KEY_BRIGHTNESSDOWN, 0,
				          backlight_binding, ec);
	weston_compositor_add_key_binding(ec, KEY_F10, mod, backlight_binding,
					  ec);
	weston_compositor_add_key_binding(ec, KEY_BRIGHTNESSUP, 0,
				          backlight_binding, ec);
	weston_compositor_add_key_binding(ec, KEY_K, mod,
				          force_kill_binding, shell);
	weston_compositor_add_key_binding(ec, KEY_UP, mod,
					  workspace_up_binding, shell);
	weston_compositor_add_key_binding(ec, KEY_DOWN, mod,
					  workspace_down_binding, shell);
	weston_compositor_add_key_binding(ec, KEY_UP, mod | MODIFIER_SHIFT,
					  workspace_move_surface_up_binding,
					  shell);
	weston_compositor_add_key_binding(ec, KEY_DOWN, mod | MODIFIER_SHIFT,
					  workspace_move_surface_down_binding,
					  shell);

	/* Add bindings for mod+F[1-6] for workspace 1 to 6. */
	if (shell->workspaces.num > 1) {
		num_workspace_bindings = shell->workspaces.num;
		if (num_workspace_bindings > 6)
			num_workspace_bindings = 6;
		for (i = 0; i < num_workspace_bindings; i++)
			weston_compositor_add_key_binding(ec, KEY_F1 + i, mod,
							  workspace_f_binding,
							  shell);
	}

	/* Debug bindings */
	weston_compositor_add_key_binding(ec, KEY_SPACE, mod | MODIFIER_SHIFT,
					  debug_binding, shell);
}

WL_EXPORT int
module_init(struct weston_compositor *ec,
	    int *argc, char *argv[])
{
	struct weston_seat *seat;
	struct desktop_shell *shell;
	struct workspace **pws;
	unsigned int i;
	struct wl_event_loop *loop;

	shell = malloc(sizeof *shell);
	if (shell == NULL)
		return -1;

	memset(shell, 0, sizeof *shell);
	shell->compositor = ec;

	shell->destroy_listener.notify = shell_destroy;
	wl_signal_add(&ec->destroy_signal, &shell->destroy_listener);
	shell->idle_listener.notify = idle_handler;
	wl_signal_add(&ec->idle_signal, &shell->idle_listener);
	shell->wake_listener.notify = wake_handler;
	wl_signal_add(&ec->wake_signal, &shell->wake_listener);
	shell->show_input_panel_listener.notify = show_input_panels;
	wl_signal_add(&ec->show_input_panel_signal, &shell->show_input_panel_listener);
	shell->hide_input_panel_listener.notify = hide_input_panels;
	wl_signal_add(&ec->hide_input_panel_signal, &shell->hide_input_panel_listener);
	shell->update_input_panel_listener.notify = update_input_panels;
	wl_signal_add(&ec->update_input_panel_signal, &shell->update_input_panel_listener);
	ec->ping_handler = ping_handler;
	ec->shell_interface.shell = shell;
	ec->shell_interface.create_shell_surface = create_shell_surface;
	ec->shell_interface.set_toplevel = set_toplevel;
	ec->shell_interface.set_transient = set_transient;
	ec->shell_interface.set_fullscreen = set_fullscreen;
	ec->shell_interface.move = surface_move;
	ec->shell_interface.resize = surface_resize;

	wl_list_init(&shell->input_panel.surfaces);

	weston_layer_init(&shell->fullscreen_layer, &ec->cursor_layer.link);
	weston_layer_init(&shell->panel_layer, &shell->fullscreen_layer.link);
	weston_layer_init(&shell->background_layer, &shell->panel_layer.link);
	weston_layer_init(&shell->lock_layer, NULL);
	weston_layer_init(&shell->input_panel_layer, NULL);

	wl_array_init(&shell->workspaces.array);
	wl_list_init(&shell->workspaces.client_list);

	shell_configuration(shell);

	for (i = 0; i < shell->workspaces.num; i++) {
		pws = wl_array_add(&shell->workspaces.array, sizeof *pws);
		if (pws == NULL)
			return -1;

		*pws = workspace_create();
		if (*pws == NULL)
			return -1;
	}
	activate_workspace(shell, 0);

	wl_list_init(&shell->workspaces.anim_sticky_list);
	wl_list_init(&shell->workspaces.animation.link);
	shell->workspaces.animation.frame = animate_workspace_change_frame;

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

	if (wl_display_add_global(ec->wl_display, &wl_input_panel_interface,
				  shell, bind_input_panel) == NULL)
		return -1;

	if (wl_display_add_global(ec->wl_display, &workspace_manager_interface,
				  shell, bind_workspace_manager) == NULL)
		return -1;

	shell->child.deathstamp = weston_compositor_get_time();

	loop = wl_display_get_event_loop(ec->wl_display);
	wl_event_loop_add_idle(loop, launch_desktop_shell_process, shell);

	shell->screensaver.timer =
		wl_event_loop_add_timer(loop, screensaver_timeout, shell);

	wl_list_for_each(seat, &ec->seat_list, link)
		create_pointer_focus_listener(seat);

	shell_add_bindings(ec, shell);

	shell_fade_init(shell);

	return 0;
}
