/*
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2011-2012 Collabora, Ltd.
 * Copyright © 2013 Raspberry Pi Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdint.h>
#include <linux/input.h>

#include "shell.h"
#include "shared/helpers.h"

struct exposay_surface {
	struct desktop_shell *shell;
	struct exposay_output *eoutput;
	struct weston_surface *surface;
	struct weston_view *view;
	struct wl_listener view_destroy_listener;
	struct wl_list link;

	int x;
	int y;
	int width;
	int height;
	double scale;

	int row;
	int column;

	/* The animations only apply a transformation for their own lifetime,
	 * and don't have an option to indefinitely maintain the
	 * transformation in a steady state - so, we apply our own once the
	 * animation has finished. */
	struct weston_transform transform;
};

static void exposay_set_state(struct desktop_shell *shell,
                              enum exposay_target_state state,
			      struct weston_seat *seat);
static void exposay_check_state(struct desktop_shell *shell);

static void
exposay_surface_destroy(struct exposay_surface *esurface)
{
	wl_list_remove(&esurface->link);
	wl_list_remove(&esurface->view_destroy_listener.link);

	if (esurface->shell->exposay.focus_current == esurface->view)
		esurface->shell->exposay.focus_current = NULL;
	if (esurface->shell->exposay.focus_prev == esurface->view)
		esurface->shell->exposay.focus_prev = NULL;

	free(esurface);
}

static void
exposay_in_flight_inc(struct desktop_shell *shell)
{
	shell->exposay.in_flight++;
}

static void
exposay_in_flight_dec(struct desktop_shell *shell)
{
	if (--shell->exposay.in_flight > 0)
		return;

	exposay_check_state(shell);
}

static void
exposay_animate_in_done(struct weston_view_animation *animation, void *data)
{
	struct exposay_surface *esurface = data;

	wl_list_insert(&esurface->view->geometry.transformation_list,
	               &esurface->transform.link);
	weston_matrix_init(&esurface->transform.matrix);
	weston_matrix_scale(&esurface->transform.matrix,
	                    esurface->scale, esurface->scale, 1.0f);
	weston_matrix_translate(&esurface->transform.matrix,
	                        esurface->x - esurface->view->geometry.x,
				esurface->y - esurface->view->geometry.y,
				0);

	weston_view_geometry_dirty(esurface->view);
	weston_compositor_schedule_repaint(esurface->view->surface->compositor);

	exposay_in_flight_dec(esurface->shell);
}

static void
exposay_animate_in(struct exposay_surface *esurface)
{
	exposay_in_flight_inc(esurface->shell);

	weston_move_scale_run(esurface->view,
	                      esurface->x - esurface->view->geometry.x,
	                      esurface->y - esurface->view->geometry.y,
			      1.0, esurface->scale, 0,
	                      exposay_animate_in_done, esurface);
}

static void
exposay_animate_out_done(struct weston_view_animation *animation, void *data)
{
	struct exposay_surface *esurface = data;
	struct desktop_shell *shell = esurface->shell;

	exposay_surface_destroy(esurface);

	exposay_in_flight_dec(shell);
}

static void
exposay_animate_out(struct exposay_surface *esurface)
{
	exposay_in_flight_inc(esurface->shell);

	/* Remove the static transformation set up by
	 * exposay_transform_in_done(). */
	wl_list_remove(&esurface->transform.link);
	weston_view_geometry_dirty(esurface->view);

	weston_move_scale_run(esurface->view,
	                      esurface->x - esurface->view->geometry.x,
	                      esurface->y - esurface->view->geometry.y,
			      1.0, esurface->scale, 1,
			      exposay_animate_out_done, esurface);
}

static void
exposay_highlight_surface(struct desktop_shell *shell,
                          struct exposay_surface *esurface)
{
	struct weston_view *view = esurface->view;

	if (shell->exposay.focus_current == view)
		return;

	shell->exposay.row_current = esurface->row;
	shell->exposay.column_current = esurface->column;
	shell->exposay.cur_output = esurface->eoutput;

	activate(shell, view, shell->exposay.seat,
		 WESTON_ACTIVATE_FLAG_NONE);
	shell->exposay.focus_current = view;
}

