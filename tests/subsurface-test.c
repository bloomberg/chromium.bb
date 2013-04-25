/*
 * Copyright © 2012 Collabora, Ltd.
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

#include <string.h>

#include "weston-test-client-helper.h"
#include "subsurface-client-protocol.h"

#define NUM_SUBSURFACES 3

struct compound_surface {
	struct wl_subcompositor *subco;
	struct wl_surface *parent;
	struct wl_surface *child[NUM_SUBSURFACES];
	struct wl_subsurface *sub[NUM_SUBSURFACES];
};

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

static void
populate_compound_surface(struct compound_surface *com, struct client *client)
{
	int i;

	com->subco = get_subcompositor(client);

	com->parent = wl_compositor_create_surface(client->wl_compositor);

	for (i = 0; i < NUM_SUBSURFACES; i++) {
		com->child[i] =
			wl_compositor_create_surface(client->wl_compositor);
		com->sub[i] =
			wl_subcompositor_get_subsurface(com->subco,
							com->child[i],
							com->parent);
	}
}

TEST(test_subsurface_basic_protocol)
{
	struct client *client;
	struct compound_surface com1;
	struct compound_surface com2;

	client = client_create(100, 50, 123, 77);
	assert(client);

	populate_compound_surface(&com1, client);
	populate_compound_surface(&com2, client);

	client_roundtrip(client);
}

TEST(test_subsurface_position_protocol)
{
	struct client *client;
	struct compound_surface com;
	int i;

	client = client_create(100, 50, 123, 77);
	assert(client);

	populate_compound_surface(&com, client);
	for (i = 0; i < NUM_SUBSURFACES; i++)
		wl_subsurface_set_position(com.sub[i],
					   (i + 2) * 20, (i + 2) * 10);

	client_roundtrip(client);
}

TEST(test_subsurface_placement_protocol)
{
	struct client *client;
	struct compound_surface com;

	client = client_create(100, 50, 123, 77);
	assert(client);

	populate_compound_surface(&com, client);

	wl_subsurface_place_above(com.sub[0], com.child[1]);
	wl_subsurface_place_above(com.sub[1], com.parent);
	wl_subsurface_place_below(com.sub[2], com.child[0]);
	wl_subsurface_place_below(com.sub[1], com.parent);

	client_roundtrip(client);
}

FAIL_TEST(test_subsurface_paradox)
{
	struct client *client;
	struct wl_surface *parent;
	struct wl_subcompositor *subco;

	client = client_create(100, 50, 123, 77);
	assert(client);

	subco = get_subcompositor(client);
	parent = wl_compositor_create_surface(client->wl_compositor);

	/* surface is its own parent */
	wl_subcompositor_get_subsurface(subco, parent, parent);

	client_roundtrip(client);
}

FAIL_TEST(test_subsurface_identical_link)
{
	struct client *client;
	struct compound_surface com;

	client = client_create(100, 50, 123, 77);
	assert(client);

	populate_compound_surface(&com, client);

	/* surface is already a subsurface */
	wl_subcompositor_get_subsurface(com.subco, com.child[0], com.parent);

	client_roundtrip(client);
}

FAIL_TEST(test_subsurface_change_link)
{
	struct client *client;
	struct compound_surface com;
	struct wl_surface *stranger;

	client = client_create(100, 50, 123, 77);
	assert(client);

	stranger = wl_compositor_create_surface(client->wl_compositor);
	populate_compound_surface(&com, client);

	/* surface is already a subsurface */
	wl_subcompositor_get_subsurface(com.subco, com.child[0], stranger);

	client_roundtrip(client);
}

TEST(test_subsurface_nesting)
{
	struct client *client;
	struct compound_surface com;
	struct wl_surface *stranger;

	client = client_create(100, 50, 123, 77);
	assert(client);

	stranger = wl_compositor_create_surface(client->wl_compositor);
	populate_compound_surface(&com, client);

	/* parent is a sub-surface */
	wl_subcompositor_get_subsurface(com.subco, stranger, com.child[0]);

	client_roundtrip(client);
}

TEST(test_subsurface_nesting_parent)
{
	struct client *client;
	struct compound_surface com;
	struct wl_surface *stranger;

	client = client_create(100, 50, 123, 77);
	assert(client);

	stranger = wl_compositor_create_surface(client->wl_compositor);
	populate_compound_surface(&com, client);

	/* surface is already a parent */
	wl_subcompositor_get_subsurface(com.subco, com.parent, stranger);

	client_roundtrip(client);
}

FAIL_TEST(test_subsurface_place_above_stranger)
{
	struct client *client;
	struct compound_surface com;
	struct wl_surface *stranger;

	client = client_create(100, 50, 123, 77);
	assert(client);

	stranger = wl_compositor_create_surface(client->wl_compositor);
	populate_compound_surface(&com, client);

	/* bad sibling */
	wl_subsurface_place_above(com.sub[0], stranger);

	client_roundtrip(client);
}

FAIL_TEST(test_subsurface_place_below_stranger)
{
	struct client *client;
	struct compound_surface com;
	struct wl_surface *stranger;

	client = client_create(100, 50, 123, 77);
	assert(client);

	stranger = wl_compositor_create_surface(client->wl_compositor);
	populate_compound_surface(&com, client);

	/* bad sibling */
	wl_subsurface_place_below(com.sub[0], stranger);

	client_roundtrip(client);
}

FAIL_TEST(test_subsurface_place_above_foreign)
{
	struct client *client;
	struct compound_surface com1;
	struct compound_surface com2;

	client = client_create(100, 50, 123, 77);
	assert(client);

	populate_compound_surface(&com1, client);
	populate_compound_surface(&com2, client);

	/* bad sibling */
	wl_subsurface_place_above(com1.sub[0], com2.child[0]);

	client_roundtrip(client);
}

FAIL_TEST(test_subsurface_place_below_foreign)
{
	struct client *client;
	struct compound_surface com1;
	struct compound_surface com2;

	client = client_create(100, 50, 123, 77);
	assert(client);

	populate_compound_surface(&com1, client);
	populate_compound_surface(&com2, client);

	/* bad sibling */
	wl_subsurface_place_below(com1.sub[0], com2.child[0]);

	client_roundtrip(client);
}

TEST(test_subsurface_destroy_protocol)
{
	struct client *client;
	struct compound_surface com;

	client = client_create(100, 50, 123, 77);
	assert(client);

	populate_compound_surface(&com, client);

	/* not needed anymore */
	wl_subcompositor_destroy(com.subco);

	/* detach child from parent */
	wl_subsurface_destroy(com.sub[0]);

	/* destroy: child, parent */
	wl_surface_destroy(com.child[1]);
	wl_surface_destroy(com.parent);

	/* destroy: parent, child */
	wl_surface_destroy(com.child[2]);

	/* destroy: sub, child */
	wl_surface_destroy(com.child[0]);

	/* 2x destroy: child, sub */
	wl_subsurface_destroy(com.sub[2]);
	wl_subsurface_destroy(com.sub[1]);

	client_roundtrip(client);
}
