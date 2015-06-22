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

#include "src/compositor.h"
#include "ivi-shell/ivi-layout-export.h"
#include "ivi-shell/ivi-layout-private.h"
#include "ivi-test.h"

struct test_context {
	struct weston_compositor *compositor;
	const struct ivi_controller_interface *controller_interface;
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
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	bool visibility;

	iassert(ctl->surface_set_visibility(NULL, true) == IVI_FAILED);

	ctl->commit_changes();

	visibility = ctl->surface_get_visibility(NULL);
	iassert(visibility == false);
}

static void
test_surface_bad_destination_rectangle(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->surface_set_destination_rectangle(NULL, 20, 30, 200, 300) == IVI_FAILED);
}

static void
test_surface_bad_orientation(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->surface_set_orientation(NULL, WL_OUTPUT_TRANSFORM_90) == IVI_FAILED);

	iassert(ctl->surface_get_orientation(NULL) == WL_OUTPUT_TRANSFORM_NORMAL);
}

static void
test_surface_bad_dimension(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_surface *ivisurf = NULL;
	int32_t dest_width;
	int32_t dest_height;

	iassert(ctl->surface_set_dimension(NULL, 200, 300) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->surface_get_dimension(NULL, &dest_width, &dest_height) == IVI_FAILED);
	iassert(ctl->surface_get_dimension(ivisurf, NULL, &dest_height) == IVI_FAILED);
	iassert(ctl->surface_get_dimension(ivisurf, &dest_width, NULL) == IVI_FAILED);
}

static void
test_surface_bad_position(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_surface *ivisurf = NULL;
	int32_t dest_x;
	int32_t dest_y;

	iassert(ctl->surface_set_position(NULL, 20, 30) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->surface_get_position(NULL, &dest_x, &dest_y) == IVI_FAILED);
	iassert(ctl->surface_get_position(ivisurf, NULL, &dest_y) == IVI_FAILED);
	iassert(ctl->surface_get_position(ivisurf, &dest_x, NULL) == IVI_FAILED);
}

static void
test_surface_bad_source_rectangle(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->surface_set_source_rectangle(NULL, 20, 30, 200, 300) == IVI_FAILED);
}

static void
test_surface_bad_properties(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->get_properties_of_surface(NULL) == NULL);
}

static void
test_layer_create(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	uint32_t id1;
	uint32_t id2;
	struct ivi_layout_layer *ivilayer;
	struct ivi_layout_layer *new_ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(IVI_TEST_LAYER_ID(0) == ctl->get_id_of_layer(ivilayer));

	new_ivilayer = ctl->get_layer_from_id(IVI_TEST_LAYER_ID(0));
	iassert(ivilayer == new_ivilayer);

	id1 = ctl->get_id_of_layer(ivilayer);
	id2 = ctl->get_id_of_layer(new_ivilayer);
	iassert(id1 == id2);

	ctl->layer_destroy(ivilayer);
	iassert(ctl->get_layer_from_id(IVI_TEST_LAYER_ID(0)) == NULL);
}

