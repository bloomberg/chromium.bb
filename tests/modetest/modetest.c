/*
 * DRM based mode setting test program
 * Copyright 2008 Tungsten Graphics
 *   Jakob Bornecrantz <jakob@tungstengraphics.com>
 * Copyright 2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * This fairly simple test program dumps output in a similar format to the
 * "xrandr" tool everyone knows & loves.  It's necessarily slightly different
 * since the kernel separates outputs into encoder and connector structures,
 * each with their own unique ID.  The program also allows test testing of the
 * memory management and mode setting APIs by allowing the user to specify a
 * connector and mode to use for mode setting.  If all works as expected, a
 * blue background should be painted on the monitor attached to the specified
 * connector after the selected mode is set.
 *
 * TODO: use cairo to write the mode info on the selected output once
 *       the mode has been programmed, along with possible test patterns.
 */
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/time.h>

#include "xf86drm.h"
#include "xf86drmMode.h"
#include "drm_fourcc.h"
#include "libkms.h"

#include "buffers.h"

struct crtc {
	drmModeCrtc *crtc;
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;
};

struct encoder {
	drmModeEncoder *encoder;
};

struct connector {
	drmModeConnector *connector;
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;
};

struct fb {
	drmModeFB *fb;
};

struct plane {
	drmModePlane *plane;
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;
};

struct resources {
	drmModeRes *res;
	drmModePlaneRes *plane_res;

	struct crtc *crtcs;
	struct encoder *encoders;
	struct connector *connectors;
	struct fb *fbs;
	struct plane *planes;
};

struct resources *resources;
int fd, modes;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct type_name {
	int type;
	const char *name;
};

