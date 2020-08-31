/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2017, 2018 Collabora, Ltd.
 * Copyright © 2017, 2018 General Electric Company
 * Copyright (c) 2018 DisplayLink (UK) Ltd.
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

#include <stdint.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <libweston/libweston.h>
#include <libweston/backend-drm.h>
#include "shared/helpers.h"
#include "drm-internal.h"
#include "pixel-formats.h"
#include "presentation-time-server-protocol.h"

#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0
#endif

struct drm_property_enum_info plane_type_enums[] = {
	[WDRM_PLANE_TYPE_PRIMARY] = {
		.name = "Primary",
	},
	[WDRM_PLANE_TYPE_OVERLAY] = {
		.name = "Overlay",
	},
	[WDRM_PLANE_TYPE_CURSOR] = {
		.name = "Cursor",
	},
};

const struct drm_property_info plane_props[] = {
	[WDRM_PLANE_TYPE] = {
		.name = "type",
		.enum_values = plane_type_enums,
		.num_enum_values = WDRM_PLANE_TYPE__COUNT,
	},
	[WDRM_PLANE_SRC_X] = { .name = "SRC_X", },
	[WDRM_PLANE_SRC_Y] = { .name = "SRC_Y", },
	[WDRM_PLANE_SRC_W] = { .name = "SRC_W", },
	[WDRM_PLANE_SRC_H] = { .name = "SRC_H", },
	[WDRM_PLANE_CRTC_X] = { .name = "CRTC_X", },
	[WDRM_PLANE_CRTC_Y] = { .name = "CRTC_Y", },
	[WDRM_PLANE_CRTC_W] = { .name = "CRTC_W", },
	[WDRM_PLANE_CRTC_H] = { .name = "CRTC_H", },
	[WDRM_PLANE_FB_ID] = { .name = "FB_ID", },
	[WDRM_PLANE_CRTC_ID] = { .name = "CRTC_ID", },
	[WDRM_PLANE_IN_FORMATS] = { .name = "IN_FORMATS" },
	[WDRM_PLANE_IN_FENCE_FD] = { .name = "IN_FENCE_FD" },
	[WDRM_PLANE_FB_DAMAGE_CLIPS] = { .name = "FB_DAMAGE_CLIPS" },
};

struct drm_property_enum_info dpms_state_enums[] = {
	[WDRM_DPMS_STATE_OFF] = {
		.name = "Off",
	},
	[WDRM_DPMS_STATE_ON] = {
		.name = "On",
	},
	[WDRM_DPMS_STATE_STANDBY] = {
		.name = "Standby",
	},
	[WDRM_DPMS_STATE_SUSPEND] = {
		.name = "Suspend",
	},
};

const struct drm_property_info connector_props[] = {
	[WDRM_CONNECTOR_EDID] = { .name = "EDID" },
	[WDRM_CONNECTOR_DPMS] = {
		.name = "DPMS",
		.enum_values = dpms_state_enums,
		.num_enum_values = WDRM_DPMS_STATE__COUNT,
	},
	[WDRM_CONNECTOR_CRTC_ID] = { .name = "CRTC_ID", },
	[WDRM_CONNECTOR_NON_DESKTOP] = { .name = "non-desktop", },
};

const struct drm_property_info crtc_props[] = {
	[WDRM_CRTC_MODE_ID] = { .name = "MODE_ID", },
	[WDRM_CRTC_ACTIVE] = { .name = "ACTIVE", },
};


/**
 * Mode for drm_pending_state_apply and co.
 */
enum drm_state_apply_mode {
	DRM_STATE_APPLY_SYNC, /**< state fully processed */
	DRM_STATE_APPLY_ASYNC, /**< state pending event delivery */
	DRM_STATE_TEST_ONLY, /**< test if the state can be applied */
};

/**
 * Get the current value of a KMS property
 *
 * Given a drmModeObjectGetProperties return, as well as the drm_property_info
 * for the target property, return the current value of that property,
 * with an optional default. If the property is a KMS enum type, the return
 * value will be translated into the appropriate internal enum.
 *
 * If the property is not present, the default value will be returned.
 *
 * @param info Internal structure for property to look up
 * @param props Raw KMS properties for the target object
 * @param def Value to return if property is not found
 */
uint64_t
drm_property_get_value(struct drm_property_info *info,
		       const drmModeObjectProperties *props,
		       uint64_t def)
{
	unsigned int i;

	if (info->prop_id == 0)
		return def;

	for (i = 0; i < props->count_props; i++) {
		unsigned int j;

		if (props->props[i] != info->prop_id)
			continue;

		/* Simple (non-enum) types can return the value directly */
		if (info->num_enum_values == 0)
			return props->prop_values[i];

		/* Map from raw value to enum value */
		for (j = 0; j < info->num_enum_values; j++) {
			if (!info->enum_values[j].valid)
				continue;
			if (info->enum_values[j].value != props->prop_values[i])
				continue;

			return j;
		}

		/* We don't have a mapping for this enum; return default. */
		break;
	}

	return def;
}

