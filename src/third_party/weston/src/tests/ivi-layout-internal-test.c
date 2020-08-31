/*
 * Copyright © 2013 DENSO CORPORATION
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

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <libweston/libweston.h>
#include "compositor/weston.h"
#include "ivi-shell/ivi-layout-export.h"
#include "ivi-shell/ivi-layout-private.h"
#include "ivi-test.h"
#include "shared/helpers.h"

struct test_context {
	struct weston_compositor *compositor;
	const struct ivi_layout_interface *layout_interface;
	uint32_t user_flags;

	struct wl_listener layer_property_changed;
	struct wl_listener layer_created;
	struct wl_listener layer_removed;
};

static void
iassert_fail(const char *cond, const char *file, int line,
	     const char *func, struct test_context *ctx)
{
	weston_log("Assert failure in %s:%d, %s: '%s'\n",
		   file, line, func, cond);
	weston_compositor_exit_with_code(ctx->compositor, EXIT_FAILURE);
}

#define iassert(cond) ({						\
	bool b_ = (cond);						\
	if (!b_)							\
		iassert_fail(#cond, __FILE__, __LINE__, __func__, ctx);	\
	b_;								\
})

/************************ tests begin ******************************/

/*
 * These are all internal ivi_layout API tests that do not require
 * any client objects.
 */
static void
test_surface_bad_visibility(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->surface_set_visibility(NULL, true) == IVI_FAILED);

	lyt->commit_changes();
}

static void
test_surface_bad_destination_rectangle(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->surface_set_destination_rectangle(NULL, 20, 30, 200, 300) == IVI_FAILED);
}

static void
test_surface_bad_source_rectangle(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->surface_set_source_rectangle(NULL, 20, 30, 200, 300) == IVI_FAILED);
}

static void
test_surface_bad_properties(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->get_properties_of_surface(NULL) == NULL);
}

static void
test_layer_create(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	uint32_t id1;
	uint32_t id2;
	struct ivi_layout_layer *ivilayer;
	struct ivi_layout_layer *new_ivilayer;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(IVI_TEST_LAYER_ID(0) == lyt->get_id_of_layer(ivilayer));

	new_ivilayer = lyt->get_layer_from_id(IVI_TEST_LAYER_ID(0));
	iassert(ivilayer == new_ivilayer);

	id1 = lyt->get_id_of_layer(ivilayer);
	id2 = lyt->get_id_of_layer(new_ivilayer);
	iassert(id1 == id2);

	lyt->layer_destroy(ivilayer);
	iassert(lyt->get_layer_from_id(IVI_TEST_LAYER_ID(0)) == NULL);
}

static void
test_layer_visibility(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	prop = lyt->get_properties_of_layer(ivilayer);

	iassert(prop->visibility == false);

	iassert(lyt->layer_set_visibility(ivilayer, true) == IVI_SUCCEEDED);

	iassert(prop->visibility == false);

	lyt->commit_changes();

	iassert(prop->visibility == true);

	lyt->layer_destroy(ivilayer);
}

static void
test_layer_opacity(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->opacity == wl_fixed_from_double(1.0));

	iassert(lyt->layer_set_opacity(
		ivilayer, wl_fixed_from_double(0.5)) == IVI_SUCCEEDED);

	iassert(prop->opacity == wl_fixed_from_double(1.0));

	lyt->commit_changes();

	iassert(prop->opacity == wl_fixed_from_double(0.5));

	lyt->layer_destroy(ivilayer);
}

static void
test_layer_dimension(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->dest_width == 200);
	iassert(prop->dest_height == 300);

	iassert(lyt->layer_set_destination_rectangle(ivilayer, prop->dest_x, prop->dest_y,
			400, 600) == IVI_SUCCEEDED);

	iassert(prop->dest_width == 200);
	iassert(prop->dest_height == 300);

	lyt->commit_changes();

	iassert(prop->dest_width == 400);
	iassert(prop->dest_height == 600);

	lyt->layer_destroy(ivilayer);
}

static void
test_layer_position(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->dest_x == 0);
	iassert(prop->dest_y == 0);

	iassert(lyt->layer_set_destination_rectangle(ivilayer, 20, 30,
			prop->dest_width, prop->dest_height) == IVI_SUCCEEDED);

	iassert(prop->dest_x == 0);
	iassert(prop->dest_y == 0);

	lyt->commit_changes();

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->dest_x == 20);
	iassert(prop->dest_y == 30);

	lyt->layer_destroy(ivilayer);
}

