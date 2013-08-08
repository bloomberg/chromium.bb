/*
 * Copyright © 2013 Richard Hughes
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
#include <string.h>

#include "compositor.h"
#include "cms-helper.h"

struct cms_static {
	struct weston_compositor	*ec;
	struct wl_listener		 destroy_listener;
	struct wl_listener		 output_created_listener;
};

static void
cms_output_created(struct cms_static *cms, struct weston_output *o)
{
	struct weston_color_profile *p;
	struct weston_config_section *s;
	char *profile;

	weston_log("cms-static: output %i [%s] created\n", o->id, o->name);

	if (o->name == NULL)
		return;
	s = weston_config_get_section(cms->ec->config,
				      "output", "name", o->name);
	if (s == NULL)
		return;
	if (weston_config_section_get_string(s, "icc_profile", &profile, NULL) < 0)
		return;
	p = weston_cms_load_profile(profile);
	if (p == NULL) {
		weston_log("cms-static: failed to load %s\n", profile);
	} else {
		weston_log("cms-static: loading %s for %s\n",
			   profile, o->name);
		weston_cms_set_color_profile(o, p);
	}
}

static void
cms_notifier_output_created(struct wl_listener *listener, void *data)
{
	struct weston_output *o = (struct weston_output *) data;
	struct cms_static *cms =
		container_of(listener, struct cms_static, output_created_listener);
	cms_output_created(cms, o);
}

static void
cms_module_destroy(struct cms_static *cms)
{
	free(cms);
}

static void
cms_notifier_destroy(struct wl_listener *listener, void *data)
{
	struct cms_static *cms = container_of(listener, struct cms_static, destroy_listener);
	cms_module_destroy(cms);
}


WL_EXPORT int
module_init(struct weston_compositor *ec,
	    int *argc, char *argv[])
{
	struct cms_static *cms;
	struct weston_output *output;

	weston_log("cms-static: initialized\n");

	/* create local state object */
	cms = zalloc(sizeof *cms);
	if (cms == NULL)
		return -1;

	cms->ec = ec;
	cms->destroy_listener.notify = cms_notifier_destroy;
	wl_signal_add(&ec->destroy_signal, &cms->destroy_listener);

	cms->output_created_listener.notify = cms_notifier_output_created;
	wl_signal_add(&ec->output_created_signal, &cms->output_created_listener);

	/* discover outputs */
	wl_list_for_each(output, &ec->output_list, link)
		cms_output_created(cms, output);

	return 0;
}