/**
 * Cache DRM property values
 *
 * Update a per-object array of drm_property_info structures, given the
 * DRM properties of the object.
 *
 * Call this every time an object newly appears (note that only connectors
 * can be hotplugged), the first time it is seen, or when its status changes
 * in a way which invalidates the potential property values (currently, the
 * only case for this is connector hotplug).
 *
 * This updates the property IDs and enum values within the drm_property_info
 * array.
 *
 * DRM property enum values are dynamic at runtime; the user must query the
 * property to find out the desired runtime value for a requested string
 * name. Using the 'type' field on planes as an example, there is no single
 * hardcoded constant for primary plane types; instead, the property must be
 * queried at runtime to find the value associated with the string "Primary".
 *
 * This helper queries and caches the enum values, to allow us to use a set
 * of compile-time-constant enums portably across various implementations.
 * The values given in enum_names are searched for, and stored in the
 * same-indexed field of the map array.
 *
 * @param b DRM backend object
 * @param src DRM property info array to source from
 * @param info DRM property info array to copy into
 * @param num_infos Number of entries in the source array
 * @param props DRM object properties for the object
 */
void
drm_property_info_populate(struct drm_backend *b,
		           const struct drm_property_info *src,
			   struct drm_property_info *info,
			   unsigned int num_infos,
			   drmModeObjectProperties *props)
{
	drmModePropertyRes *prop;
	unsigned i, j;

	for (i = 0; i < num_infos; i++) {
		unsigned int j;

		info[i].name = src[i].name;
		info[i].prop_id = 0;
		info[i].num_enum_values = src[i].num_enum_values;

		if (src[i].num_enum_values == 0)
			continue;

		info[i].enum_values =
			malloc(src[i].num_enum_values *
			       sizeof(*info[i].enum_values));
		assert(info[i].enum_values);
		for (j = 0; j < info[i].num_enum_values; j++) {
			info[i].enum_values[j].name = src[i].enum_values[j].name;
			info[i].enum_values[j].valid = false;
		}
	}

	for (i = 0; i < props->count_props; i++) {
		unsigned int k;

		prop = drmModeGetProperty(b->drm.fd, props->props[i]);
		if (!prop)
			continue;

		for (j = 0; j < num_infos; j++) {
			if (!strcmp(prop->name, info[j].name))
				break;
		}

		/* We don't know/care about this property. */
		if (j == num_infos) {
#ifdef DEBUG
			weston_log("DRM debug: unrecognized property %u '%s'\n",
				   prop->prop_id, prop->name);
#endif
			drmModeFreeProperty(prop);
			continue;
		}

		if (info[j].num_enum_values == 0 &&
		    (prop->flags & DRM_MODE_PROP_ENUM)) {
			weston_log("DRM: expected property %s to not be an"
			           " enum, but it is; ignoring\n", prop->name);
			drmModeFreeProperty(prop);
			continue;
		}

		info[j].prop_id = props->props[i];

		if (info[j].num_enum_values == 0) {
			drmModeFreeProperty(prop);
			continue;
		}

		if (!(prop->flags & DRM_MODE_PROP_ENUM)) {
			weston_log("DRM: expected property %s to be an enum,"
				   " but it is not; ignoring\n", prop->name);
			drmModeFreeProperty(prop);
			info[j].prop_id = 0;
			continue;
		}

		for (k = 0; k < info[j].num_enum_values; k++) {
			int l;

			for (l = 0; l < prop->count_enums; l++) {
				if (!strcmp(prop->enums[l].name,
					    info[j].enum_values[k].name))
					break;
			}

			if (l == prop->count_enums)
				continue;

			info[j].enum_values[k].valid = true;
			info[j].enum_values[k].value = prop->enums[l].value;
		}

		drmModeFreeProperty(prop);
	}

#ifdef DEBUG
	for (i = 0; i < num_infos; i++) {
		if (info[i].prop_id == 0)
			weston_log("DRM warning: property '%s' missing\n",
				   info[i].name);
	}
#endif
}

/**
 * Free DRM property information
 *
 * Frees all memory associated with a DRM property info array and zeroes
 * it out, leaving it usable for a further drm_property_info_update() or
 * drm_property_info_free().
 *
 * @param info DRM property info array
 * @param num_props Number of entries in array to free
 */
void
drm_property_info_free(struct drm_property_info *info, int num_props)
{
	int i;

	for (i = 0; i < num_props; i++)
		free(info[i].enum_values);

	memset(info, 0, sizeof(*info) * num_props);
}

#ifdef HAVE_DRM_FORMATS_BLOB
static inline uint32_t *
formats_ptr(struct drm_format_modifier_blob *blob)
{
	return (uint32_t *)(((char *)blob) + blob->formats_offset);
}

static inline struct drm_format_modifier *
modifiers_ptr(struct drm_format_modifier_blob *blob)
{
	return (struct drm_format_modifier *)
		(((char *)blob) + blob->modifiers_offset);
}
#endif

/**
 * Populates the plane's formats array, using either the IN_FORMATS blob
 * property (if available), or the plane's format list if not.
 */