static void
test_layer_destination_rectangle(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->dest_width == 200);
	iassert(prop->dest_height == 300);
	iassert(prop->dest_x == 0);
	iassert(prop->dest_y == 0);

	iassert(lyt->layer_set_destination_rectangle(
		ivilayer, 20, 30, 400, 600) == IVI_SUCCEEDED);

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->dest_width == 200);
	iassert(prop->dest_height == 300);
	iassert(prop->dest_x == 0);
	iassert(prop->dest_y == 0);

	lyt->commit_changes();

	iassert(prop->dest_width == 400);
	iassert(prop->dest_height == 600);
	iassert(prop->dest_x == 20);
	iassert(prop->dest_y == 30);

	lyt->layer_destroy(ivilayer);
}

static void
test_layer_source_rectangle(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->source_width == 200);
	iassert(prop->source_height == 300);
	iassert(prop->source_x == 0);
	iassert(prop->source_y == 0);

	iassert(lyt->layer_set_source_rectangle(
		ivilayer, 20, 30, 400, 600) == IVI_SUCCEEDED);

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->source_width == 200);
	iassert(prop->source_height == 300);
	iassert(prop->source_x == 0);
	iassert(prop->source_y == 0);

	lyt->commit_changes();

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->source_width == 400);
	iassert(prop->source_height == 600);
	iassert(prop->source_x == 20);
	iassert(prop->source_y == 30);

	lyt->layer_destroy(ivilayer);
}

static void
test_layer_bad_remove(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	lyt->layer_destroy(NULL);
}

static void
test_layer_bad_visibility(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->layer_set_visibility(NULL, true) == IVI_FAILED);

	lyt->commit_changes();
}

static void
test_layer_bad_opacity(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(lyt->layer_set_opacity(
		NULL, wl_fixed_from_double(0.3)) == IVI_FAILED);

	iassert(lyt->layer_set_opacity(
		ivilayer, wl_fixed_from_double(0.3)) == IVI_SUCCEEDED);

	iassert(lyt->layer_set_opacity(
		ivilayer, wl_fixed_from_double(-1)) == IVI_FAILED);

	lyt->commit_changes();

	prop = lyt->get_properties_of_layer(ivilayer);
	iassert(prop->opacity == wl_fixed_from_double(0.3));

	iassert(lyt->layer_set_opacity(
		ivilayer, wl_fixed_from_double(1.1)) == IVI_FAILED);

	lyt->commit_changes();

	iassert(prop->opacity == wl_fixed_from_double(0.3));

	iassert(lyt->layer_set_opacity(
		NULL, wl_fixed_from_double(0.5)) == IVI_FAILED);

	lyt->commit_changes();

	lyt->layer_destroy(ivilayer);
}

static void
test_layer_bad_destination_rectangle(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->layer_set_destination_rectangle(
		NULL, 20, 30, 200, 300) == IVI_FAILED);
}

static void
test_layer_bad_source_rectangle(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->layer_set_source_rectangle(
		NULL, 20, 30, 200, 300) == IVI_FAILED);
}

static void
test_layer_bad_properties(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->get_properties_of_layer(NULL) == NULL);
}

static void
test_commit_changes_after_visibility_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(lyt->layer_set_visibility(ivilayer, true) == IVI_SUCCEEDED);
	lyt->layer_destroy(ivilayer);
	lyt->commit_changes();
}

static void
test_commit_changes_after_opacity_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(lyt->layer_set_opacity(
		    ivilayer, wl_fixed_from_double(0.5)) == IVI_SUCCEEDED);
	lyt->layer_destroy(ivilayer);
	lyt->commit_changes();
}

static void
test_commit_changes_after_source_rectangle_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(lyt->layer_set_source_rectangle(
		    ivilayer, 20, 30, 200, 300) == IVI_SUCCEEDED);
	lyt->layer_destroy(ivilayer);
	lyt->commit_changes();
}

static void
test_commit_changes_after_destination_rectangle_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(lyt->layer_set_destination_rectangle(
		    ivilayer, 20, 30, 200, 300) == IVI_SUCCEEDED);
	lyt->layer_destroy(ivilayer);
	lyt->commit_changes();
}