#define type_name_fn(res) \
const char * res##_str(int type) {			\
	unsigned int i;					\
	for (i = 0; i < ARRAY_SIZE(res##_names); i++) { \
		if (res##_names[i].type == type)	\
			return res##_names[i].name;	\
	}						\
	return "(invalid)";				\
}

struct type_name encoder_type_names[] = {
	{ DRM_MODE_ENCODER_NONE, "none" },
	{ DRM_MODE_ENCODER_DAC, "DAC" },
	{ DRM_MODE_ENCODER_TMDS, "TMDS" },
	{ DRM_MODE_ENCODER_LVDS, "LVDS" },
	{ DRM_MODE_ENCODER_TVDAC, "TVDAC" },
};

static type_name_fn(encoder_type)

struct type_name connector_status_names[] = {
	{ DRM_MODE_CONNECTED, "connected" },
	{ DRM_MODE_DISCONNECTED, "disconnected" },
	{ DRM_MODE_UNKNOWNCONNECTION, "unknown" },
};

static type_name_fn(connector_status)

struct type_name connector_type_names[] = {
	{ DRM_MODE_CONNECTOR_Unknown, "unknown" },
	{ DRM_MODE_CONNECTOR_VGA, "VGA" },
	{ DRM_MODE_CONNECTOR_DVII, "DVI-I" },
	{ DRM_MODE_CONNECTOR_DVID, "DVI-D" },
	{ DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
	{ DRM_MODE_CONNECTOR_Composite, "composite" },
	{ DRM_MODE_CONNECTOR_SVIDEO, "s-video" },
	{ DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ DRM_MODE_CONNECTOR_Component, "component" },
	{ DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN" },
	{ DRM_MODE_CONNECTOR_DisplayPort, "DP" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "eDP" },
};

static type_name_fn(connector_type)

#define bit_name_fn(res)					\
const char * res##_str(int type) {				\
	unsigned int i;						\
	const char *sep = "";					\
	for (i = 0; i < ARRAY_SIZE(res##_names); i++) {		\
		if (type & (1 << i)) {				\
			printf("%s%s", sep, res##_names[i]);	\
			sep = ", ";				\
		}						\
	}							\
	return NULL;						\
}

static const char *mode_type_names[] = {
	"builtin",
	"clock_c",
	"crtc_c",
	"preferred",
	"default",
	"userdef",
	"driver",
};

static bit_name_fn(mode_type)

static const char *mode_flag_names[] = {
	"phsync",
	"nhsync",
	"pvsync",
	"nvsync",
	"interlace",
	"dblscan",
	"csync",
	"pcsync",
	"ncsync",
	"hskew",
	"bcast",
	"pixmux",
	"dblclk",
	"clkdiv2"
};

static bit_name_fn(mode_flag)

static void dump_encoders(void)
{
	drmModeEncoder *encoder;
	int i;

	printf("Encoders:\n");
	printf("id\tcrtc\ttype\tpossible crtcs\tpossible clones\t\n");
	for (i = 0; i < resources->res->count_encoders; i++) {
		encoder = resources->encoders[i].encoder;
		if (!encoder)
			continue;

		printf("%d\t%d\t%s\t0x%08x\t0x%08x\n",
		       encoder->encoder_id,
		       encoder->crtc_id,
		       encoder_type_str(encoder->encoder_type),
		       encoder->possible_crtcs,
		       encoder->possible_clones);
	}
	printf("\n");
}

static void dump_mode(drmModeModeInfo *mode)
{
	printf("  %s %d %d %d %d %d %d %d %d %d",
	       mode->name,
	       mode->vrefresh,
	       mode->hdisplay,
	       mode->hsync_start,
	       mode->hsync_end,
	       mode->htotal,
	       mode->vdisplay,
	       mode->vsync_start,
	       mode->vsync_end,
	       mode->vtotal);

	printf(" flags: ");
	mode_flag_str(mode->flags);
	printf("; type: ");
	mode_type_str(mode->type);
	printf("\n");
}

static void
dump_blob(uint32_t blob_id)
{
	uint32_t i;
	unsigned char *blob_data;
	drmModePropertyBlobPtr blob;

	blob = drmModeGetPropertyBlob(fd, blob_id);
	if (!blob)
		return;

	blob_data = blob->data;

	for (i = 0; i < blob->length; i++) {
		if (i % 16 == 0)
			printf("\n\t\t\t");
		printf("%.2hhx", blob_data[i]);
	}
	printf("\n");

	drmModeFreePropertyBlob(blob);
}

static void
dump_prop(drmModePropertyPtr prop, uint32_t prop_id, uint64_t value)
{
	int i;
	printf("\t%d", prop_id);
	if (!prop) {
		printf("\n");
		return;
	}

	printf(" %s:\n", prop->name);

	printf("\t\tflags:");
	if (prop->flags & DRM_MODE_PROP_PENDING)
		printf(" pending");
	if (prop->flags & DRM_MODE_PROP_RANGE)
		printf(" range");
	if (prop->flags & DRM_MODE_PROP_IMMUTABLE)
		printf(" immutable");
	if (prop->flags & DRM_MODE_PROP_ENUM)
		printf(" enum");
	if (prop->flags & DRM_MODE_PROP_BITMASK)
		printf(" bitmask");
	if (prop->flags & DRM_MODE_PROP_BLOB)
		printf(" blob");
	printf("\n");

	if (prop->flags & DRM_MODE_PROP_RANGE) {
		printf("\t\tvalues:");
		for (i = 0; i < prop->count_values; i++)
			printf(" %"PRIu64, prop->values[i]);
		printf("\n");
	}

	if (prop->flags & DRM_MODE_PROP_ENUM) {
		printf("\t\tenums:");
		for (i = 0; i < prop->count_enums; i++)
			printf(" %s=%llu", prop->enums[i].name,
			       prop->enums[i].value);
		printf("\n");
	} else if (prop->flags & DRM_MODE_PROP_BITMASK) {
		printf("\t\tvalues:");
		for (i = 0; i < prop->count_enums; i++)
			printf(" %s=0x%llx", prop->enums[i].name,
			       (1LL << prop->enums[i].value));
		printf("\n");
	} else {
		assert(prop->count_enums == 0);
	}

	if (prop->flags & DRM_MODE_PROP_BLOB) {
		printf("\t\tblobs:\n");
		for (i = 0; i < prop->count_blobs; i++)
			dump_blob(prop->blob_ids[i]);
		printf("\n");
	} else {
		assert(prop->count_blobs == 0);
	}

	printf("\t\tvalue:");
	if (prop->flags & DRM_MODE_PROP_BLOB)
		dump_blob(value);
	else
		printf(" %"PRIu64"\n", value);
}

static void dump_connectors(void)
{
	int i, j;

	printf("Connectors:\n");
	printf("id\tencoder\tstatus\t\ttype\tsize (mm)\tmodes\tencoders\n");
	for (i = 0; i < resources->res->count_connectors; i++) {
		struct connector *_connector = &resources->connectors[i];
		drmModeConnector *connector = _connector->connector;
		if (!connector)
			continue;

		printf("%d\t%d\t%s\t%s\t%dx%d\t\t%d\t",
		       connector->connector_id,
		       connector->encoder_id,
		       connector_status_str(connector->connection),
		       connector_type_str(connector->connector_type),
		       connector->mmWidth, connector->mmHeight,
		       connector->count_modes);

		for (j = 0; j < connector->count_encoders; j++)
			printf("%s%d", j > 0 ? ", " : "", connector->encoders[j]);
		printf("\n");

		if (connector->count_modes) {
			printf("  modes:\n");
			printf("\tname refresh (Hz) hdisp hss hse htot vdisp "
			       "vss vse vtot)\n");
			for (j = 0; j < connector->count_modes; j++)
				dump_mode(&connector->modes[j]);
		}

		if (_connector->props) {
			printf("  props:\n");
			for (j = 0; j < (int)_connector->props->count_props; j++)
				dump_prop(_connector->props_info[j],
					  _connector->props->props[j],
					  _connector->props->prop_values[j]);
		}
	}
	printf("\n");
}

static void dump_crtcs(void)
{
	int i;
	uint32_t j;

	printf("CRTCs:\n");
	printf("id\tfb\tpos\tsize\n");
	for (i = 0; i < resources->res->count_crtcs; i++) {
		struct crtc *_crtc = &resources->crtcs[i];
		drmModeCrtc *crtc = _crtc->crtc;
		if (!crtc)
			continue;

		printf("%d\t%d\t(%d,%d)\t(%dx%d)\n",
		       crtc->crtc_id,
		       crtc->buffer_id,
		       crtc->x, crtc->y,
		       crtc->width, crtc->height);
		dump_mode(&crtc->mode);

		if (_crtc->props) {
			printf("  props:\n");
			for (j = 0; j < _crtc->props->count_props; j++)
				dump_prop(_crtc->props_info[j],
					  _crtc->props->props[j],
					  _crtc->props->prop_values[j]);
		} else {
			printf("  no properties found\n");
		}
	}
	printf("\n");
}

static void dump_framebuffers(void)
{
	drmModeFB *fb;
	int i;

	printf("Frame buffers:\n");
	printf("id\tsize\tpitch\n");
	for (i = 0; i < resources->res->count_fbs; i++) {
		fb = resources->fbs[i].fb;
		if (!fb)
			continue;

		printf("%u\t(%ux%u)\t%u\n",
		       fb->fb_id,
		       fb->width, fb->height,
		       fb->pitch);
	}
	printf("\n");
}

static void dump_planes(void)
{
	unsigned int i, j;

	printf("Planes:\n");
	printf("id\tcrtc\tfb\tCRTC x,y\tx,y\tgamma size\tpossible crtcs\n");

	if (!resources->plane_res)
		return;

	for (i = 0; i < resources->plane_res->count_planes; i++) {
		struct plane *plane = &resources->planes[i];
		drmModePlane *ovr = plane->plane;
		if (!ovr)
			continue;

		printf("%d\t%d\t%d\t%d,%d\t\t%d,%d\t%-8d\t0x%08x\n",
		       ovr->plane_id, ovr->crtc_id, ovr->fb_id,
		       ovr->crtc_x, ovr->crtc_y, ovr->x, ovr->y,
		       ovr->gamma_size, ovr->possible_crtcs);

		if (!ovr->count_formats)
			continue;

		printf("  formats:");
		for (j = 0; j < ovr->count_formats; j++)
			printf(" %4.4s", (char *)&ovr->formats[j]);
		printf("\n");

		if (plane->props) {
			printf("  props:\n");
			for (j = 0; j < plane->props->count_props; j++)
				dump_prop(plane->props_info[j],
					  plane->props->props[j],
					  plane->props->prop_values[j]);
		} else {
			printf("  no properties found\n");
		}
	}
	printf("\n");

	return;
}

static void free_resources(struct resources *res)
{
	if (!res)
		return;

#define free_resource(_res, __res, type, Type)					\
	do {									\
		int i;								\
		if (!(_res)->type##s)						\
			break;							\
		for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) {	\
			if (!(_res)->type##s[i].type)				\
				break;						\
			drmModeFree##Type((_res)->type##s[i].type);		\
		}								\
		free((_res)->type##s);						\
	} while (0)

#define free_properties(_res, __res, type)					\
	do {									\
		int i;								\
		for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) {	\
			drmModeFreeObjectProperties(res->type##s[i].props);	\
			free(res->type##s[i].props_info);			\
		}								\
	} while (0)

	if (res->res) {
		free_properties(res, res, crtc);

		free_resource(res, res, crtc, Crtc);
		free_resource(res, res, encoder, Encoder);
		free_resource(res, res, connector, Connector);
		free_resource(res, res, fb, FB);

		drmModeFreeResources(res->res);
	}

	if (res->plane_res) {
		free_properties(res, plane_res, plane);

		free_resource(res, plane_res, plane, Plane);

		drmModeFreePlaneResources(res->plane_res);
	}

	free(res);
}

static struct resources *get_resources(int fd)
{
	struct resources *res;

	res = malloc(sizeof *res);
	if (res == 0)
		return NULL;

	memset(res, 0, sizeof *res);

	res->res = drmModeGetResources(fd);
	if (!res->res) {
		fprintf(stderr, "drmModeGetResources failed: %s\n",
			strerror(errno));
		goto error;
	}

	res->crtcs = malloc(res->res->count_crtcs * sizeof *res->crtcs);
	res->encoders = malloc(res->res->count_encoders * sizeof *res->encoders);
	res->connectors = malloc(res->res->count_connectors * sizeof *res->connectors);
	res->fbs = malloc(res->res->count_fbs * sizeof *res->fbs);

	if (!res->crtcs || !res->encoders || !res->connectors || !res->fbs)
		goto error;

	memset(res->crtcs , 0, res->res->count_crtcs * sizeof *res->crtcs);
	memset(res->encoders, 0, res->res->count_encoders * sizeof *res->encoders);
	memset(res->connectors, 0, res->res->count_connectors * sizeof *res->connectors);
	memset(res->fbs, 0, res->res->count_fbs * sizeof *res->fbs);

#define get_resource(_res, __res, type, Type)					\
	do {									\
		int i;								\
		for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) {	\
			(_res)->type##s[i].type =				\
				drmModeGet##Type(fd, (_res)->__res->type##s[i]); \
			if (!(_res)->type##s[i].type)				\
				fprintf(stderr, "could not get %s %i: %s\n",	\
					#type, (_res)->__res->type##s[i],	\
					strerror(errno));			\
		}								\
	} while (0)

	get_resource(res, res, crtc, Crtc);
	get_resource(res, res, encoder, Encoder);
	get_resource(res, res, connector, Connector);
	get_resource(res, res, fb, FB);

#define get_properties(_res, __res, type, Type)					\
	do {									\
		int i;								\
		for (i = 0; i < (int)(_res)->__res->count_##type##s; ++i) {	\
			struct type *obj = &res->type##s[i];			\
			unsigned int j;						\
			obj->props =						\
				drmModeObjectGetProperties(fd, obj->type->type##_id, \
							   DRM_MODE_OBJECT_##Type); \
			if (!obj->props) {					\
				fprintf(stderr,					\
					"could not get %s %i properties: %s\n", \
					#type, obj->type->type##_id,		\
					strerror(errno));			\
				continue;					\
			}							\
			obj->props_info = malloc(obj->props->count_props *	\
						 sizeof *obj->props_info);	\
			if (!obj->props_info)					\
				continue;					\
			for (j = 0; j < obj->props->count_props; ++j)		\
				obj->props_info[j] =				\
					drmModeGetProperty(fd, obj->props->props[j]); \
		}								\
	} while (0)

	get_properties(res, res, crtc, CRTC);
	get_properties(res, res, connector, CONNECTOR);

	res->plane_res = drmModeGetPlaneResources(fd);
	if (!res->plane_res) {
		fprintf(stderr, "drmModeGetPlaneResources failed: %s\n",
			strerror(errno));
		return res;
	}

	res->planes = malloc(res->plane_res->count_planes * sizeof *res->planes);
	if (!res->planes)
		goto error;

	memset(res->planes, 0, res->plane_res->count_planes * sizeof *res->planes);

	get_resource(res, plane_res, plane, Plane);
	get_properties(res, plane_res, plane, PLANE);

	return res;

error:
	free_resources(res);
	return NULL;
}

/* -----------------------------------------------------------------------------
 * Connectors and planes
 */

/*
 * Mode setting with the kernel interfaces is a bit of a chore.
 * First you have to find the connector in question and make sure the
 * requested mode is available.
 * Then you need to find the encoder attached to that connector so you
 * can bind it with a free crtc.
 */
struct connector_arg {
	uint32_t id;
	char mode_str[64];
	char format_str[5];
	unsigned int fourcc;
	drmModeModeInfo *mode;
	drmModeEncoder *encoder;
	int crtc;
	int pipe;
	unsigned int fb_id[2], current_fb_id;
	struct timeval start;

	int swap_count;
};

struct plane_arg {
	uint32_t con_id;  /* the id of connector to bind to */
	uint32_t w, h;
	unsigned int fb_id;
	char format_str[5]; /* need to leave room for terminating \0 */
	unsigned int fourcc;
};

static void
connector_find_mode(struct connector_arg *c)
{
	drmModeConnector *connector;
	int i, j;

	/* First, find the connector & mode */
	c->mode = NULL;
	for (i = 0; i < resources->res->count_connectors; i++) {
		connector = resources->connectors[i].connector;
		if (!connector)
			continue;

		if (!connector->count_modes)
			continue;

		if (connector->connector_id != c->id)
			continue;

		for (j = 0; j < connector->count_modes; j++) {
			c->mode = &connector->modes[j];
			if (!strcmp(c->mode->name, c->mode_str))
				break;
		}

		/* Found it, break out */
		if (c->mode)
			break;
	}

	if (!c->mode) {
		fprintf(stderr, "failed to find mode \"%s\"\n", c->mode_str);
		return;
	}

	/* Now get the encoder */
	for (i = 0; i < resources->res->count_encoders; i++) {
		c->encoder = resources->encoders[i].encoder;
		if (!c->encoder)
			continue;

		if (c->encoder->encoder_id  == connector->encoder_id)
			break;
	}

	if (c->crtc == -1)
		c->crtc = c->encoder->crtc_id;

	/* and figure out which crtc index it is: */
	for (i = 0; i < resources->res->count_crtcs; i++) {
		if (c->crtc == (int)resources->res->crtcs[i]) {
			c->pipe = i;
			break;
		}
	}

}

/* -------------------------------------------------------------------------- */

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct connector_arg *c;
	unsigned int new_fb_id;
	struct timeval end;
	double t;

	c = data;
	if (c->current_fb_id == c->fb_id[0])
		new_fb_id = c->fb_id[1];
	else
		new_fb_id = c->fb_id[0];

	drmModePageFlip(fd, c->crtc, new_fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, c);
	c->current_fb_id = new_fb_id;
	c->swap_count++;
	if (c->swap_count == 60) {
		gettimeofday(&end, NULL);
		t = end.tv_sec + end.tv_usec * 1e-6 -
			(c->start.tv_sec + c->start.tv_usec * 1e-6);
		fprintf(stderr, "freq: %.02fHz\n", c->swap_count / t);
		c->swap_count = 0;
		c->start = end;
	}
}

static int
set_plane(struct kms_driver *kms, struct connector_arg *c, struct plane_arg *p)
{
	drmModePlane *ovr;
	uint32_t handles[4], pitches[4], offsets[4] = {0}; /* we only use [0] */
	uint32_t plane_id = 0;
	struct kms_bo *plane_bo;
	uint32_t plane_flags = 0;
	int crtc_x, crtc_y, crtc_w, crtc_h;
	unsigned int i;

	/* find an unused plane which can be connected to our crtc */
	for (i = 0; i < resources->plane_res->count_planes && !plane_id; i++) {
		ovr = resources->planes[i].plane;
		if (!ovr)
			continue;

		if ((ovr->possible_crtcs & (1 << c->pipe)) && !ovr->crtc_id)
			plane_id = ovr->plane_id;
	}

	fprintf(stderr, "testing %dx%d@%s overlay plane\n",
			p->w, p->h, p->format_str);

	if (!plane_id) {
		fprintf(stderr, "failed to find plane!\n");
		return -1;
	}

	plane_bo = create_test_buffer(kms, p->fourcc, p->w, p->h, handles,
				      pitches, offsets, PATTERN_TILES);
	if (plane_bo == NULL)
		return -1;

	/* just use single plane format for now.. */
	if (drmModeAddFB2(fd, p->w, p->h, p->fourcc,
			handles, pitches, offsets, &p->fb_id, plane_flags)) {
		fprintf(stderr, "failed to add fb: %s\n", strerror(errno));
		return -1;
	}

	/* ok, boring.. but for now put in middle of screen: */
	crtc_x = c->mode->hdisplay / 3;
	crtc_y = c->mode->vdisplay / 3;
	crtc_w = crtc_x;
	crtc_h = crtc_y;

	/* note src coords (last 4 args) are in Q16 format */
	if (drmModeSetPlane(fd, plane_id, c->crtc, p->fb_id,
			    plane_flags, crtc_x, crtc_y, crtc_w, crtc_h,
			    0, 0, p->w << 16, p->h << 16)) {
		fprintf(stderr, "failed to enable plane: %s\n",
			strerror(errno));
		return -1;
	}

	ovr->crtc_id = c->crtc;

	return 0;
}

static void
set_mode(struct connector_arg *c, int count, struct plane_arg *p, int plane_count,
		int page_flip)
{
	struct kms_driver *kms;
	struct kms_bo *bo, *other_bo;
	unsigned int fb_id, other_fb_id;
	int i, j, ret, width, height, x;
	uint32_t handles[4], pitches[4], offsets[4] = {0}; /* we only use [0] */
	drmEventContext evctx;

	width = 0;
	height = 0;
	for (i = 0; i < count; i++) {
		connector_find_mode(&c[i]);
		if (c[i].mode == NULL)
			continue;
		width += c[i].mode->hdisplay;
		if (height < c[i].mode->vdisplay)
			height = c[i].mode->vdisplay;
	}

	ret = kms_create(fd, &kms);
	if (ret) {
		fprintf(stderr, "failed to create kms driver: %s\n",
			strerror(-ret));
		return;
	}

	bo = create_test_buffer(kms, c->fourcc, width, height, handles,
				pitches, offsets, PATTERN_SMPTE);
	if (bo == NULL)
		return;

	ret = drmModeAddFB2(fd, width, height, c->fourcc,
			    handles, pitches, offsets, &fb_id, 0);
	if (ret) {
		fprintf(stderr, "failed to add fb (%ux%u): %s\n",
			width, height, strerror(errno));
		return;
	}

	x = 0;
	for (i = 0; i < count; i++) {
		if (c[i].mode == NULL)
			continue;

		printf("setting mode %s@%s on connector %d, crtc %d\n",
		       c[i].mode_str, c[i].format_str, c[i].id, c[i].crtc);

		ret = drmModeSetCrtc(fd, c[i].crtc, fb_id, x, 0,
				     &c[i].id, 1, c[i].mode);

		/* XXX: Actually check if this is needed */
		drmModeDirtyFB(fd, fb_id, NULL, 0);

		x += c[i].mode->hdisplay;

		if (ret) {
			fprintf(stderr, "failed to set mode: %s\n", strerror(errno));
			return;
		}

		/* if we have a plane/overlay to show, set that up now: */
		for (j = 0; j < plane_count; j++)
			if (p[j].con_id == c[i].id)
				if (set_plane(kms, &c[i], &p[j]))
					return;
	}

	if (!page_flip)
		return;
	
	other_bo = create_test_buffer(kms, c->fourcc, width, height, handles,
				      pitches, offsets, PATTERN_PLAIN);
	if (other_bo == NULL)
		return;

	ret = drmModeAddFB2(fd, width, height, c->fourcc, handles, pitches, offsets,
			    &other_fb_id, 0);
	if (ret) {
		fprintf(stderr, "failed to add fb: %s\n", strerror(errno));
		return;
	}

	for (i = 0; i < count; i++) {
		if (c[i].mode == NULL)
			continue;

		ret = drmModePageFlip(fd, c[i].crtc, other_fb_id,
				      DRM_MODE_PAGE_FLIP_EVENT, &c[i]);
		if (ret) {
			fprintf(stderr, "failed to page flip: %s\n", strerror(errno));
			return;
		}
		gettimeofday(&c[i].start, NULL);
		c[i].swap_count = 0;
		c[i].fb_id[0] = fb_id;
		c[i].fb_id[1] = other_fb_id;
		c[i].current_fb_id = other_fb_id;
	}

	memset(&evctx, 0, sizeof evctx);
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.vblank_handler = NULL;
	evctx.page_flip_handler = page_flip_handler;
	
	while (1) {
#if 0
		struct pollfd pfd[2];

		pfd[0].fd = 0;
		pfd[0].events = POLLIN;
		pfd[1].fd = fd;
		pfd[1].events = POLLIN;

		if (poll(pfd, 2, -1) < 0) {
			fprintf(stderr, "poll error\n");
			break;
		}

		if (pfd[0].revents)
			break;
#else
		struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
		fd_set fds;
		int ret;

		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(fd, &fds);
		ret = select(fd + 1, &fds, NULL, NULL, &timeout);

		if (ret <= 0) {
			fprintf(stderr, "select timed out or error (ret %d)\n",
				ret);
			continue;
		} else if (FD_ISSET(0, &fds)) {
			break;
		}
#endif

		drmHandleEvent(fd, &evctx);
	}

	kms_bo_destroy(&bo);
	kms_bo_destroy(&other_bo);
	kms_destroy(&kms);
}

#define min(a, b)	((a) < (b) ? (a) : (b))

static int parse_connector(struct connector_arg *c, const char *arg)
{
	unsigned int len;
	const char *p;
	char *endp;

	c->crtc = -1;
	strcpy(c->format_str, "XR24");

	c->id = strtoul(arg, &endp, 10);
	if (*endp == '@') {
		arg = endp + 1;
		c->crtc = strtoul(arg, &endp, 10);
	}
	if (*endp != ':')
		return -1;

	arg = endp + 1;

	p = strchrnul(arg, '@');
	len = min(sizeof c->mode_str - 1, (unsigned int)(p - arg));
	strncpy(c->mode_str, arg, len);
	c->mode_str[len] = '\0';

	if (*p == '@') {
		strncpy(c->format_str, p + 1, 4);
		c->format_str[4] = '\0';
	}

	c->fourcc = format_fourcc(c->format_str);
	if (c->fourcc == 0)  {
		fprintf(stderr, "unknown format %s\n", c->format_str);
		return -1;
	}

	return 0;
}

static int parse_plane(struct plane_arg *p, const char *arg)
{
	strcpy(p->format_str, "XR24");

	if (sscanf(arg, "%d:%dx%d@%4s", &p->con_id, &p->w, &p->h, p->format_str) != 4 &&
	    sscanf(arg, "%d:%dx%d", &p->con_id, &p->w, &p->h) != 3)
		return -1;

	p->fourcc = format_fourcc(p->format_str);
	if (p->fourcc == 0) {
		fprintf(stderr, "unknown format %s\n", p->format_str);
		return -1;
	}

	return 0;
}

static void usage(char *name)
{
	fprintf(stderr, "usage: %s [-cdefMmPpsv]\n", name);

	fprintf(stderr, "\n Query options:\n\n");
	fprintf(stderr, "\t-c\tlist connectors\n");
	fprintf(stderr, "\t-e\tlist encoders\n");
	fprintf(stderr, "\t-f\tlist framebuffers\n");
	fprintf(stderr, "\t-m\tlist modes\n");
	fprintf(stderr, "\t-p\tlist CRTCs and planes (pipes)\n");

	fprintf(stderr, "\n Test options:\n\n");
	fprintf(stderr, "\t-P <connector_id>:<w>x<h>[@<format>]\tset a plane\n");
	fprintf(stderr, "\t-s <connector_id>[@<crtc_id>]:<mode>[@<format>]\tset a mode\n");
	fprintf(stderr, "\t-v\ttest vsynced page flipping\n");

	fprintf(stderr, "\n Generic options:\n\n");
	fprintf(stderr, "\t-d\tdrop master after mode set\n");
	fprintf(stderr, "\t-M module\tuse the given driver\n");

	fprintf(stderr, "\n\tDefault is to dump all info.\n");
	exit(0);
}

#define dump_resource(res) if (res) dump_##res()

static int page_flipping_supported(void)
{
	/*FIXME: generic ioctl needed? */
	return 1;
#if 0
	int ret, value;
	struct drm_i915_getparam gp;

	gp.param = I915_PARAM_HAS_PAGEFLIPPING;
	gp.value = &value;

	ret = drmCommandWriteRead(fd, DRM_I915_GETPARAM, &gp, sizeof(gp));
	if (ret) {
		fprintf(stderr, "drm_i915_getparam: %m\n");
		return 0;
	}

	return *gp.value;
#endif
}

static char optstr[] = "cdefM:mP:ps:v";

int main(int argc, char **argv)
{
	int c;
	int encoders = 0, connectors = 0, crtcs = 0, planes = 0, framebuffers = 0;
	int drop_master = 0;
	int test_vsync = 0;
	const char *modules[] = { "i915", "radeon", "nouveau", "vmwgfx", "omapdrm", "exynos", "tilcdc" };
	char *module = NULL;
	unsigned int i;
	int count = 0, plane_count = 0;
	struct connector_arg *con_args = NULL;
	struct plane_arg *plane_args = NULL;
	unsigned int args = 0;
	
	opterr = 0;
	while ((c = getopt(argc, argv, optstr)) != -1) {
		args++;

		switch (c) {
		case 'c':
			connectors = 1;
			break;
		case 'd':
			drop_master = 1;
			break;
		case 'e':
			encoders = 1;
			break;
		case 'f':
			framebuffers = 1;
			break;
		case 'M':
			module = optarg;
			/* Preserve the default behaviour of dumping all information. */
			args--;
			break;
		case 'm':
			modes = 1;
			break;
		case 'P':
			plane_args = realloc(plane_args,
					     (plane_count + 1) * sizeof *plane_args);
			if (plane_args == NULL) {
				fprintf(stderr, "memory allocation failed\n");
				return 1;
			}

			if (parse_plane(&plane_args[plane_count], optarg) < 0)
				usage(argv[0]);

			plane_count++;
			break;
		case 'p':
			crtcs = 1;
			planes = 1;
			break;
		case 's':
			con_args = realloc(con_args,
					   (count + 1) * sizeof *con_args);
			if (con_args == NULL) {
				fprintf(stderr, "memory allocation failed\n");
				return 1;
			}

			if (parse_connector(&con_args[count], optarg) < 0)
				usage(argv[0]);

			count++;				      
			break;
		case 'v':
			test_vsync = 1;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	if (!args)
		encoders = connectors = crtcs = planes = modes = framebuffers = 1;

	if (module) {
		fd = drmOpen(module, NULL);
		if (fd < 0) {
			fprintf(stderr, "failed to open device '%s'.\n", module);
			return 1;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(modules); i++) {
			printf("trying to open device '%s'...", modules[i]);
			fd = drmOpen(modules[i], NULL);
			if (fd < 0) {
				printf("failed.\n");
			} else {
				printf("success.\n");
				break;
			}
		}

		if (fd < 0) {
			fprintf(stderr, "no device found.\n");
			return 1;
		}
	}

	if (test_vsync && !page_flipping_supported()) {
		fprintf(stderr, "page flipping not supported by drm.\n");
		return -1;
	}

	resources = get_resources(fd);
	if (!resources) {
		drmClose(fd);
		return 1;
	}

	dump_resource(encoders);
	dump_resource(connectors);
	dump_resource(crtcs);
	dump_resource(planes);
	dump_resource(framebuffers);

	if (count > 0) {
		set_mode(con_args, count, plane_args, plane_count, test_vsync);
		if (drop_master)
			drmDropMaster(fd);
		getchar();
	}

	free_resources(resources);

	return 0;
}
