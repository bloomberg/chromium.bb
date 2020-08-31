/*
 * Copyright © 2015 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "weston-test-client-helper.h"
#include "ivi-application-client-protocol.h"

static struct ivi_application *
get_ivi_application(struct client *client)
{
	struct global *g;
	struct global *global_iviapp = NULL;
	static struct ivi_application *iviapp;

	if (iviapp)
		return iviapp;

	wl_list_for_each(g, &client->global_list, link) {
		if (strcmp(g->interface, "ivi_application"))
			continue;

		if (global_iviapp)
			assert(0 && "multiple ivi_application objects");

		global_iviapp = g;
	}

	assert(global_iviapp && "no ivi_application found");

	assert(global_iviapp->version == 1);

	iviapp = wl_registry_bind(client->wl_registry, global_iviapp->name,
				  &ivi_application_interface, 1);
	assert(iviapp);

	return iviapp;
}

TEST(ivi_application_exists)
{
	struct client *client;
	struct ivi_application *iviapp;

	client = create_client();
	iviapp = get_ivi_application(client);
	client_roundtrip(client);

	printf("Successful bind: %p\n", iviapp);
}