static int
exposay_is_animating(struct desktop_shell *shell)
{
	if (shell->exposay.state_cur == EXPOSAY_LAYOUT_INACTIVE ||
	    shell->exposay.state_cur == EXPOSAY_LAYOUT_OVERVIEW)
		return 0;

	return (shell->exposay.in_flight > 0);
}

static void
exposay_pick(struct desktop_shell *shell, int x, int y)
{
	struct exposay_surface *esurface;

        if (exposay_is_animating(shell))
            return;

	wl_list_for_each(esurface, &shell->exposay.surface_list, link) {
		if (x < esurface->x || x > esurface->x + esurface->width)
			continue;
		if (y < esurface->y || y > esurface->y + esurface->height)
			continue;

		exposay_highlight_surface(shell, esurface);
		return;
	}
}

static void
handle_view_destroy(struct wl_listener *listener, void *data)
{
	struct exposay_surface *esurface = container_of(listener,
						 struct exposay_surface,
						 view_destroy_listener);

	exposay_surface_destroy(esurface);
}

/* Pretty lame layout for now; just tries to make a square.  Should take
 * aspect ratio into account really.  Also needs to be notified of surface
 * addition and removal and adjust layout/animate accordingly. */
static enum exposay_layout_state
exposay_layout(struct desktop_shell *shell, struct shell_output *shell_output)
{
	struct workspace *workspace = shell->exposay.workspace;
	struct weston_output *output = shell_output->output;
	struct exposay_output *eoutput = &shell_output->eoutput;
	struct weston_view *view;
	struct exposay_surface *esurface, *highlight = NULL;
	int w, h;
	int i;
	int last_row_removed = 0;

	eoutput->num_surfaces = 0;
	wl_list_for_each(view, &workspace->layer.view_list.link, layer_link.link) {
		if (!get_shell_surface(view->surface))
			continue;
		if (view->output != output)
			continue;
		eoutput->num_surfaces++;
	}

	if (eoutput->num_surfaces == 0) {
		eoutput->grid_size = 0;
		eoutput->hpadding_outer = 0;
		eoutput->vpadding_outer = 0;
		eoutput->padding_inner = 0;
		eoutput->surface_size = 0;
		return EXPOSAY_LAYOUT_OVERVIEW;
	}

	/* Lay the grid out as square as possible, losing surfaces from the
	 * bottom row if required.  Start with fixed padding of a 10% margin
	 * around the outside and 80px internal padding between surfaces, and
	 * maximise the area made available to surfaces after this, but only
	 * to a maximum of 1/3rd the total output size.
	 *
	 * If we can't make a square grid, add one extra row at the bottom
	 * which will have a smaller number of columns.
	 *
	 * XXX: Surely there has to be a better way to express this maths,
	 *      right?!
	 */
	eoutput->grid_size = floor(sqrtf(eoutput->num_surfaces));
	if (pow(eoutput->grid_size, 2) != eoutput->num_surfaces)
		eoutput->grid_size++;
	last_row_removed = pow(eoutput->grid_size, 2) - eoutput->num_surfaces;

	eoutput->hpadding_outer = (output->width / 10);
	eoutput->vpadding_outer = (output->height / 10);
	eoutput->padding_inner = 80;

	w = output->width - (eoutput->hpadding_outer * 2);
	w -= eoutput->padding_inner * (eoutput->grid_size - 1);
	w /= eoutput->grid_size;

	h = output->height - (eoutput->vpadding_outer * 2);
	h -= eoutput->padding_inner * (eoutput->grid_size - 1);
	h /= eoutput->grid_size;

	eoutput->surface_size = (w < h) ? w : h;
	if (eoutput->surface_size > (output->width / 2))
		eoutput->surface_size = output->width / 2;
	if (eoutput->surface_size > (output->height / 2))
		eoutput->surface_size = output->height / 2;

	i = 0;
	wl_list_for_each(view, &workspace->layer.view_list.link, layer_link.link) {
		int pad;

		pad = eoutput->surface_size + eoutput->padding_inner;

		if (!get_shell_surface(view->surface))
			continue;
		if (view->output != output)
			continue;

		esurface = malloc(sizeof(*esurface));
		if (!esurface) {
			exposay_set_state(shell, EXPOSAY_TARGET_CANCEL,
			                  shell->exposay.seat);
			break;
		}

		wl_list_insert(&shell->exposay.surface_list, &esurface->link);
		esurface->shell = shell;
		esurface->eoutput = eoutput;
		esurface->view = view;

		esurface->row = i / eoutput->grid_size;
		esurface->column = i % eoutput->grid_size;

		esurface->x = output->x + eoutput->hpadding_outer;
		esurface->x += pad * esurface->column;
		esurface->y = output->y + eoutput->vpadding_outer;
		esurface->y += pad * esurface->row;

		if (esurface->row == eoutput->grid_size - 1)
			esurface->x += (eoutput->surface_size + eoutput->padding_inner) * last_row_removed / 2;

		if (view->surface->width > view->surface->height)
			esurface->scale = eoutput->surface_size / (float) view->surface->width;
		else
			esurface->scale = eoutput->surface_size / (float) view->surface->height;
		esurface->width = view->surface->width * esurface->scale;
		esurface->height = view->surface->height * esurface->scale;

		if (shell->exposay.focus_current == esurface->view)
			highlight = esurface;

		exposay_animate_in(esurface);

		/* We want our destroy handler to be after the animation
		 * destroy handler in the list, this way when the view is
		 * destroyed, the animation can safely call the animation
		 * completion callback before we free the esurface in our
		 * destroy handler.
		 */
		esurface->view_destroy_listener.notify = handle_view_destroy;
		wl_signal_add(&view->destroy_signal, &esurface->view_destroy_listener);

		i++;
	}

	if (highlight) {
		shell->exposay.focus_current = NULL;
		exposay_highlight_surface(shell, highlight);
	}

	weston_compositor_schedule_repaint(shell->compositor);

	return EXPOSAY_LAYOUT_ANIMATE_TO_OVERVIEW;
}