static void
test_layer_visibility(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_get_visibility(ivilayer) == false);

	iassert(ctl->layer_set_visibility(ivilayer, true) == IVI_SUCCEEDED);

	iassert(ctl->layer_get_visibility(ivilayer) == false);

	ctl->commit_changes();

	iassert(ctl->layer_get_visibility(ivilayer) == true);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->visibility == true);

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_opacity(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_get_opacity(ivilayer) == wl_fixed_from_double(1.0));

	iassert(ctl->layer_set_opacity(
		ivilayer, wl_fixed_from_double(0.5)) == IVI_SUCCEEDED);

	iassert(ctl->layer_get_opacity(ivilayer) == wl_fixed_from_double(1.0));

	ctl->commit_changes();

	iassert(ctl->layer_get_opacity(ivilayer) == wl_fixed_from_double(0.5));

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->opacity == wl_fixed_from_double(0.5));

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_orientation(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_get_orientation(ivilayer) == WL_OUTPUT_TRANSFORM_NORMAL);

	iassert(ctl->layer_set_orientation(
		ivilayer, WL_OUTPUT_TRANSFORM_90) == IVI_SUCCEEDED);

	iassert(ctl->layer_get_orientation(ivilayer) == WL_OUTPUT_TRANSFORM_NORMAL);

	ctl->commit_changes();

	iassert(ctl->layer_get_orientation(ivilayer) == WL_OUTPUT_TRANSFORM_90);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->orientation == WL_OUTPUT_TRANSFORM_90);

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_dimension(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;
	int32_t dest_width;
	int32_t dest_height;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_get_dimension(
		ivilayer, &dest_width, &dest_height) == IVI_SUCCEEDED);
	iassert(dest_width == 200);
	iassert(dest_height == 300);

	iassert(ctl->layer_set_dimension(ivilayer, 400, 600) == IVI_SUCCEEDED);

	iassert(ctl->layer_get_dimension(
		ivilayer, &dest_width, &dest_height) == IVI_SUCCEEDED);
	iassert(dest_width == 200);
	iassert(dest_height == 300);

	ctl->commit_changes();

	iassert(IVI_SUCCEEDED == ctl->layer_get_dimension(
		ivilayer, &dest_width, &dest_height));
	iassert(dest_width == 400);
	iassert(dest_height == 600);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->dest_width == 400);
	iassert(prop->dest_height == 600);

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_position(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;
	int32_t dest_x;
	int32_t dest_y;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_get_position(
		ivilayer, &dest_x, &dest_y) == IVI_SUCCEEDED);
	iassert(dest_x == 0);
	iassert(dest_y == 0);

	iassert(ctl->layer_set_position(ivilayer, 20, 30) == IVI_SUCCEEDED);

	iassert(ctl->layer_get_position(
		ivilayer, &dest_x, &dest_y) == IVI_SUCCEEDED);
	iassert(dest_x == 0);
	iassert(dest_y == 0);

	ctl->commit_changes();

	iassert(ctl->layer_get_position(
		ivilayer, &dest_x, &dest_y) == IVI_SUCCEEDED);
	iassert(dest_x == 20);
	iassert(dest_y == 30);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->dest_x == 20);
	iassert(prop->dest_y == 30);

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_destination_rectangle(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;
	int32_t dest_width;
	int32_t dest_height;
	int32_t dest_x;
	int32_t dest_y;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->dest_width == 200);
	iassert(prop->dest_height == 300);
	iassert(prop->dest_x == 0);
	iassert(prop->dest_y == 0);

	iassert(ctl->layer_set_destination_rectangle(
		ivilayer, 20, 30, 400, 600) == IVI_SUCCEEDED);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->dest_width == 200);
	iassert(prop->dest_height == 300);
	iassert(prop->dest_x == 0);
	iassert(prop->dest_y == 0);

	ctl->commit_changes();

	iassert(ctl->layer_get_dimension(
		ivilayer, &dest_width, &dest_height) == IVI_SUCCEEDED);
	iassert(dest_width == 400);
	iassert(dest_height == 600);

	iassert(ctl->layer_get_position(
		ivilayer, &dest_x, &dest_y) == IVI_SUCCEEDED);
	iassert(dest_x == 20);
	iassert(dest_y == 30);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->dest_width == 400);
	iassert(prop->dest_height == 600);
	iassert(prop->dest_x == 20);
	iassert(prop->dest_y == 30);

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_source_rectangle(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	const struct ivi_layout_layer_properties *prop;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->source_width == 200);
	iassert(prop->source_height == 300);
	iassert(prop->source_x == 0);
	iassert(prop->source_y == 0);

	iassert(ctl->layer_set_source_rectangle(
		ivilayer, 20, 30, 400, 600) == IVI_SUCCEEDED);

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->source_width == 200);
	iassert(prop->source_height == 300);
	iassert(prop->source_x == 0);
	iassert(prop->source_y == 0);

	ctl->commit_changes();

	prop = ctl->get_properties_of_layer(ivilayer);
	iassert(prop->source_width == 400);
	iassert(prop->source_height == 600);
	iassert(prop->source_x == 20);
	iassert(prop->source_y == 30);

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_bad_remove(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	ctl->layer_destroy(NULL);
}