int
drm_plane_populate_formats(struct drm_plane *plane, const drmModePlane *kplane,
			   const drmModeObjectProperties *props)
{
	unsigned i;
#ifdef HAVE_DRM_FORMATS_BLOB
	drmModePropertyBlobRes *blob;
	struct drm_format_modifier_blob *fmt_mod_blob;
	struct drm_format_modifier *blob_modifiers;
	uint32_t *blob_formats;
	uint32_t blob_id;

	blob_id = drm_property_get_value(&plane->props[WDRM_PLANE_IN_FORMATS],
				         props,
				         0);
	if (blob_id == 0)
		goto fallback;

	blob = drmModeGetPropertyBlob(plane->backend->drm.fd, blob_id);
	if (!blob)
		goto fallback;

	fmt_mod_blob = blob->data;
	blob_formats = formats_ptr(fmt_mod_blob);
	blob_modifiers = modifiers_ptr(fmt_mod_blob);

	if (plane->count_formats != fmt_mod_blob->count_formats) {
		weston_log("DRM backend: format count differs between "
		           "plane (%d) and IN_FORMATS (%d)\n",
			   plane->count_formats,
			   fmt_mod_blob->count_formats);
		weston_log("This represents a kernel bug; Weston is "
			   "unable to continue.\n");
		abort();
	}

	for (i = 0; i < fmt_mod_blob->count_formats; i++) {
		uint32_t count_modifiers = 0;
		uint64_t *modifiers = NULL;
		unsigned j;

		for (j = 0; j < fmt_mod_blob->count_modifiers; j++) {
			struct drm_format_modifier *mod = &blob_modifiers[j];

			if ((i < mod->offset) || (i > mod->offset + 63))
				continue;
			if (!(mod->formats & (1 << (i - mod->offset))))
				continue;

			modifiers = realloc(modifiers,
					    (count_modifiers + 1) *
					     sizeof(modifiers[0]));
			assert(modifiers);
			modifiers[count_modifiers++] = mod->modifier;
		}

		if (count_modifiers == 0) {
			modifiers = malloc(sizeof(*modifiers));
			*modifiers = DRM_FORMAT_MOD_LINEAR;
			count_modifiers = 1;
		}

		plane->formats[i].format = blob_formats[i];
		plane->formats[i].modifiers = modifiers;
		plane->formats[i].count_modifiers = count_modifiers;
	}

	drmModeFreePropertyBlob(blob);

	return 0;

fallback:
#endif
	/* No IN_FORMATS blob available, so just use the old. */
	assert(plane->count_formats == kplane->count_formats);
	for (i = 0; i < kplane->count_formats; i++) {
		plane->formats[i].format = kplane->formats[i];
		plane->formats[i].modifiers = malloc(sizeof(uint64_t));
		plane->formats[i].modifiers[0] = DRM_FORMAT_MOD_LINEAR;
		plane->formats[i].count_modifiers = 1;
	}

	return 0;
}

void
drm_output_set_gamma(struct weston_output *output_base,
		     uint16_t size, uint16_t *r, uint16_t *g, uint16_t *b)
{
	int rc;
	struct drm_output *output = to_drm_output(output_base);
	struct drm_backend *backend =
		to_drm_backend(output->base.compositor);

	/* check */
	if (output_base->gamma_size != size)
		return;

	rc = drmModeCrtcSetGamma(backend->drm.fd,
				 output->crtc_id,
				 size, r, g, b);
	if (rc)
		weston_log("set gamma failed: %s\n", strerror(errno));
}

/**
 * Mark an output state as current on the output, i.e. it has been
 * submitted to the kernel. The mode argument determines whether this
 * update will be applied synchronously (e.g. when calling drmModeSetCrtc),
 * or asynchronously (in which case we wait for events to complete).
 */
static void
drm_output_assign_state(struct drm_output_state *state,
			enum drm_state_apply_mode mode)
{
	struct drm_output *output = state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_plane_state *plane_state;

	assert(!output->state_last);

	if (mode == DRM_STATE_APPLY_ASYNC)
		output->state_last = output->state_cur;
	else
		drm_output_state_free(output->state_cur);

	wl_list_remove(&state->link);
	wl_list_init(&state->link);
	state->pending_state = NULL;

	output->state_cur = state;

	if (b->atomic_modeset && mode == DRM_STATE_APPLY_ASYNC) {
		drm_debug(b, "\t[CRTC:%u] setting pending flip\n", output->crtc_id);
		output->atomic_complete_pending = 1;
	}

	/* Replace state_cur on each affected plane with the new state, being
	 * careful to dispose of orphaned (but only orphaned) previous state.
	 * If the previous state is not orphaned (still has an output_state
	 * attached), it will be disposed of by freeing the output_state. */
	wl_list_for_each(plane_state, &state->plane_list, link) {
		struct drm_plane *plane = plane_state->plane;

		if (plane->state_cur && !plane->state_cur->output_state)
			drm_plane_state_free(plane->state_cur, true);
		plane->state_cur = plane_state;

		if (mode != DRM_STATE_APPLY_ASYNC) {
			plane_state->complete = true;
			continue;
		}

		if (b->atomic_modeset)
			continue;

		assert(plane->type != WDRM_PLANE_TYPE_OVERLAY);
		if (plane->type == WDRM_PLANE_TYPE_PRIMARY)
			output->page_flip_pending = 1;
	}
}

static void
drm_output_set_cursor(struct drm_output_state *output_state)
{
	struct drm_output *output = output_state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_plane *plane = output->cursor_plane;
	struct drm_plane_state *state;
	uint32_t handle;

	if (!plane)
		return;

	state = drm_output_state_get_existing_plane(output_state, plane);
	if (!state)
		return;

	if (!state->fb) {
		pixman_region32_fini(&plane->base.damage);
		pixman_region32_init(&plane->base.damage);
		drmModeSetCursor(b->drm.fd, output->crtc_id, 0, 0, 0);
		return;
	}

	assert(state->fb == output->gbm_cursor_fb[output->current_cursor]);
	assert(!plane->state_cur->output || plane->state_cur->output == output);

	handle = output->gbm_cursor_handle[output->current_cursor];
	if (plane->state_cur->fb != state->fb) {
		if (drmModeSetCursor(b->drm.fd, output->crtc_id, handle,
				     b->cursor_width, b->cursor_height)) {
			weston_log("failed to set cursor: %s\n",
				   strerror(errno));
			goto err;
		}
	}

	pixman_region32_fini(&plane->base.damage);
	pixman_region32_init(&plane->base.damage);

	if (drmModeMoveCursor(b->drm.fd, output->crtc_id,
	                      state->dest_x, state->dest_y)) {
		weston_log("failed to move cursor: %s\n", strerror(errno));
		goto err;
	}

	return;

err:
	b->cursors_are_broken = 1;
	drmModeSetCursor(b->drm.fd, output->crtc_id, 0, 0, 0);
}