static void
exposay_focus(struct weston_pointer_grab *grab)
{
}

static void
exposay_motion(struct weston_pointer_grab *grab,
	       const struct timespec *time,
	       struct weston_pointer_motion_event *event)
{
	struct desktop_shell *shell =
		container_of(grab, struct desktop_shell, exposay.grab_ptr);

	weston_pointer_move(grab->pointer, event);

	exposay_pick(shell,
	             wl_fixed_to_int(grab->pointer->x),
	             wl_fixed_to_int(grab->pointer->y));
}

static void
exposay_button(struct weston_pointer_grab *grab, const struct timespec *time,
	       uint32_t button, uint32_t state_w)
{
	struct desktop_shell *shell =
		container_of(grab, struct desktop_shell, exposay.grab_ptr);
	struct weston_seat *seat = grab->pointer->seat;
	enum wl_pointer_button_state state = state_w;

	if (button != BTN_LEFT)
		return;

	/* Store the surface we clicked on, and don't do anything if we end up
	 * releasing on a different surface. */
	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		shell->exposay.clicked = shell->exposay.focus_current;
		return;
	}

	if (shell->exposay.focus_current == shell->exposay.clicked)
		exposay_set_state(shell, EXPOSAY_TARGET_SWITCH, seat);
	else
		shell->exposay.clicked = NULL;
}

static void
exposay_axis(struct weston_pointer_grab *grab,
	     const struct timespec *time,
	     struct weston_pointer_axis_event *event)
{
}

static void
exposay_axis_source(struct weston_pointer_grab *grab, uint32_t source)
{
}

static void
exposay_frame(struct weston_pointer_grab *grab)
{
}

static void
exposay_pointer_grab_cancel(struct weston_pointer_grab *grab)
{
	struct desktop_shell *shell =
		container_of(grab, struct desktop_shell, exposay.grab_ptr);

	exposay_set_state(shell, EXPOSAY_TARGET_CANCEL, shell->exposay.seat);
}

static const struct weston_pointer_grab_interface exposay_ptr_grab = {
	exposay_focus,
	exposay_motion,
	exposay_button,
	exposay_axis,
	exposay_axis_source,
	exposay_frame,
	exposay_pointer_grab_cancel,
};

static int
exposay_maybe_move(struct desktop_shell *shell, int row, int column)
{
	struct exposay_surface *esurface;

	wl_list_for_each(esurface, &shell->exposay.surface_list, link) {
		if (esurface->eoutput != shell->exposay.cur_output ||
		    esurface->row != row || esurface->column != column)
			continue;

		exposay_highlight_surface(shell, esurface);
		return 1;
	}

	return 0;
}