static void
test_layer_bad_visibility(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->layer_set_visibility(NULL, true) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->layer_get_visibility(NULL) == false);
}

static void
test_layer_bad_opacity(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_opacity(
		NULL, wl_fixed_from_double(0.3)) == IVI_FAILED);

	iassert(ctl->layer_set_opacity(
		ivilayer, wl_fixed_from_double(0.3)) == IVI_SUCCEEDED);

	iassert(ctl->layer_set_opacity(
		ivilayer, wl_fixed_from_double(-1)) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->layer_get_opacity(ivilayer) == wl_fixed_from_double(0.3));

	iassert(ctl->layer_set_opacity(
		ivilayer, wl_fixed_from_double(1.1)) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->layer_get_opacity(ivilayer) == wl_fixed_from_double(0.3));

	iassert(ctl->layer_set_opacity(
		NULL, wl_fixed_from_double(0.5)) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->layer_get_opacity(NULL) == wl_fixed_from_double(0.0));

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_bad_destination_rectangle(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->layer_set_destination_rectangle(
		NULL, 20, 30, 200, 300) == IVI_FAILED);
}

static void
test_layer_bad_orientation(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->layer_set_orientation(
		NULL, WL_OUTPUT_TRANSFORM_90) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->layer_get_orientation(NULL) == WL_OUTPUT_TRANSFORM_NORMAL);
}

static void
test_layer_bad_dimension(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	int32_t dest_width;
	int32_t dest_height;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_dimension(NULL, 200, 300) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->layer_get_dimension(
		NULL, &dest_width, &dest_height) == IVI_FAILED);
	iassert(ctl->layer_get_dimension(
		ivilayer, NULL, &dest_height) == IVI_FAILED);
	iassert(ctl->layer_get_dimension(
		ivilayer, &dest_width, NULL) == IVI_FAILED);

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_bad_position(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	int32_t dest_x;
	int32_t dest_y;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_position(NULL, 20, 30) == IVI_FAILED);

	ctl->commit_changes();

	iassert(ctl->layer_get_position(NULL, &dest_x, &dest_y) == IVI_FAILED);
	iassert(ctl->layer_get_position(ivilayer, NULL, &dest_y) == IVI_FAILED);
	iassert(ctl->layer_get_position(ivilayer, &dest_x, NULL) == IVI_FAILED);

	ctl->layer_destroy(ivilayer);
}

static void
test_layer_bad_source_rectangle(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->layer_set_source_rectangle(
		NULL, 20, 30, 200, 300) == IVI_FAILED);
}

static void
test_layer_bad_properties(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;

	iassert(ctl->get_properties_of_layer(NULL) == NULL);
}

static void
test_commit_changes_after_visibility_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_visibility(ivilayer, true) == IVI_SUCCEEDED);
	ctl->layer_destroy(ivilayer);
	ctl->commit_changes();
}

static void
test_commit_changes_after_opacity_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_opacity(
		    ivilayer, wl_fixed_from_double(0.5)) == IVI_SUCCEEDED);
	ctl->layer_destroy(ivilayer);
	ctl->commit_changes();
}

static void
test_commit_changes_after_orientation_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_orientation(
		    ivilayer, WL_OUTPUT_TRANSFORM_90) == IVI_SUCCEEDED);
	ctl->layer_destroy(ivilayer);
	ctl->commit_changes();
}

static void
test_commit_changes_after_dimension_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_dimension(ivilayer, 200, 300) == IVI_SUCCEEDED);
	ctl->layer_destroy(ivilayer);
	ctl->commit_changes();
}

static void
test_commit_changes_after_position_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_position(ivilayer, 20, 30) == IVI_SUCCEEDED);
	ctl->layer_destroy(ivilayer);
	ctl->commit_changes();
}

static void
test_commit_changes_after_source_rectangle_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_source_rectangle(
		    ivilayer, 20, 30, 200, 300) == IVI_SUCCEEDED);
	ctl->layer_destroy(ivilayer);
	ctl->commit_changes();
}