static int
drm_output_apply_state_legacy(struct drm_output_state *state)
{
	struct drm_output *output = state->output;
	struct drm_backend *backend = to_drm_backend(output->base.compositor);
	struct drm_plane *scanout_plane = output->scanout_plane;
	struct drm_property_info *dpms_prop;
	struct drm_plane_state *scanout_state;
	struct drm_mode *mode;
	struct drm_head *head;
	const struct pixel_format_info *pinfo = NULL;
	uint32_t connectors[MAX_CLONED_CONNECTORS];
	int n_conn = 0;
	struct timespec now;
	int ret = 0;

	wl_list_for_each(head, &output->base.head_list, base.output_link) {
		assert(n_conn < MAX_CLONED_CONNECTORS);
		connectors[n_conn++] = head->connector_id;
	}

	/* If disable_planes is set then assign_planes() wasn't
	 * called for this render, so we could still have a stale
	 * cursor plane set up.
	 */
	if (output->base.disable_planes) {
		output->cursor_view = NULL;
		if (output->cursor_plane) {
			output->cursor_plane->base.x = INT32_MIN;
			output->cursor_plane->base.y = INT32_MIN;
		}
	}

	if (state->dpms != WESTON_DPMS_ON) {
		if (output->cursor_plane) {
			ret = drmModeSetCursor(backend->drm.fd, output->crtc_id,
					       0, 0, 0);
			if (ret)
				weston_log("drmModeSetCursor failed disable: %s\n",
					   strerror(errno));
		}

		ret = drmModeSetCrtc(backend->drm.fd, output->crtc_id, 0, 0, 0,
				     NULL, 0, NULL);
		if (ret)
			weston_log("drmModeSetCrtc failed disabling: %s\n",
				   strerror(errno));

		drm_output_assign_state(state, DRM_STATE_APPLY_SYNC);
		weston_compositor_read_presentation_clock(output->base.compositor, &now);
		drm_output_update_complete(output,
		                           WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION,
					   now.tv_sec, now.tv_nsec / 1000);

		return 0;
	}

	scanout_state =
		drm_output_state_get_existing_plane(state, scanout_plane);

	/* The legacy SetCrtc API doesn't allow us to do scaling, and the
	 * legacy PageFlip API doesn't allow us to do clipping either. */
	assert(scanout_state->src_x == 0);
	assert(scanout_state->src_y == 0);
	assert(scanout_state->src_w ==
		(unsigned) (output->base.current_mode->width << 16));
	assert(scanout_state->src_h ==
		(unsigned) (output->base.current_mode->height << 16));
	assert(scanout_state->dest_x == 0);
	assert(scanout_state->dest_y == 0);
	assert(scanout_state->dest_w == scanout_state->src_w >> 16);
	assert(scanout_state->dest_h == scanout_state->src_h >> 16);
	/* The legacy SetCrtc API doesn't support fences */
	assert(scanout_state->in_fence_fd == -1);

	mode = to_drm_mode(output->base.current_mode);
	if (backend->state_invalid ||
	    !scanout_plane->state_cur->fb ||
	    scanout_plane->state_cur->fb->strides[0] !=
	    scanout_state->fb->strides[0]) {

		ret = drmModeSetCrtc(backend->drm.fd, output->crtc_id,
				     scanout_state->fb->fb_id,
				     0, 0,
				     connectors, n_conn,
				     &mode->mode_info);
		if (ret) {
			weston_log("set mode failed: %s\n", strerror(errno));
			goto err;
		}
	}

	pinfo = scanout_state->fb->format;
	drm_debug(backend, "\t[CRTC:%u, PLANE:%u] FORMAT: %s\n",
			   output->crtc_id, scanout_state->plane->plane_id,
			   pinfo ? pinfo->drm_format_name : "UNKNOWN");

	if (drmModePageFlip(backend->drm.fd, output->crtc_id,
			    scanout_state->fb->fb_id,
			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
		weston_log("queueing pageflip failed: %s\n", strerror(errno));
		goto err;
	}

	assert(!output->page_flip_pending);

	if (output->pageflip_timer)
		wl_event_source_timer_update(output->pageflip_timer,
		                             backend->pageflip_timeout);

	drm_output_set_cursor(state);

	if (state->dpms != output->state_cur->dpms) {
		wl_list_for_each(head, &output->base.head_list, base.output_link) {
			dpms_prop = &head->props_conn[WDRM_CONNECTOR_DPMS];
			if (dpms_prop->prop_id == 0)
				continue;

			ret = drmModeConnectorSetProperty(backend->drm.fd,
							  head->connector_id,
							  dpms_prop->prop_id,
							  state->dpms);
			if (ret) {
				weston_log("DRM: DPMS: failed property set for %s\n",
					   head->base.name);
			}
		}
	}

	drm_output_assign_state(state, DRM_STATE_APPLY_ASYNC);

	return 0;

err:
	output->cursor_view = NULL;
	drm_output_state_free(state);
	return -1;
}

#ifdef HAVE_DRM_ATOMIC
static int
crtc_add_prop(drmModeAtomicReq *req, struct drm_output *output,
	      enum wdrm_crtc_property prop, uint64_t val)
{
	struct drm_property_info *info = &output->props_crtc[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, output->crtc_id, info->prop_id,
				       val);
	drm_debug(output->backend, "\t\t\t[CRTC:%lu] %lu (%s) -> %llu (0x%llx)\n",
		  (unsigned long) output->crtc_id,
		  (unsigned long) info->prop_id, info->name,
		  (unsigned long long) val, (unsigned long long) val);
	return (ret <= 0) ? -1 : 0;
}

static int
connector_add_prop(drmModeAtomicReq *req, struct drm_head *head,
		   enum wdrm_connector_property prop, uint64_t val)
{
	struct drm_property_info *info = &head->props_conn[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, head->connector_id,
				       info->prop_id, val);
	drm_debug(head->backend, "\t\t\t[CONN:%lu] %lu (%s) -> %llu (0x%llx)\n",
		  (unsigned long) head->connector_id,
		  (unsigned long) info->prop_id, info->name,
		  (unsigned long long) val, (unsigned long long) val);
	return (ret <= 0) ? -1 : 0;
}

static int
plane_add_prop(drmModeAtomicReq *req, struct drm_plane *plane,
	       enum wdrm_plane_property prop, uint64_t val)
{
	struct drm_property_info *info = &plane->props[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, plane->plane_id, info->prop_id,
				       val);
	drm_debug(plane->backend, "\t\t\t[PLANE:%lu] %lu (%s) -> %llu (0x%llx)\n",
		  (unsigned long) plane->plane_id,
		  (unsigned long) info->prop_id, info->name,
		  (unsigned long long) val, (unsigned long long) val);
	return (ret <= 0) ? -1 : 0;
}


static int
plane_add_damage(drmModeAtomicReq *req, struct drm_backend *backend,
		 struct drm_plane_state *plane_state)
{
	struct drm_plane *plane = plane_state->plane;
	struct drm_property_info *info =
		&plane->props[WDRM_PLANE_FB_DAMAGE_CLIPS];
	pixman_box32_t *rects;
	uint32_t blob_id;
	int n_rects;
	int ret;

	if (!pixman_region32_not_empty(&plane_state->damage))
		return 0;

	/*
	 * If a plane doesn't support fb damage blob property, kernel will
	 * perform full plane update.
	 */
	if (info->prop_id == 0)
		return 0;

	rects = pixman_region32_rectangles(&plane_state->damage, &n_rects);

	ret = drmModeCreatePropertyBlob(backend->drm.fd, rects,
					sizeof(*rects) * n_rects, &blob_id);
	if (ret != 0)
		return ret;

	ret = plane_add_prop(req, plane, WDRM_PLANE_FB_DAMAGE_CLIPS, blob_id);
	if (ret != 0)
		return ret;

	return 0;
}

static int
drm_output_apply_state_atomic(struct drm_output_state *state,
			      drmModeAtomicReq *req,
			      uint32_t *flags)
{
	struct drm_output *output = state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_plane_state *plane_state;
	struct drm_mode *current_mode = to_drm_mode(output->base.current_mode);
	struct drm_head *head;
	int ret = 0;

	drm_debug(b, "\t\t[atomic] %s output %lu (%s) state\n",
		  (*flags & DRM_MODE_ATOMIC_TEST_ONLY) ? "testing" : "applying",
		  (unsigned long) output->base.id, output->base.name);

	if (state->dpms != output->state_cur->dpms) {
		drm_debug(b, "\t\t\t[atomic] DPMS state differs, modeset OK\n");
		*flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
	}

	if (state->dpms == WESTON_DPMS_ON) {
		ret = drm_mode_ensure_blob(b, current_mode);
		if (ret != 0)
			return ret;

		ret |= crtc_add_prop(req, output, WDRM_CRTC_MODE_ID,
				     current_mode->blob_id);
		ret |= crtc_add_prop(req, output, WDRM_CRTC_ACTIVE, 1);

		/* No need for the DPMS property, since it is implicit in
		 * routing and CRTC activity. */
		wl_list_for_each(head, &output->base.head_list, base.output_link) {
			ret |= connector_add_prop(req, head, WDRM_CONNECTOR_CRTC_ID,
						  output->crtc_id);
		}
	} else {
		ret |= crtc_add_prop(req, output, WDRM_CRTC_MODE_ID, 0);
		ret |= crtc_add_prop(req, output, WDRM_CRTC_ACTIVE, 0);

		/* No need for the DPMS property, since it is implicit in
		 * routing and CRTC activity. */
		wl_list_for_each(head, &output->base.head_list, base.output_link)
			ret |= connector_add_prop(req, head, WDRM_CONNECTOR_CRTC_ID, 0);
	}

	if (ret != 0) {
		weston_log("couldn't set atomic CRTC/connector state\n");
		return ret;
	}

	wl_list_for_each(plane_state, &state->plane_list, link) {
		struct drm_plane *plane = plane_state->plane;
		const struct pixel_format_info *pinfo = NULL;

		ret |= plane_add_prop(req, plane, WDRM_PLANE_FB_ID,
				      plane_state->fb ? plane_state->fb->fb_id : 0);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_ID,
				      plane_state->fb ? output->crtc_id : 0);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_SRC_X,
				      plane_state->src_x);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_SRC_Y,
				      plane_state->src_y);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_SRC_W,
				      plane_state->src_w);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_SRC_H,
				      plane_state->src_h);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_X,
				      plane_state->dest_x);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_Y,
				      plane_state->dest_y);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_W,
				      plane_state->dest_w);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_H,
				      plane_state->dest_h);
		ret |= plane_add_damage(req, b, plane_state);

		if (plane_state->fb && plane_state->fb->format)
			pinfo = plane_state->fb->format;

		drm_debug(plane->backend, "\t\t\t[PLANE:%lu] FORMAT: %s\n",
				(unsigned long) plane->plane_id,
				pinfo ? pinfo->drm_format_name : "UNKNOWN");

		if (plane_state->in_fence_fd >= 0) {
			ret |= plane_add_prop(req, plane,
					      WDRM_PLANE_IN_FENCE_FD,
					      plane_state->in_fence_fd);
		}

		if (ret != 0) {
			weston_log("couldn't set plane state\n");
			return ret;
		}
	}

	return 0;
}