static void
exposay_key(struct weston_keyboard_grab *grab, const struct timespec *time,
	    uint32_t key, uint32_t state_w)
{
	struct weston_seat *seat = grab->keyboard->seat;
	struct desktop_shell *shell =
		container_of(grab, struct desktop_shell, exposay.grab_kbd);
	enum wl_keyboard_key_state state = state_w;

	if (state != WL_KEYBOARD_KEY_STATE_RELEASED)
		return;

	switch (key) {
	case KEY_ESC:
		exposay_set_state(shell, EXPOSAY_TARGET_CANCEL, seat);
		break;
	case KEY_ENTER:
		exposay_set_state(shell, EXPOSAY_TARGET_SWITCH, seat);
		break;
	case KEY_UP:
		exposay_maybe_move(shell, shell->exposay.row_current - 1,
		                   shell->exposay.column_current);
		break;
	case KEY_DOWN:
		/* Special case for trying to move to the bottom row when it
		 * has fewer items than all the others. */
		if (!exposay_maybe_move(shell, shell->exposay.row_current + 1,
		                        shell->exposay.column_current) &&
		    shell->exposay.row_current < (shell->exposay.cur_output->grid_size - 1)) {
			exposay_maybe_move(shell, shell->exposay.row_current + 1,
					   (shell->exposay.cur_output->num_surfaces %
					    shell->exposay.cur_output->grid_size) - 1);
		}
		break;
	case KEY_LEFT:
		exposay_maybe_move(shell, shell->exposay.row_current,
		                   shell->exposay.column_current - 1);
		break;
	case KEY_RIGHT:
		exposay_maybe_move(shell, shell->exposay.row_current,
		                   shell->exposay.column_current + 1);
		break;
	case KEY_TAB:
		/* Try to move right, then down (and to the leftmost column),
		 * then if all else fails, to the top left. */
		if (!exposay_maybe_move(shell, shell->exposay.row_current,
					shell->exposay.column_current + 1) &&
		    !exposay_maybe_move(shell, shell->exposay.row_current + 1, 0))
			exposay_maybe_move(shell, 0, 0);
		break;
	default:
		break;
	}
}

static void
exposay_modifier(struct weston_keyboard_grab *grab, uint32_t serial,
                 uint32_t mods_depressed, uint32_t mods_latched,
                 uint32_t mods_locked, uint32_t group)
{
	struct desktop_shell *shell =
		container_of(grab, struct desktop_shell, exposay.grab_kbd);
	struct weston_seat *seat = (struct weston_seat *) grab->keyboard->seat;

	/* We want to know when mod has been pressed and released.
	 * FIXME: There is a problem here: if mod is pressed, then a key
	 * is pressed and released, then mod is released, we will treat that
	 * as if only mod had been pressed and released. */
	if (seat->modifier_state) {
		if (seat->modifier_state == shell->binding_modifier) {
			shell->exposay.mod_pressed = true;
		} else {
			shell->exposay.mod_invalid = true;
		}
	} else {
		if (shell->exposay.mod_pressed && !shell->exposay.mod_invalid)
			exposay_set_state(shell, EXPOSAY_TARGET_CANCEL, seat);

		shell->exposay.mod_invalid = false;
		shell->exposay.mod_pressed = false;
	}

	return;
}

static void
exposay_cancel(struct weston_keyboard_grab *grab)
{
	struct desktop_shell *shell =
		container_of(grab, struct desktop_shell, exposay.grab_kbd);

	exposay_set_state(shell, EXPOSAY_TARGET_CANCEL, shell->exposay.seat);
}

static const struct weston_keyboard_grab_interface exposay_kbd_grab = {
	exposay_key,
	exposay_modifier,
	exposay_cancel,
};

/**
 * Called when the transition from overview -> inactive has completed.
 */
static enum exposay_layout_state
exposay_set_inactive(struct desktop_shell *shell)
{
	struct weston_seat *seat = shell->exposay.seat;
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	if (pointer)
		weston_pointer_end_grab(pointer);

	if (keyboard) {
		weston_keyboard_end_grab(keyboard);
		if (keyboard->input_method_resource)
			keyboard->grab = &keyboard->input_method_grab;
	}

	return EXPOSAY_LAYOUT_INACTIVE;
}

/**
 * Begins the transition from overview to inactive. */