static void
test_layer_create_duplicate(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	struct ivi_layout_layer *duplicatelayer;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	if (ivilayer != NULL)
		iassert(ivilayer->ref_count == 1);

	duplicatelayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer == duplicatelayer);

	if (ivilayer != NULL)
		iassert(ivilayer->ref_count == 2);

	lyt->layer_destroy(ivilayer);

	if (ivilayer != NULL)
		iassert(ivilayer->ref_count == 1);

	lyt->layer_destroy(ivilayer);
}

static void
test_get_layer_after_destory_layer(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	lyt->layer_destroy(ivilayer);

	ivilayer = lyt->get_layer_from_id(IVI_TEST_LAYER_ID(0));
	iassert(ivilayer == NULL);
}

static void
test_screen_render_order(struct test_context *ctx)
{
#define LAYER_NUM (3)
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct weston_output *output;
	struct ivi_layout_layer *ivilayers[LAYER_NUM] = {};
	struct ivi_layout_layer **array;
	int32_t length = 0;
	uint32_t i;

	if (!iassert(!wl_list_empty(&ctx->compositor->output_list)))
		return;

	output = wl_container_of(ctx->compositor->output_list.next, output, link);

	for (i = 0; i < LAYER_NUM; i++)
		ivilayers[i] = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(i), 200, 300);

	iassert(lyt->screen_set_render_order(output, ivilayers, LAYER_NUM) == IVI_SUCCEEDED);

	lyt->commit_changes();

	iassert(lyt->get_layers_on_screen(output, &length, &array) == IVI_SUCCEEDED);
	iassert(length == LAYER_NUM);
	for (i = 0; i < LAYER_NUM; i++)
		iassert(array[i] == ivilayers[i]);

	if (length > 0)
		free(array);

	array = NULL;

	iassert(lyt->screen_set_render_order(output, NULL, 0) == IVI_SUCCEEDED);

	lyt->commit_changes();

	iassert(lyt->get_layers_on_screen(output, &length, &array) == IVI_SUCCEEDED);
	iassert(length == 0 && array == NULL);

	for (i = 0; i < LAYER_NUM; i++)
		lyt->layer_destroy(ivilayers[i]);

#undef LAYER_NUM
}

static void
test_screen_bad_render_order(struct test_context *ctx)
{
#define LAYER_NUM (3)
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct weston_output *output;
	struct ivi_layout_layer *ivilayers[LAYER_NUM] = {};
	struct ivi_layout_layer **array;
	int32_t length = 0;
	uint32_t i;

	if (!iassert(!wl_list_empty(&ctx->compositor->output_list)))
		return;

	output = wl_container_of(ctx->compositor->output_list.next, output, link);

	for (i = 0; i < LAYER_NUM; i++)
		ivilayers[i] = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(i), 200, 300);

	iassert(lyt->screen_set_render_order(NULL, ivilayers, LAYER_NUM) == IVI_FAILED);

	lyt->commit_changes();

	iassert(lyt->get_layers_on_screen(NULL, &length, &array) == IVI_FAILED);
	iassert(lyt->get_layers_on_screen(output, NULL, &array) == IVI_FAILED);
	iassert(lyt->get_layers_on_screen(output, &length, NULL) == IVI_FAILED);

	for (i = 0; i < LAYER_NUM; i++)
		lyt->layer_destroy(ivilayers[i]);

#undef LAYER_NUM
}

static void
test_screen_add_layers(struct test_context *ctx)
{
#define LAYER_NUM (3)
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct weston_output *output;
	struct ivi_layout_layer *ivilayers[LAYER_NUM] = {};
	struct ivi_layout_layer **array;
	int32_t length = 0;
	uint32_t i;

	if (!iassert(!wl_list_empty(&ctx->compositor->output_list)))
		return;

	output = wl_container_of(ctx->compositor->output_list.next, output, link);

	for (i = 0; i < LAYER_NUM; i++) {
		ivilayers[i] = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(i), 200, 300);
		iassert(lyt->screen_add_layer(output, ivilayers[i]) == IVI_SUCCEEDED);
	}

	lyt->commit_changes();

	iassert(lyt->get_layers_on_screen(output, &length, &array) == IVI_SUCCEEDED);
	iassert(length == LAYER_NUM);
	for (i = 0; i < (uint32_t)length; i++)
		iassert(array[i] == ivilayers[i]);

	if (length > 0)
		free(array);

	array = NULL;

	iassert(lyt->screen_set_render_order(output, NULL, 0) == IVI_SUCCEEDED);
	for (i = LAYER_NUM; i-- > 0;)
		iassert(lyt->screen_add_layer(output, ivilayers[i]) == IVI_SUCCEEDED);

	lyt->commit_changes();

	iassert(lyt->get_layers_on_screen(output, &length, &array) == IVI_SUCCEEDED);
	iassert(length == LAYER_NUM);
	for (i = 0; i < (uint32_t)length; i++)
		iassert(array[i] == ivilayers[LAYER_NUM - (i + 1)]);

	if (length > 0)
		free(array);

	for (i = 0; i < LAYER_NUM; i++)
		lyt->layer_destroy(ivilayers[i]);

