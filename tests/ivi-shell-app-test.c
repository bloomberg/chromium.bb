/*
 * Copyright © 2015 Collabora, Ltd.
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
