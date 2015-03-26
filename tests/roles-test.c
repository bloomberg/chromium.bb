/*
 * Copyright © 2014 Collabora, Ltd.
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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "weston-test-client-helper.h"

static struct wl_subcompositor *
get_subcompositor(struct client *client)
{
	struct global *g;
	struct global *global_sub = NULL;
	struct wl_subcompositor *sub;

	wl_list_for_each(g, &client->global_list, link) {
		if (strcmp(g->interface, "wl_subcompositor"))
			continue;

		if (global_sub)
			assert(0 && "multiple wl_subcompositor objects");

		global_sub = g;
	}

	assert(global_sub && "no wl_subcompositor found");

	assert(global_sub->version == 1);

	sub = wl_registry_bind(client->wl_registry, global_sub->name,
			       &wl_subcompositor_interface, 1);
	assert(sub);

	return sub;
}

static struct wl_shell *
get_wl_shell(struct client *client)
{
	struct global *g;
	struct global *global = NULL;
	struct wl_shell *shell;

	wl_list_for_each(g, &client->global_list, link) {
		if (strcmp(g->interface, "wl_shell"))
			continue;

		if (global)
			assert(0 && "multiple wl_shell objects");

		global = g;
	}

	assert(global && "no wl_shell found");

	shell = wl_registry_bind(client->wl_registry, global->name,
				 &wl_shell_interface, 1);
	assert(shell);

	return shell;
}

TEST(test_role_conflict_sub_wlshell)
{
	struct client *client;
	struct wl_subcompositor *subco;
	struct wl_surface *child;
	struct wl_subsurface *sub;
	struct wl_shell *shell;
	struct wl_shell_surface *shsurf;

	client = create_client_and_test_surface(100, 50, 123, 77);
	assert(client);

	subco = get_subcompositor(client);
	shell = get_wl_shell(client);

	child = wl_compositor_create_surface(client->wl_compositor);
	assert(child);
	sub = wl_subcompositor_get_subsurface(subco, child,
					      client->surface->wl_surface);
	assert(sub);

	shsurf = wl_shell_get_shell_surface(shell, child);
	assert(shsurf);

	expect_protocol_error(client, &wl_shell_interface,
			      WL_SHELL_ERROR_ROLE);
}

TEST(test_role_conflict_wlshell_sub)
{
	struct client *client;
	struct wl_subcompositor *subco;
	struct wl_surface *child;
	struct wl_subsurface *sub;
	struct wl_shell *shell;
	struct wl_shell_surface *shsurf;

	client = create_client_and_test_surface(100, 50, 123, 77);
	assert(client);

	subco = get_subcompositor(client);
	shell = get_wl_shell(client);

	child = wl_compositor_create_surface(client->wl_compositor);
	assert(child);
	shsurf = wl_shell_get_shell_surface(shell, child);
	assert(shsurf);

	sub = wl_subcompositor_get_subsurface(subco, child,
					      client->surface->wl_surface);
	assert(sub);

	expect_protocol_error(client, &wl_subcompositor_interface,
			      WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE);
}