static enum exposay_layout_state
exposay_transition_inactive(struct desktop_shell *shell, int switch_focus)
{
	struct exposay_surface *esurface;

	/* Call activate() before we start the animations to avoid
	 * animating back the old state and then immediately transitioning
	 * to the new. */
	if (switch_focus && shell->exposay.focus_current)
		activate(shell, shell->exposay.focus_current,
		         shell->exposay.seat,
			 WESTON_ACTIVATE_FLAG_CONFIGURE);
	else if (shell->exposay.focus_prev)
		activate(shell, shell->exposay.focus_prev,
		         shell->exposay.seat,
			 WESTON_ACTIVATE_FLAG_CONFIGURE);

	wl_list_for_each(esurface, &shell->exposay.surface_list, link)
		exposay_animate_out(esurface);
	weston_compositor_schedule_repaint(shell->compositor);

	return EXPOSAY_LAYOUT_ANIMATE_TO_INACTIVE;
}

static enum exposay_layout_state
exposay_transition_active(struct desktop_shell *shell)
{
	struct weston_seat *seat = shell->exposay.seat;
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	struct shell_output *shell_output;
	bool animate = false;

	shell->exposay.workspace = get_current_workspace(shell);
	shell->exposay.focus_prev = get_default_view(keyboard->focus);
	shell->exposay.focus_current = get_default_view(keyboard->focus);
	shell->exposay.clicked = NULL;
	wl_list_init(&shell->exposay.surface_list);

	lower_fullscreen_layer(shell, NULL);
	shell->exposay.grab_kbd.interface = &exposay_kbd_grab;
	weston_keyboard_start_grab(keyboard,
	                           &shell->exposay.grab_kbd);
	weston_keyboard_set_focus(keyboard, NULL);

	shell->exposay.grab_ptr.interface = &exposay_ptr_grab;
	if (pointer) {
		weston_pointer_start_grab(pointer,
		                          &shell->exposay.grab_ptr);
		weston_pointer_clear_focus(pointer);
	}
	wl_list_for_each(shell_output, &shell->output_list, link) {
		enum exposay_layout_state state;

		state = exposay_layout(shell, shell_output);

		if (state == EXPOSAY_LAYOUT_ANIMATE_TO_OVERVIEW)
			animate = true;
	}

	return animate ? EXPOSAY_LAYOUT_ANIMATE_TO_OVERVIEW
		       : EXPOSAY_LAYOUT_OVERVIEW;
}

static void
exposay_check_state(struct desktop_shell *shell)
{
	enum exposay_layout_state state_new = shell->exposay.state_cur;
	int do_switch = 0;

	/* Don't do anything whilst animations are running, just store up
	 * target state changes and only act on them when the animations have
	 * completed. */
	if (exposay_is_animating(shell))
		return;

	switch (shell->exposay.state_target) {
	case EXPOSAY_TARGET_OVERVIEW:
		switch (shell->exposay.state_cur) {
		case EXPOSAY_LAYOUT_OVERVIEW:
			goto out;
		case EXPOSAY_LAYOUT_ANIMATE_TO_OVERVIEW:
			state_new = EXPOSAY_LAYOUT_OVERVIEW;
			break;
		default:
			state_new = exposay_transition_active(shell);
			break;
		}
		break;

	case EXPOSAY_TARGET_SWITCH:
		do_switch = 1; /* fallthrough */
	case EXPOSAY_TARGET_CANCEL:
		switch (shell->exposay.state_cur) {
		case EXPOSAY_LAYOUT_INACTIVE:
			goto out;
		case EXPOSAY_LAYOUT_ANIMATE_TO_INACTIVE:
			state_new = exposay_set_inactive(shell);
			break;
		default:
			state_new = exposay_transition_inactive(shell, do_switch);
			break;
		}

		break;
	}

out:
	shell->exposay.state_cur = state_new;
}

static void
exposay_set_state(struct desktop_shell *shell, enum exposay_target_state state,
                  struct weston_seat *seat)
{
	shell->exposay.state_target = state;
	shell->exposay.seat = seat;
	exposay_check_state(shell);
}

void
exposay_binding(struct weston_keyboard *keyboard, enum weston_keyboard_modifier modifier,
		void *data)
{
	struct desktop_shell *shell = data;

	exposay_set_state(shell, EXPOSAY_TARGET_OVERVIEW, keyboard->seat);
}