/**
 * Helper function used only by drm_pending_state_apply, with the same
 * guarantees and constraints as that function.
 */
static int
drm_pending_state_apply_atomic(struct drm_pending_state *pending_state,
			       enum drm_state_apply_mode mode)
{
	struct drm_backend *b = pending_state->backend;
	struct drm_output_state *output_state, *tmp;
	struct drm_plane *plane;
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	uint32_t flags;
	int ret = 0;

	if (!req)
		return -1;

	switch (mode) {
	case DRM_STATE_APPLY_SYNC:
		flags = 0;
		break;
	case DRM_STATE_APPLY_ASYNC:
		flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;
		break;
	case DRM_STATE_TEST_ONLY:
		flags = DRM_MODE_ATOMIC_TEST_ONLY;
		break;
	}

	if (b->state_invalid) {
		struct weston_head *head_base;
		struct drm_head *head;
		uint32_t *unused;
		int err;

		drm_debug(b, "\t\t[atomic] previous state invalid; "
			     "starting with fresh state\n");

		/* If we need to reset all our state (e.g. because we've
		 * just started, or just been VT-switched in), explicitly
		 * disable all the CRTCs and connectors we aren't using. */
		wl_list_for_each(head_base,
				 &b->compositor->head_list, compositor_link) {
			struct drm_property_info *info;

			if (weston_head_is_enabled(head_base))
				continue;

			head = to_drm_head(head_base);

			drm_debug(b, "\t\t[atomic] disabling inactive head %s\n",
				  head_base->name);

			info = &head->props_conn[WDRM_CONNECTOR_CRTC_ID];
			err = drmModeAtomicAddProperty(req, head->connector_id,
						       info->prop_id, 0);
			drm_debug(b, "\t\t\t[CONN:%lu] %lu (%s) -> 0\n",
				  (unsigned long) head->connector_id,
				  (unsigned long) info->prop_id,
				  info->name);
			if (err <= 0)
				ret = -1;
		}

		wl_array_for_each(unused, &b->unused_crtcs) {
			struct drm_property_info infos[WDRM_CRTC__COUNT];
			struct drm_property_info *info;
			drmModeObjectProperties *props;
			uint64_t active;

			memset(infos, 0, sizeof(infos));

			/* We can't emit a disable on a CRTC that's already
			 * off, as the kernel will refuse to generate an event
			 * for an off->off state and fail the commit.
			 */
			props = drmModeObjectGetProperties(b->drm.fd,
							   *unused,
							   DRM_MODE_OBJECT_CRTC);
			if (!props) {
				ret = -1;
				continue;
			}

			drm_property_info_populate(b, crtc_props, infos,
						   WDRM_CRTC__COUNT,
						   props);

			info = &infos[WDRM_CRTC_ACTIVE];
			active = drm_property_get_value(info, props, 0);
			drmModeFreeObjectProperties(props);
			if (active == 0) {
				drm_property_info_free(infos, WDRM_CRTC__COUNT);
				continue;
			}

			drm_debug(b, "\t\t[atomic] disabling unused CRTC %lu\n",
				  (unsigned long) *unused);

			drm_debug(b, "\t\t\t[CRTC:%lu] %lu (%s) -> 0\n",
				  (unsigned long) *unused,
				  (unsigned long) info->prop_id, info->name);
			err = drmModeAtomicAddProperty(req, *unused,
						       info->prop_id, 0);
			if (err <= 0)
				ret = -1;

			info = &infos[WDRM_CRTC_MODE_ID];
			drm_debug(b, "\t\t\t[CRTC:%lu] %lu (%s) -> 0\n",
				  (unsigned long) *unused,
				  (unsigned long) info->prop_id, info->name);
			err = drmModeAtomicAddProperty(req, *unused,
						       info->prop_id, 0);
			if (err <= 0)
				ret = -1;

			drm_property_info_free(infos, WDRM_CRTC__COUNT);
		}

		/* Disable all the planes; planes which are being used will
		 * override this state in the output-state application. */
		wl_list_for_each(plane, &b->plane_list, link) {
			drm_debug(b, "\t\t[atomic] starting with plane %lu disabled\n",
				  (unsigned long) plane->plane_id);
			plane_add_prop(req, plane, WDRM_PLANE_CRTC_ID, 0);
			plane_add_prop(req, plane, WDRM_PLANE_FB_ID, 0);
		}

		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
	}