static void
test_commit_changes_after_destination_rectangle_set_layer_destroy(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	iassert(ctl->layer_set_destination_rectangle(
		    ivilayer, 20, 30, 200, 300) == IVI_SUCCEEDED);
	ctl->layer_destroy(ivilayer);
	ctl->commit_changes();
}

static void
test_layer_create_duplicate(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;
	struct ivi_layout_layer *duplicatelayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	if (ivilayer != NULL)
		iassert(ivilayer->ref_count == 1);

	duplicatelayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer == duplicatelayer);

	if (ivilayer != NULL)
		iassert(ivilayer->ref_count == 2);

	ctl->layer_destroy(ivilayer);

	if (ivilayer != NULL)
		iassert(ivilayer->ref_count == 1);

	ctl->layer_destroy(ivilayer);
}

static void
test_get_layer_after_destory_layer(struct test_context *ctx)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_layer *ivilayer;

	ivilayer = ctl->layer_create_with_dimension(IVI_TEST_LAYER_ID(0), 200, 300);
	iassert(ivilayer != NULL);

	ctl->layer_destroy(ivilayer);

	ivilayer = ctl->get_layer_from_id(IVI_TEST_LAYER_ID(0));
	iassert(ivilayer == NULL);
}

/************************ tests end ********************************/

static void
run_internal_tests(void *data)
{
	struct test_context *ctx = data;

	test_surface_bad_visibility(ctx);
	test_surface_bad_destination_rectangle(ctx);
	test_surface_bad_orientation(ctx);
	test_surface_bad_dimension(ctx);
	test_surface_bad_position(ctx);
	test_surface_bad_source_rectangle(ctx);
	test_surface_bad_properties(ctx);

	test_layer_create(ctx);
	test_layer_visibility(ctx);
	test_layer_opacity(ctx);
	test_layer_orientation(ctx);
	test_layer_dimension(ctx);
	test_layer_position(ctx);
	test_layer_destination_rectangle(ctx);
	test_layer_source_rectangle(ctx);
	test_layer_bad_remove(ctx);
	test_layer_bad_visibility(ctx);
	test_layer_bad_opacity(ctx);
	test_layer_bad_destination_rectangle(ctx);
	test_layer_bad_orientation(ctx);
	test_layer_bad_dimension(ctx);
	test_layer_bad_position(ctx);
	test_layer_bad_source_rectangle(ctx);
	test_layer_bad_properties(ctx);
	test_commit_changes_after_visibility_set_layer_destroy(ctx);
	test_commit_changes_after_opacity_set_layer_destroy(ctx);
	test_commit_changes_after_orientation_set_layer_destroy(ctx);
	test_commit_changes_after_dimension_set_layer_destroy(ctx);
	test_commit_changes_after_position_set_layer_destroy(ctx);
	test_commit_changes_after_source_rectangle_set_layer_destroy(ctx);
	test_commit_changes_after_destination_rectangle_set_layer_destroy(ctx);
	test_layer_create_duplicate(ctx);
	test_get_layer_after_destory_layer(ctx);

	weston_compositor_exit_with_code(ctx->compositor, EXIT_SUCCESS);
	free(ctx);
}

int
controller_module_init(struct weston_compositor *compositor,
		       int *argc, char *argv[],
		       const struct ivi_controller_interface *iface,
		       size_t iface_version);

WL_EXPORT int
controller_module_init(struct weston_compositor *compositor,
		       int *argc, char *argv[],
		       const struct ivi_controller_interface *iface,
		       size_t iface_version)
{
	struct wl_event_loop *loop;
	struct test_context *ctx;

	/* strict check, since this is an internal test module */
	if (iface_version != sizeof(*iface)) {
		weston_log("fatal: controller interface mismatch\n");
		return -1;
	}

	ctx = zalloc(sizeof(*ctx));
	if (!ctx)
		return -1;

	ctx->compositor = compositor;
	ctx->controller_interface = iface;

	loop = wl_display_get_event_loop(compositor->wl_display);
	wl_event_loop_add_idle(loop, run_internal_tests, ctx);

	return 0;
}