#undef LAYER_NUM
}

static void
test_screen_remove_layer(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	struct weston_output *output;
	struct ivi_layout_layer **array;
	int32_t length = 0;

	if (wl_list_empty(&ctx->compositor->output_list))
		return;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	output = wl_container_of(ctx->compositor->output_list.next, output, link);

	iassert(lyt->screen_add_layer(output, ivilayer) == IVI_SUCCEEDED);
	lyt->commit_changes();

	iassert(lyt->get_layers_on_screen(output, &length, &array) == IVI_SUCCEEDED);
	iassert(length == 1);
	iassert(array[0] == ivilayer);

	iassert(lyt->screen_remove_layer(output, ivilayer) == IVI_SUCCEEDED);
	lyt->commit_changes();

	if (length > 0)
		free(array);

	array = NULL;

	iassert(lyt->get_layers_on_screen(output, &length, &array) == IVI_SUCCEEDED);
	iassert(length == 0);
	iassert(array == NULL);

	lyt->layer_destroy(ivilayer);
}

static void
test_screen_bad_remove_layer(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;
	struct weston_output *output;

	if (wl_list_empty(&ctx->compositor->output_list))
		return;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	output = wl_container_of(ctx->compositor->output_list.next, output, link);

	iassert(lyt->screen_remove_layer(NULL, ivilayer) == IVI_FAILED);
	lyt->commit_changes();

	iassert(lyt->screen_remove_layer(output, NULL) == IVI_FAILED);
	lyt->commit_changes();

	iassert(lyt->screen_remove_layer(NULL, NULL) == IVI_FAILED);
	lyt->commit_changes();

	lyt->layer_destroy(ivilayer);
}


static void
test_commit_changes_after_render_order_set_layer_destroy(
	struct test_context *ctx)
{
#define LAYER_NUM (3)
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct weston_output *output;
	struct ivi_layout_layer *ivilayers[LAYER_NUM] = {};
	uint32_t i;

	if (!iassert(!wl_list_empty(&ctx->compositor->output_list)))
		return;

	output = wl_container_of(ctx->compositor->output_list.next, output, link);

	for (i = 0; i < LAYER_NUM; i++)
		ivilayers[i] = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(i), 200, 300);

	iassert(lyt->screen_set_render_order(output, ivilayers, LAYER_NUM) == IVI_SUCCEEDED);

	lyt->layer_destroy(ivilayers[1]);

	lyt->commit_changes();

	lyt->layer_destroy(ivilayers[0]);
	lyt->layer_destroy(ivilayers[2]);
#undef LAYER_NUM
}

static void
test_layer_properties_changed_notification_callback(struct wl_listener *listener, void *data)
{
	struct test_context *ctx =
			container_of(listener, struct test_context,
					layer_property_changed);
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer = data;
	const struct ivi_layout_layer_properties *prop = lyt->get_properties_of_layer(ivilayer);

	iassert(lyt->get_id_of_layer(ivilayer) == IVI_TEST_LAYER_ID(0));
	iassert(prop->source_width == 200);
	iassert(prop->source_height == 300);

	if (lyt->get_id_of_layer(ivilayer) == IVI_TEST_LAYER_ID(0) &&
	    prop->source_width == 200 && prop->source_height == 300)
		ctx->user_flags = 1;
}