	wl_list_for_each(output_state, &pending_state->output_list, link) {
		if (output_state->output->virtual)
			continue;
		if (mode == DRM_STATE_APPLY_SYNC)
			assert(output_state->dpms == WESTON_DPMS_OFF);
		ret |= drm_output_apply_state_atomic(output_state, req, &flags);
	}

	if (ret != 0) {
		weston_log("atomic: couldn't compile atomic state\n");
		goto out;
	}

	ret = drmModeAtomicCommit(b->drm.fd, req, flags, b);
	drm_debug(b, "[atomic] drmModeAtomicCommit\n");

	/* Test commits do not take ownership of the state; return
	 * without freeing here. */
	if (mode == DRM_STATE_TEST_ONLY) {
		drmModeAtomicFree(req);
		return ret;
	}

	if (ret != 0) {
		weston_log("atomic: couldn't commit new state: %s\n",
			   strerror(errno));
		goto out;
	}

	wl_list_for_each_safe(output_state, tmp, &pending_state->output_list,
			      link)
		drm_output_assign_state(output_state, mode);

	b->state_invalid = false;

	assert(wl_list_empty(&pending_state->output_list));

out:
	drmModeAtomicFree(req);
	drm_pending_state_free(pending_state);
	return ret;
}
#endif

/**
 * Tests a pending state, to see if the kernel will accept the update as
 * constructed.
 *
 * Using atomic modesetting, the kernel performs the same checks as it would
 * on a real commit, returning success or failure without actually modifying
 * the running state. It does not return -EBUSY if there are pending updates
 * in flight, so states may be tested at any point, however this means a
 * state which passed testing may fail on a real commit if the timing is not
 * respected (e.g. committing before the previous commit has completed).
 *
 * Without atomic modesetting, we have no way to check, so we optimistically
 * claim it will work.
 *
 * Unlike drm_pending_state_apply() and drm_pending_state_apply_sync(), this
 * function does _not_ take ownership of pending_state, nor does it clear
 * state_invalid.
 */
