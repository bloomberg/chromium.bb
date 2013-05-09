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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "compositor.h"
#include "cms-helper.h"

struct cms_static {
	struct weston_compositor	*ec;
	struct wl_listener		 destroy_listener;
	struct wl_listener		 output_created_listener;
	struct wl_list			 configured_output_list;
};

struct cms_configured_output {
	char				*icc_profile;
	char				*name;
	struct wl_list			 link;
};

static void
cms_output_created(struct cms_static *cms, struct weston_output *o)
{
	struct cms_configured_output *configured_output;
	struct weston_color_profile *p;

	weston_log("cms-static: output %i [%s] created\n", o->id, o->name);

	/* find profile from configured list */
	wl_list_for_each(configured_output, &cms->configured_output_list, link) {
		if (strcmp (o->name, configured_output->name) == 0) {
			p = weston_cms_load_profile(configured_output->icc_profile);
			if (p == NULL) {
				weston_log("cms-static: failed to load %s\n",
					   configured_output->icc_profile);
			} else {
				       weston_log("cms-static: loading %s for %s\n",
						  configured_output->icc_profile, o->name);
				       weston_cms_set_color_profile(o, p);
			}
			break;
		}
	}
}

static void
cms_notifier_output_created(struct wl_listener *listener, void *data)
{
	struct weston_output *o = (struct weston_output *) data;
	struct cms_static *cms = container_of(listener, struct cms_static, destroy_listener);
	cms_output_created(cms, o);
}

static void
cms_module_destroy(struct cms_static *cms)
{
	struct cms_configured_output *configured_output, *next_co;

	wl_list_for_each_safe(configured_output, next_co,
			      &cms->configured_output_list, link) {
		free(configured_output->name);
		free(configured_output->icc_profile);
		free(configured_output);
	}
	free(cms);
}

static void
cms_notifier_destroy(struct wl_listener *listener, void *data)
{
	struct cms_static *cms = container_of(listener, struct cms_static, destroy_listener);
	cms_module_destroy(cms);
}

static char *output_icc_profile;
static char *output_name;

static void
output_section_done(void *data)
{
	struct cms_configured_output *configured_output;
	struct cms_static *cms = (struct cms_static *) data;

	if (output_name == NULL || output_icc_profile == NULL) {
		free(output_name);
		free(output_icc_profile);
		output_name = NULL;
		output_icc_profile = NULL;
		return;
	}

	weston_log("cms-static: output %s profile configured as %s\n",
		   output_name, output_icc_profile);

	/* create an object used to store name<->profile data to avoid parsing
	 * the config file every time a new output is added */
	configured_output = malloc(sizeof *configured_output);
	memset(configured_output, 0, sizeof *configured_output);
	configured_output->name = output_name;
	configured_output->icc_profile = output_icc_profile;
	wl_list_insert(&cms->configured_output_list, &configured_output->link);
	output_name = NULL;
	output_icc_profile = NULL;
}

WL_EXPORT int
module_init(struct weston_compositor *ec,
	    int *argc, char *argv[], const char *config_file)
{
	struct cms_static *cms;
	struct weston_output *output;

	weston_log("cms-static: initialized\n");

	/* create local state object */
	cms = malloc(sizeof *cms);
	if (cms == NULL)
		return -1;
	memset(cms, 0, sizeof *cms);

	wl_list_init(&cms->configured_output_list);

	/* parse config file */
	const struct config_key drm_config_keys[] = {
		{ "name", CONFIG_KEY_STRING, &output_name },
		{ "icc_profile", CONFIG_KEY_STRING, &output_icc_profile },
	};

	const struct config_section config_section[] = {
		{ "output", drm_config_keys,
		ARRAY_LENGTH(drm_config_keys), output_section_done },
	};

	parse_config_file(config_file, config_section,
			  ARRAY_LENGTH(config_section), cms);

	cms->destroy_listener.notify = cms_notifier_destroy;
	wl_signal_add(&ec->destroy_signal, &cms->destroy_listener);

	cms->output_created_listener.notify = cms_notifier_output_created;
	wl_signal_add(&ec->output_created_signal, &cms->output_created_listener);

	/* discover outputs */
	wl_list_for_each(output, &ec->output_list, link)
		cms_output_created(cms, output);

	return 0;
}