static void
test_layer_properties_changed_notification(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;

	ctx->user_flags = 0;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);

	ctx->layer_property_changed.notify = test_layer_properties_changed_notification_callback;

	iassert(lyt->layer_add_listener(ivilayer, &ctx->layer_property_changed) == IVI_SUCCEEDED);

	lyt->commit_changes();

	iassert(ctx->user_flags == 0);

	iassert(lyt->layer_set_destination_rectangle(
		ivilayer, 20, 30, 200, 300) == IVI_SUCCEEDED);

	lyt->commit_changes();

	iassert(ctx->user_flags == 1);

	ctx->user_flags = 0;
	iassert(lyt->layer_set_destination_rectangle(
		ivilayer, 20, 30, 200, 300) == IVI_SUCCEEDED);

	lyt->commit_changes();

	iassert(ctx->user_flags == 0);

	// remove layer property changed listener.
	wl_list_remove(&ctx->layer_property_changed.link);

	ctx->user_flags = 0;
	lyt->commit_changes();

	iassert(ctx->user_flags == 0);

	lyt->layer_destroy(ivilayer);
}

static void
test_layer_create_notification_callback(struct wl_listener *listener, void *data)
{
	struct test_context *ctx =
			container_of(listener, struct test_context,
					layer_created);
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer = data;
	const struct ivi_layout_layer_properties *prop = lyt->get_properties_of_layer(ivilayer);

	iassert(lyt->get_id_of_layer(ivilayer) == IVI_TEST_LAYER_ID(0));
	iassert(prop->source_width == 200);
	iassert(prop->source_height == 300);

	if (lyt->get_id_of_layer(ivilayer) == IVI_TEST_LAYER_ID(0) &&
	    prop->source_width == 200 && prop->source_height == 300)
		ctx->user_flags = 1;
}

static void
test_layer_create_notification(struct test_context *ctx)
{
#define LAYER_NUM (2)
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	static const uint32_t layers[LAYER_NUM] = {IVI_TEST_LAYER_ID(0), IVI_TEST_LAYER_ID(1)};
	struct ivi_layout_layer *ivilayers[LAYER_NUM] = {};

	ctx->user_flags = 0;
	ctx->layer_created.notify = test_layer_create_notification_callback;

	iassert(lyt->add_listener_create_layer(&ctx->layer_created) == IVI_SUCCEEDED);
	ivilayers[0] = lyt->layer_create_with_dimension(layers[0], 200, 300);

	iassert(ctx->user_flags == 1);

	ctx->user_flags = 0;
	// remove layer created listener.
	wl_list_remove(&ctx->layer_created.link);

	ivilayers[1] = lyt->layer_create_with_dimension(layers[1], 400, 500);

	iassert(ctx->user_flags == 0);

	lyt->layer_destroy(ivilayers[0]);
	lyt->layer_destroy(ivilayers[1]);
#undef LAYER_NUM
}

static void
test_layer_remove_notification_callback(struct wl_listener *listener, void *data)
{
	struct test_context *ctx =
			container_of(listener, struct test_context,
					layer_removed);
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer = data;
	const struct ivi_layout_layer_properties *prop =
		lyt->get_properties_of_layer(ivilayer);

	iassert(lyt->get_id_of_layer(ivilayer) == IVI_TEST_LAYER_ID(0));
	iassert(prop->source_width == 200);
	iassert(prop->source_height == 300);

	if (lyt->get_id_of_layer(ivilayer) == IVI_TEST_LAYER_ID(0) &&
	    prop->source_width == 200 && prop->source_height == 300)
		ctx->user_flags = 1;
}

static void
test_layer_remove_notification(struct test_context *ctx)
{
#define LAYER_NUM (2)
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	static const uint32_t layers[LAYER_NUM] = {IVI_TEST_LAYER_ID(0), IVI_TEST_LAYER_ID(1)};
	struct ivi_layout_layer *ivilayers[LAYER_NUM] = {};

	ctx->user_flags = 0;
	ctx->layer_removed.notify = test_layer_remove_notification_callback;

	ivilayers[0] = lyt->layer_create_with_dimension(layers[0], 200, 300);
	iassert(lyt->add_listener_remove_layer(&ctx->layer_removed) == IVI_SUCCEEDED);
	lyt->layer_destroy(ivilayers[0]);

	iassert(ctx->user_flags == 1);

	ctx->user_flags = 0;
	ivilayers[1] = lyt->layer_create_with_dimension(layers[1], 250, 350);

	// remove layer property changed listener.
	wl_list_remove(&ctx->layer_removed.link);
	lyt->layer_destroy(ivilayers[1]);

	iassert(ctx->user_flags == 0);
#undef LAYER_NUM
}