int
drm_pending_state_test(struct drm_pending_state *pending_state)
{
#ifdef HAVE_DRM_ATOMIC
	struct drm_backend *b = pending_state->backend;

	if (b->atomic_modeset)
		return drm_pending_state_apply_atomic(pending_state,
						      DRM_STATE_TEST_ONLY);
#endif

	/* We have no way to test state before application on the legacy
	 * modesetting API, so just claim it succeeded. */
	return 0;
}

/**
 * Applies all of a pending_state asynchronously: the primary entry point for
 * applying KMS state to a device. Updates the state for all outputs in the
 * pending_state, as well as disabling any unclaimed outputs.
 *
 * Unconditionally takes ownership of pending_state, and clears state_invalid.
 */
int
drm_pending_state_apply(struct drm_pending_state *pending_state)
{
	struct drm_backend *b = pending_state->backend;
	struct drm_output_state *output_state, *tmp;
	uint32_t *unused;

#ifdef HAVE_DRM_ATOMIC
	if (b->atomic_modeset)
		return drm_pending_state_apply_atomic(pending_state,
						      DRM_STATE_APPLY_ASYNC);
#endif

	if (b->state_invalid) {
		/* If we need to reset all our state (e.g. because we've
		 * just started, or just been VT-switched in), explicitly
		 * disable all the CRTCs we aren't using. This also disables
		 * all connectors on these CRTCs, so we don't need to do that
		 * separately with the pre-atomic API. */
		wl_array_for_each(unused, &b->unused_crtcs)
			drmModeSetCrtc(b->drm.fd, *unused, 0, 0, 0, NULL, 0,
				       NULL);
	}

	wl_list_for_each_safe(output_state, tmp, &pending_state->output_list,
			      link) {
		struct drm_output *output = output_state->output;
		int ret;

		if (output->virtual) {
			drm_output_assign_state(output_state,
						DRM_STATE_APPLY_ASYNC);
			continue;
		}

		ret = drm_output_apply_state_legacy(output_state);
		if (ret != 0) {
			weston_log("Couldn't apply state for output %s\n",
				   output->base.name);
		}
	}

	b->state_invalid = false;

	assert(wl_list_empty(&pending_state->output_list));

	drm_pending_state_free(pending_state);

	return 0;
}

/**
 * The synchronous version of drm_pending_state_apply. May only be used to
 * disable outputs. Does so synchronously: the request is guaranteed to have
 * completed on return, and the output will not be touched afterwards.
 *
 * Unconditionally takes ownership of pending_state, and clears state_invalid.
 */
int
drm_pending_state_apply_sync(struct drm_pending_state *pending_state)
{
	struct drm_backend *b = pending_state->backend;
	struct drm_output_state *output_state, *tmp;
	uint32_t *unused;

#ifdef HAVE_DRM_ATOMIC
	if (b->atomic_modeset)
		return drm_pending_state_apply_atomic(pending_state,
						      DRM_STATE_APPLY_SYNC);
#endif

	if (b->state_invalid) {
		/* If we need to reset all our state (e.g. because we've
		 * just started, or just been VT-switched in), explicitly
		 * disable all the CRTCs we aren't using. This also disables
		 * all connectors on these CRTCs, so we don't need to do that
		 * separately with the pre-atomic API. */
		wl_array_for_each(unused, &b->unused_crtcs)
			drmModeSetCrtc(b->drm.fd, *unused, 0, 0, 0, NULL, 0,
				       NULL);
	}

	wl_list_for_each_safe(output_state, tmp, &pending_state->output_list,
			      link) {
		int ret;

		assert(output_state->dpms == WESTON_DPMS_OFF);
		ret = drm_output_apply_state_legacy(output_state);
		if (ret != 0) {
			weston_log("Couldn't apply state for output %s\n",
				   output_state->output->base.name);
		}
	}

	b->state_invalid = false;

	assert(wl_list_empty(&pending_state->output_list));

	drm_pending_state_free(pending_state);

	return 0;
}

void
drm_output_update_msc(struct drm_output *output, unsigned int seq)
{
	uint64_t msc_hi = output->base.msc >> 32;

	if (seq < (output->base.msc & 0xffffffff))
		msc_hi++;

	output->base.msc = (msc_hi << 32) + seq;
}

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct drm_output *output = data;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_VSYNC |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;

	drm_output_update_msc(output, frame);

	assert(!b->atomic_modeset);
	assert(output->page_flip_pending);
	output->page_flip_pending = 0;

	drm_output_update_complete(output, flags, sec, usec);
}

#ifdef HAVE_DRM_ATOMIC
static void
atomic_flip_handler(int fd, unsigned int frame, unsigned int sec,
		    unsigned int usec, unsigned int crtc_id, void *data)
{
	struct drm_backend *b = data;
	struct drm_output *output = drm_output_find_by_crtc(b, crtc_id);
	uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_VSYNC |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;

	/* During the initial modeset, we can disable CRTCs which we don't
	 * actually handle during normal operation; this will give us events
	 * for unknown outputs. Ignore them. */
	if (!output || !output->base.enabled)
		return;

	drm_output_update_msc(output, frame);

	drm_debug(b, "[atomic][CRTC:%u] flip processing started\n", crtc_id);
	assert(b->atomic_modeset);
	assert(output->atomic_complete_pending);
	output->atomic_complete_pending = 0;

	drm_output_update_complete(output, flags, sec, usec);
	drm_debug(b, "[atomic][CRTC:%u] flip processing completed\n", crtc_id);
}
#endif

int
on_drm_input(int fd, uint32_t mask, void *data)
{
#ifdef HAVE_DRM_ATOMIC
	struct drm_backend *b = data;
#endif
	drmEventContext evctx;

	memset(&evctx, 0, sizeof evctx);
#ifndef HAVE_DRM_ATOMIC
	evctx.version = 2;
#else
	evctx.version = 3;
	if (b->atomic_modeset)
		evctx.page_flip_handler2 = atomic_flip_handler;
	else
#endif
		evctx.page_flip_handler = page_flip_handler;
	drmHandleEvent(fd, &evctx);

	return 1;
}

int
init_kms_caps(struct drm_backend *b)
{
	uint64_t cap;
	int ret;
	clockid_t clk_id;

	weston_log("using %s\n", b->drm.filename);

	ret = drmGetCap(b->drm.fd, DRM_CAP_TIMESTAMP_MONOTONIC, &cap);
	if (ret == 0 && cap == 1)
		clk_id = CLOCK_MONOTONIC;
	else
		clk_id = CLOCK_REALTIME;

	if (weston_compositor_set_presentation_clock(b->compositor, clk_id) < 0) {
		weston_log("Error: failed to set presentation clock %d.\n",
			   clk_id);
		return -1;
	}

	ret = drmGetCap(b->drm.fd, DRM_CAP_CURSOR_WIDTH, &cap);
	if (ret == 0)
		b->cursor_width = cap;
	else
		b->cursor_width = 64;

	ret = drmGetCap(b->drm.fd, DRM_CAP_CURSOR_HEIGHT, &cap);
	if (ret == 0)
		b->cursor_height = cap;
	else
		b->cursor_height = 64;

	if (!getenv("WESTON_DISABLE_UNIVERSAL_PLANES")) {
		ret = drmSetClientCap(b->drm.fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
		b->universal_planes = (ret == 0);
	}
	weston_log("DRM: %s universal planes\n",
		   b->universal_planes ? "supports" : "does not support");

#ifdef HAVE_DRM_ATOMIC
	if (b->universal_planes && !getenv("WESTON_DISABLE_ATOMIC")) {
		ret = drmGetCap(b->drm.fd, DRM_CAP_CRTC_IN_VBLANK_EVENT, &cap);
		if (ret != 0)
			cap = 0;
		ret = drmSetClientCap(b->drm.fd, DRM_CLIENT_CAP_ATOMIC, 1);
		b->atomic_modeset = ((ret == 0) && (cap == 1));
	}
#endif
	weston_log("DRM: %s atomic modesetting\n",
		   b->atomic_modeset ? "supports" : "does not support");

#ifdef HAVE_DRM_ADDFB2_MODIFIERS
	ret = drmGetCap(b->drm.fd, DRM_CAP_ADDFB2_MODIFIERS, &cap);
	if (ret == 0)
		b->fb_modifiers = cap;
	else
#endif
		b->fb_modifiers = 0;

	/*
	 * KMS support for hardware planes cannot properly synchronize
	 * without nuclear page flip. Without nuclear/atomic, hw plane
	 * and cursor plane updates would either tear or cause extra
	 * waits for vblanks which means dropping the compositor framerate
	 * to a fraction. For cursors, it's not so bad, so they are
	 * enabled.
	 */
	if (!b->atomic_modeset || getenv("WESTON_FORCE_RENDERER"))
		b->sprites_are_broken = 1;

	ret = drmSetClientCap(b->drm.fd, DRM_CLIENT_CAP_ASPECT_RATIO, 1);
	b->aspect_ratio_supported = (ret == 0);
	weston_log("DRM: %s picture aspect ratio\n",
		   b->aspect_ratio_supported ? "supports" : "does not support");

	return 0;
}