static void
test_layer_bad_properties_changed_notification_callback(struct wl_listener *listener, void *data)
{
}

static void
test_layer_bad_properties_changed_notification(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = lyt->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);

	ctx->layer_property_changed.notify = test_layer_bad_properties_changed_notification_callback;

	iassert(lyt->layer_add_listener(NULL, &ctx->layer_property_changed) == IVI_FAILED);
	iassert(lyt->layer_add_listener(ivilayer, NULL) == IVI_FAILED);

	lyt->layer_destroy(ivilayer);
}

static void
test_surface_bad_configure_notification(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->add_listener_configure_surface(NULL) == IVI_FAILED);
}

static void
test_layer_bad_create_notification(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->add_listener_create_layer(NULL) == IVI_FAILED);
}

static void
test_surface_bad_create_notification(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->add_listener_create_surface(NULL) == IVI_FAILED);
}

static void
test_layer_bad_remove_notification(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->add_listener_remove_layer(NULL) == IVI_FAILED);
}

static void
test_surface_bad_remove_notification(struct test_context *ctx)
{
	const struct ivi_layout_interface *lyt = ctx->layout_interface;

	iassert(lyt->add_listener_remove_surface(NULL) == IVI_FAILED);
}

/************************ tests end ********************************/

static void
run_internal_tests(void *data)
{
	struct test_context *ctx = data;

	test_surface_bad_visibility(ctx);
	test_surface_bad_destination_rectangle(ctx);
	test_surface_bad_source_rectangle(ctx);
	test_surface_bad_properties(ctx);

	test_layer_create(ctx);
	test_layer_visibility(ctx);
	test_layer_opacity(ctx);
	test_layer_dimension(ctx);
	test_layer_position(ctx);
	test_layer_destination_rectangle(ctx);
	test_layer_source_rectangle(ctx);
	test_layer_bad_remove(ctx);
	test_layer_bad_visibility(ctx);
	test_layer_bad_opacity(ctx);
	test_layer_bad_destination_rectangle(ctx);
	test_layer_bad_source_rectangle(ctx);
	test_layer_bad_properties(ctx);
	test_commit_changes_after_visibility_set_layer_destroy(ctx);
	test_commit_changes_after_opacity_set_layer_destroy(ctx);
	test_commit_changes_after_source_rectangle_set_layer_destroy(ctx);
	test_commit_changes_after_destination_rectangle_set_layer_destroy(ctx);
	test_layer_create_duplicate(ctx);
	test_get_layer_after_destory_layer(ctx);

	test_screen_render_order(ctx);
	test_screen_bad_render_order(ctx);
	test_screen_add_layers(ctx);
	test_screen_remove_layer(ctx);
	test_screen_bad_remove_layer(ctx);
	test_commit_changes_after_render_order_set_layer_destroy(ctx);

	test_layer_properties_changed_notification(ctx);
	test_layer_create_notification(ctx);
	test_layer_remove_notification(ctx);
	test_layer_bad_properties_changed_notification(ctx);
	test_surface_bad_configure_notification(ctx);
	test_layer_bad_create_notification(ctx);
	test_surface_bad_create_notification(ctx);
	test_layer_bad_remove_notification(ctx);
	test_surface_bad_remove_notification(ctx);

	weston_compositor_exit_with_code(ctx->compositor, EXIT_SUCCESS);
	free(ctx);
}

WL_EXPORT int
wet_module_init(struct weston_compositor *compositor,
		       int *argc, char *argv[])
{
	struct wl_event_loop *loop;
	struct test_context *ctx;
	const struct ivi_layout_interface *iface;

	iface = ivi_layout_get_api(compositor);

	if (!iface) {
		weston_log("fatal: cannot use ivi_layout_interface.\n");
		return -1;
	}

	ctx = zalloc(sizeof(*ctx));
	if (!ctx)
		return -1;

	ctx->compositor = compositor;
	ctx->layout_interface = iface;

	loop = wl_display_get_event_loop(compositor->wl_display);
	wl_event_loop_add_idle(loop, run_internal_tests, ctx);

	return 0;
}
