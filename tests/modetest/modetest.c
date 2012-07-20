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

#ifdef HAVE_CAIRO
#include <math.h>
#include <cairo.h>
#endif

drmModeRes *resources;
int fd, modes;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct type_name {
	int type;
	char *name;
};

#define type_name_fn(res) \
char * res##_str(int type) {			\
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

type_name_fn(encoder_type)

struct type_name connector_status_names[] = {
	{ DRM_MODE_CONNECTED, "connected" },
	{ DRM_MODE_DISCONNECTED, "disconnected" },
	{ DRM_MODE_UNKNOWNCONNECTION, "unknown" },
};

type_name_fn(connector_status)

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
	{ DRM_MODE_CONNECTOR_DisplayPort, "displayport" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "embedded displayport" },
};

type_name_fn(connector_type)

#define bit_name_fn(res)					\
char * res##_str(int type) {					\
	int i;							\
	const char *sep = "";					\
	for (i = 0; i < ARRAY_SIZE(res##_names); i++) {		\
		if (type & (1 << i)) {				\
			printf("%s%s", sep, res##_names[i]);	\
			sep = ", ";				\
		}						\
	}							\
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

bit_name_fn(mode_type)

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

bit_name_fn(mode_flag)

void dump_encoders(void)
{
	drmModeEncoder *encoder;
	int i;

	printf("Encoders:\n");
	printf("id\tcrtc\ttype\tpossible crtcs\tpossible clones\t\n");
	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(fd, resources->encoders[i]);

		if (!encoder) {
			fprintf(stderr, "could not get encoder %i: %s\n",
				resources->encoders[i], strerror(errno));
			continue;
		}
		printf("%d\t%d\t%s\t0x%08x\t0x%08x\n",
		       encoder->encoder_id,
		       encoder->crtc_id,
		       encoder_type_str(encoder->encoder_type),
		       encoder->possible_crtcs,
		       encoder->possible_clones);
		drmModeFreeEncoder(encoder);
	}
	printf("\n");
}

void dump_mode(drmModeModeInfo *mode)
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
dump_prop(uint32_t prop_id, uint64_t value)
{
	int i;
	drmModePropertyPtr prop;

	prop = drmModeGetProperty(fd, prop_id);

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

	drmModeFreeProperty(prop);
}

void dump_connectors(void)
{
	drmModeConnector *connector;
	int i, j;

	printf("Connectors:\n");
	printf("id\tencoder\tstatus\t\ttype\tsize (mm)\tmodes\tencoders\n");
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(fd, resources->connectors[i]);

		if (!connector) {
			fprintf(stderr, "could not get connector %i: %s\n",
				resources->connectors[i], strerror(errno));
			continue;
		}

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

			printf("  props:\n");
			for (j = 0; j < connector->count_props; j++)
				dump_prop(connector->props[j],
					  connector->prop_values[j]);
		}

		drmModeFreeConnector(connector);
	}
	printf("\n");
}

void dump_crtcs(void)
{
	drmModeCrtc *crtc;
	drmModeObjectPropertiesPtr props;
	int i;
	uint32_t j;

	printf("CRTCs:\n");
	printf("id\tfb\tpos\tsize\n");
	for (i = 0; i < resources->count_crtcs; i++) {
		crtc = drmModeGetCrtc(fd, resources->crtcs[i]);

		if (!crtc) {
			fprintf(stderr, "could not get crtc %i: %s\n",
				resources->crtcs[i], strerror(errno));
			continue;
		}
		printf("%d\t%d\t(%d,%d)\t(%dx%d)\n",
		       crtc->crtc_id,
		       crtc->buffer_id,
		       crtc->x, crtc->y,
		       crtc->width, crtc->height);
		dump_mode(&crtc->mode);

		printf("  props:\n");
		props = drmModeObjectGetProperties(fd, crtc->crtc_id,
						   DRM_MODE_OBJECT_CRTC);
		if (props) {
			for (j = 0; j < props->count_props; j++)
				dump_prop(props->props[j],
					  props->prop_values[j]);
			drmModeFreeObjectProperties(props);
		} else {
			printf("\tcould not get crtc properties: %s\n",
			       strerror(errno));
		}

		drmModeFreeCrtc(crtc);
	}
	printf("\n");
}

void dump_framebuffers(void)
{
	drmModeFB *fb;
	int i;

	printf("Frame buffers:\n");
	printf("id\tsize\tpitch\n");
	for (i = 0; i < resources->count_fbs; i++) {
		fb = drmModeGetFB(fd, resources->fbs[i]);

		if (!fb) {
			fprintf(stderr, "could not get fb %i: %s\n",
				resources->fbs[i], strerror(errno));
			continue;
		}
		printf("%u\t(%ux%u)\t%u\n",
		       fb->fb_id,
		       fb->width, fb->height,
		       fb->pitch);

		drmModeFreeFB(fb);
	}
	printf("\n");
}

static void dump_planes(void)
{
	drmModeObjectPropertiesPtr props;
	drmModePlaneRes *plane_resources;
	drmModePlane *ovr;
	unsigned int i, j;

	plane_resources = drmModeGetPlaneResources(fd);
	if (!plane_resources) {
		fprintf(stderr, "drmModeGetPlaneResources failed: %s\n",
			strerror(errno));
		return;
	}

	printf("Planes:\n");
	printf("id\tcrtc\tfb\tCRTC x,y\tx,y\tgamma size\n");
	for (i = 0; i < plane_resources->count_planes; i++) {
		ovr = drmModeGetPlane(fd, plane_resources->planes[i]);
		if (!ovr) {
			fprintf(stderr, "drmModeGetPlane failed: %s\n",
				strerror(errno));
			continue;
		}

		printf("%d\t%d\t%d\t%d,%d\t\t%d,%d\t%d\n",
		       ovr->plane_id, ovr->crtc_id, ovr->fb_id,
		       ovr->crtc_x, ovr->crtc_y, ovr->x, ovr->y,
		       ovr->gamma_size);

		if (!ovr->count_formats)
			continue;

		printf("  formats:");
		for (j = 0; j < ovr->count_formats; j++)
			printf(" %4.4s", (char *)&ovr->formats[j]);
		printf("\n");

		printf("  props:\n");
		props = drmModeObjectGetProperties(fd, ovr->plane_id,
						   DRM_MODE_OBJECT_PLANE);
		if (props) {
			for (j = 0; j < props->count_props; j++)
				dump_prop(props->props[j],
					  props->prop_values[j]);
			drmModeFreeObjectProperties(props);
		} else {
			printf("\tcould not get plane properties: %s\n",
			       strerror(errno));
		}

		drmModeFreePlane(ovr);
	}
	printf("\n");

	drmModeFreePlaneResources(plane_resources);
	return;
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
struct connector {
	uint32_t id;
	char mode_str[64];
	drmModeModeInfo *mode;
	drmModeEncoder *encoder;
	int crtc;
	int pipe;
	unsigned int fb_id[2], current_fb_id;
	struct timeval start;

	int swap_count;
};

struct plane {
	uint32_t con_id;  /* the id of connector to bind to */
	uint32_t w, h;
	unsigned int fb_id;
	char format_str[5]; /* need to leave room for terminating \0 */
};

static void
connector_find_mode(struct connector *c)
{
	drmModeConnector *connector;
	int i, j;

	/* First, find the connector & mode */
	c->mode = NULL;
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(fd, resources->connectors[i]);

		if (!connector) {
			fprintf(stderr, "could not get connector %i: %s\n",
				resources->connectors[i], strerror(errno));
			drmModeFreeConnector(connector);
			continue;
		}

		if (!connector->count_modes) {
			drmModeFreeConnector(connector);
			continue;
		}

		if (connector->connector_id != c->id) {
			drmModeFreeConnector(connector);
			continue;
		}

		for (j = 0; j < connector->count_modes; j++) {
			c->mode = &connector->modes[j];
			if (!strcmp(c->mode->name, c->mode_str))
				break;
		}

		/* Found it, break out */
		if (c->mode)
			break;

		drmModeFreeConnector(connector);
	}

	if (!c->mode) {
		fprintf(stderr, "failed to find mode \"%s\"\n", c->mode_str);
		return;
	}

	/* Now get the encoder */
	for (i = 0; i < resources->count_encoders; i++) {
		c->encoder = drmModeGetEncoder(fd, resources->encoders[i]);

		if (!c->encoder) {
			fprintf(stderr, "could not get encoder %i: %s\n",
				resources->encoders[i], strerror(errno));
			drmModeFreeEncoder(c->encoder);
			continue;
		}

		if (c->encoder->encoder_id  == connector->encoder_id)
			break;

		drmModeFreeEncoder(c->encoder);
	}

	if (c->crtc == -1)
		c->crtc = c->encoder->crtc_id;

	/* and figure out which crtc index it is: */
	for (i = 0; i < resources->count_crtcs; i++) {
		if (c->crtc == resources->crtcs[i]) {
			c->pipe = i;
			break;
		}
	}

}

/* -----------------------------------------------------------------------------
 * Formats
 */

struct color_component {
	unsigned int length;
	unsigned int offset;
};

struct rgb_info {
	struct color_component red;
	struct color_component green;
	struct color_component blue;
	struct color_component alpha;
};

enum yuv_order {
	YUV_YCbCr = 1,
	YUV_YCrCb = 2,
	YUV_YC = 4,
	YUV_CY = 8,
};

struct yuv_info {
	enum yuv_order order;
	unsigned int xsub;
	unsigned int ysub;
	unsigned int chroma_stride;
};

struct format_info {
	unsigned int format;
	const char *name;
	const struct rgb_info rgb;
	const struct yuv_info yuv;
};

#define MAKE_RGB_INFO(rl, ro, bl, bo, gl, go, al, ao) \
	.rgb = { { (rl), (ro) }, { (bl), (bo) }, { (gl), (go) }, { (al), (ao) } }

#define MAKE_YUV_INFO(order, xsub, ysub, chroma_stride) \
	.yuv = { (order), (xsub), (ysub), (chroma_stride) }

static const struct format_info format_info[] = {
	/* YUV packed */
	{ DRM_FORMAT_UYVY, "UYVY", MAKE_YUV_INFO(YUV_YCbCr | YUV_CY, 2, 2, 2) },
	{ DRM_FORMAT_VYUY, "VYUY", MAKE_YUV_INFO(YUV_YCrCb | YUV_CY, 2, 2, 2) },
	{ DRM_FORMAT_YUYV, "YUYV", MAKE_YUV_INFO(YUV_YCbCr | YUV_YC, 2, 2, 2) },
	{ DRM_FORMAT_YVYU, "YVYU", MAKE_YUV_INFO(YUV_YCrCb | YUV_YC, 2, 2, 2) },
	/* YUV semi-planar */
	{ DRM_FORMAT_NV12, "NV12", MAKE_YUV_INFO(YUV_YCbCr, 2, 2, 2) },
	{ DRM_FORMAT_NV21, "NV21", MAKE_YUV_INFO(YUV_YCrCb, 2, 2, 2) },
	{ DRM_FORMAT_NV16, "NV16", MAKE_YUV_INFO(YUV_YCbCr, 2, 1, 2) },
	{ DRM_FORMAT_NV61, "NV61", MAKE_YUV_INFO(YUV_YCrCb, 2, 1, 2) },
	/* YUV planar */
	{ DRM_FORMAT_YVU420, "YV12", MAKE_YUV_INFO(YUV_YCrCb, 2, 2, 1) },
	/* RGB */
	{ DRM_FORMAT_XRGB1555, "XR15", MAKE_RGB_INFO(5, 10, 5, 5, 5, 0, 0, 0) },
	{ DRM_FORMAT_XRGB8888, "XR24", MAKE_RGB_INFO(8, 16, 8, 8, 8, 0, 0, 0) },
	{ DRM_FORMAT_ARGB1555, "AR15", MAKE_RGB_INFO(5, 10, 5, 5, 5, 0, 1, 15) },
};

unsigned int format_fourcc(const char *name)
{
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(format_info); i++) {
		if (!strcmp(format_info[i].name, name))
			return format_info[i].format;
	}
	return 0;
}

/* -----------------------------------------------------------------------------
 * Test patterns
 */

enum fill_pattern {
	PATTERN_TILES = 0,
	PATTERN_PLAIN = 1,
	PATTERN_SMPTE = 2,
};

struct color_rgb24 {
	unsigned int value:24;
} __attribute__((__packed__));

struct color_yuv {
	unsigned char y;
	unsigned char u;
	unsigned char v;
};

#define MAKE_YUV_601_Y(r, g, b) \
	((( 66 * (r) + 129 * (g) +  25 * (b) + 128) >> 8) + 16)
#define MAKE_YUV_601_U(r, g, b) \
	(((-38 * (r) -  74 * (g) + 112 * (b) + 128) >> 8) + 128)
#define MAKE_YUV_601_V(r, g, b) \
	(((112 * (r) -  94 * (g) -  18 * (b) + 128) >> 8) + 128)

#define MAKE_YUV_601(r, g, b) \
	{ .y = MAKE_YUV_601_Y(r, g, b), \
	  .u = MAKE_YUV_601_U(r, g, b), \
	  .v = MAKE_YUV_601_V(r, g, b) }

#define MAKE_RGBA(rgb, r, g, b, a) \
	((((r) >> (8 - (rgb)->red.length)) << (rgb)->red.offset) | \
	 (((g) >> (8 - (rgb)->green.length)) << (rgb)->green.offset) | \
	 (((b) >> (8 - (rgb)->blue.length)) << (rgb)->blue.offset) | \
	 (((a) >> (8 - (rgb)->alpha.length)) << (rgb)->alpha.offset))

static void
fill_smpte_yuv_planar(const struct yuv_info *yuv,
		      unsigned char *y_mem, unsigned char *u_mem,
		      unsigned char *v_mem, unsigned int width,
		      unsigned int height, unsigned int stride)
{
	const struct color_yuv colors_top[] = {
		MAKE_YUV_601(191, 192, 192),	/* grey */
		MAKE_YUV_601(192, 192, 0),	/* yellow */
		MAKE_YUV_601(0, 192, 192),	/* cyan */
		MAKE_YUV_601(0, 192, 0),	/* green */
		MAKE_YUV_601(192, 0, 192),	/* magenta */
		MAKE_YUV_601(192, 0, 0),	/* red */
		MAKE_YUV_601(0, 0, 192),	/* blue */
	};
	const struct color_yuv colors_middle[] = {
		MAKE_YUV_601(0, 0, 192),	/* blue */
		MAKE_YUV_601(19, 19, 19),	/* black */
		MAKE_YUV_601(192, 0, 192),	/* magenta */
		MAKE_YUV_601(19, 19, 19),	/* black */
		MAKE_YUV_601(0, 192, 192),	/* cyan */
		MAKE_YUV_601(19, 19, 19),	/* black */
		MAKE_YUV_601(192, 192, 192),	/* grey */
	};
	const struct color_yuv colors_bottom[] = {
		MAKE_YUV_601(0, 33, 76),	/* in-phase */
		MAKE_YUV_601(255, 255, 255),	/* super white */
		MAKE_YUV_601(50, 0, 106),	/* quadrature */
		MAKE_YUV_601(19, 19, 19),	/* black */
		MAKE_YUV_601(9, 9, 9),		/* 3.5% */
		MAKE_YUV_601(19, 19, 19),	/* 7.5% */
		MAKE_YUV_601(29, 29, 29),	/* 11.5% */
		MAKE_YUV_601(19, 19, 19),	/* black */
	};
	unsigned int cs = yuv->chroma_stride;
	unsigned int xsub = yuv->xsub;
	unsigned int ysub = yuv->ysub;
	unsigned int x;
	unsigned int y;

	/* Luma */
	for (y = 0; y < height * 6 / 9; ++y) {
		for (x = 0; x < width; ++x)
			y_mem[x] = colors_top[x * 7 / width].y;
		y_mem += stride;
	}

	for (; y < height * 7 / 9; ++y) {
		for (x = 0; x < width; ++x)
			y_mem[x] = colors_middle[x * 7 / width].y;
		y_mem += stride;
	}

	for (; y < height; ++y) {
		for (x = 0; x < width * 5 / 7; ++x)
			y_mem[x] = colors_bottom[x * 4 / (width * 5 / 7)].y;
		for (; x < width * 6 / 7; ++x)
			y_mem[x] = colors_bottom[(x - width * 5 / 7) * 3
						 / (width / 7) + 4].y;
		for (; x < width; ++x)
			y_mem[x] = colors_bottom[7].y;
		y_mem += stride;
	}

	/* Chroma */
	for (y = 0; y < height / ysub * 6 / 9; ++y) {
		for (x = 0; x < width; x += xsub) {
			u_mem[x*cs/xsub] = colors_top[x * 7 / width].u;
			v_mem[x*cs/xsub] = colors_top[x * 7 / width].v;
		}
		u_mem += stride * cs / xsub;
		v_mem += stride * cs / xsub;
	}

	for (; y < height / ysub * 7 / 9; ++y) {
		for (x = 0; x < width; x += xsub) {
			u_mem[x*cs/xsub] = colors_middle[x * 7 / width].u;
			v_mem[x*cs/xsub] = colors_middle[x * 7 / width].v;
		}
		u_mem += stride * cs / xsub;
		v_mem += stride * cs / xsub;
	}

	for (; y < height / ysub; ++y) {
		for (x = 0; x < width * 5 / 7; x += xsub) {
			u_mem[x*cs/xsub] =
				colors_bottom[x * 4 / (width * 5 / 7)].u;
			v_mem[x*cs/xsub] =
				colors_bottom[x * 4 / (width * 5 / 7)].v;
		}
		for (; x < width * 6 / 7; x += xsub) {
			u_mem[x*cs/xsub] = colors_bottom[(x - width * 5 / 7) *
							 3 / (width / 7) + 4].u;
			v_mem[x*cs/xsub] = colors_bottom[(x - width * 5 / 7) *
							 3 / (width / 7) + 4].v;
		}
		for (; x < width; x += xsub) {
			u_mem[x*cs/xsub] = colors_bottom[7].u;
			v_mem[x*cs/xsub] = colors_bottom[7].v;
		}
		u_mem += stride * cs / xsub;
		v_mem += stride * cs / xsub;
	}
}

static void
fill_smpte_yuv_packed(const struct yuv_info *yuv, unsigned char *mem,
		      unsigned int width, unsigned int height,
		      unsigned int stride)
{
	const struct color_yuv colors_top[] = {
		MAKE_YUV_601(191, 192, 192),	/* grey */
		MAKE_YUV_601(192, 192, 0),	/* yellow */
		MAKE_YUV_601(0, 192, 192),	/* cyan */
		MAKE_YUV_601(0, 192, 0),	/* green */
		MAKE_YUV_601(192, 0, 192),	/* magenta */
		MAKE_YUV_601(192, 0, 0),	/* red */
		MAKE_YUV_601(0, 0, 192),	/* blue */
	};
	const struct color_yuv colors_middle[] = {
		MAKE_YUV_601(0, 0, 192),	/* blue */
		MAKE_YUV_601(19, 19, 19),	/* black */
		MAKE_YUV_601(192, 0, 192),	/* magenta */
		MAKE_YUV_601(19, 19, 19),	/* black */
		MAKE_YUV_601(0, 192, 192),	/* cyan */
		MAKE_YUV_601(19, 19, 19),	/* black */
		MAKE_YUV_601(192, 192, 192),	/* grey */
	};
	const struct color_yuv colors_bottom[] = {
		MAKE_YUV_601(0, 33, 76),	/* in-phase */
		MAKE_YUV_601(255, 255, 255),	/* super white */
		MAKE_YUV_601(50, 0, 106),	/* quadrature */
		MAKE_YUV_601(19, 19, 19),	/* black */
		MAKE_YUV_601(9, 9, 9),		/* 3.5% */
		MAKE_YUV_601(19, 19, 19),	/* 7.5% */
		MAKE_YUV_601(29, 29, 29),	/* 11.5% */
		MAKE_YUV_601(19, 19, 19),	/* black */
	};
	unsigned char *y_mem = (yuv->order & YUV_YC) ? mem : mem + 1;
	unsigned char *c_mem = (yuv->order & YUV_CY) ? mem : mem + 1;
	unsigned int u = (yuv->order & YUV_YCrCb) ? 2 : 0;
	unsigned int v = (yuv->order & YUV_YCbCr) ? 2 : 0;
	unsigned int x;
	unsigned int y;

	/* Luma */
	for (y = 0; y < height * 6 / 9; ++y) {
		for (x = 0; x < width; ++x)
			y_mem[2*x] = colors_top[x * 7 / width].y;
		y_mem += stride * 2;
	}

	for (; y < height * 7 / 9; ++y) {
		for (x = 0; x < width; ++x)
			y_mem[2*x] = colors_middle[x * 7 / width].y;
		y_mem += stride * 2;
	}

	for (; y < height; ++y) {
		for (x = 0; x < width * 5 / 7; ++x)
			y_mem[2*x] = colors_bottom[x * 4 / (width * 5 / 7)].y;
		for (; x < width * 6 / 7; ++x)
			y_mem[2*x] = colors_bottom[(x - width * 5 / 7) * 3
						   / (width / 7) + 4].y;
		for (; x < width; ++x)
			y_mem[2*x] = colors_bottom[7].y;
		y_mem += stride * 2;
	}

	/* Chroma */
	for (y = 0; y < height * 6 / 9; ++y) {
		for (x = 0; x < width; x += 2) {
			c_mem[2*x+u] = colors_top[x * 7 / width].u;
			c_mem[2*x+v] = colors_top[x * 7 / width].v;
		}
		c_mem += stride * 2;
	}

	for (; y < height * 7 / 9; ++y) {
		for (x = 0; x < width; x += 2) {
			c_mem[2*x+u] = colors_middle[x * 7 / width].u;
			c_mem[2*x+v] = colors_middle[x * 7 / width].v;
		}
		c_mem += stride * 2;
	}

	for (; y < height; ++y) {
		for (x = 0; x < width * 5 / 7; x += 2) {
			c_mem[2*x+u] = colors_bottom[x * 4 / (width * 5 / 7)].u;
			c_mem[2*x+v] = colors_bottom[x * 4 / (width * 5 / 7)].v;
		}
		for (; x < width * 6 / 7; x += 2) {
			c_mem[2*x+u] = colors_bottom[(x - width * 5 / 7) *
						     3 / (width / 7) + 4].u;
			c_mem[2*x+v] = colors_bottom[(x - width * 5 / 7) *
						     3 / (width / 7) + 4].v;
		}
		for (; x < width; x += 2) {
			c_mem[2*x+u] = colors_bottom[7].u;
			c_mem[2*x+v] = colors_bottom[7].v;
		}
		c_mem += stride * 2;
	}
}

static void
fill_smpte_rgb16(const struct rgb_info *rgb, unsigned char *mem,
		 unsigned int width, unsigned int height, unsigned int stride)
{
	const uint16_t colors_top[] = {
		MAKE_RGBA(rgb, 192, 192, 192, 255),	/* grey */
		MAKE_RGBA(rgb, 192, 192, 0, 255),	/* yellow */
		MAKE_RGBA(rgb, 0, 192, 192, 255),	/* cyan */
		MAKE_RGBA(rgb, 0, 192, 0, 255),		/* green */
		MAKE_RGBA(rgb, 192, 0, 192, 255),	/* magenta */
		MAKE_RGBA(rgb, 192, 0, 0, 255),		/* red */
		MAKE_RGBA(rgb, 0, 0, 192, 255),		/* blue */
	};
	const uint16_t colors_middle[] = {
		MAKE_RGBA(rgb, 0, 0, 192, 255),		/* blue */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
		MAKE_RGBA(rgb, 192, 0, 192, 255),	/* magenta */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
		MAKE_RGBA(rgb, 0, 192, 192, 255),	/* cyan */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
		MAKE_RGBA(rgb, 192, 192, 192, 255),	/* grey */
	};
	const uint16_t colors_bottom[] = {
		MAKE_RGBA(rgb, 0, 33, 76, 255),		/* in-phase */
		MAKE_RGBA(rgb, 255, 255, 255, 255),	/* super white */
		MAKE_RGBA(rgb, 50, 0, 106, 255),	/* quadrature */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
		MAKE_RGBA(rgb, 9, 9, 9, 255),		/* 3.5% */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* 7.5% */
		MAKE_RGBA(rgb, 29, 29, 29, 255),	/* 11.5% */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
	};
	unsigned int x;
	unsigned int y;

	for (y = 0; y < height * 6 / 9; ++y) {
		for (x = 0; x < width; ++x)
			((uint16_t *)mem)[x] = colors_top[x * 7 / width];
		mem += stride;
	}

	for (; y < height * 7 / 9; ++y) {
		for (x = 0; x < width; ++x)
			((uint16_t *)mem)[x] = colors_middle[x * 7 / width];
		mem += stride;
	}

	for (; y < height; ++y) {
		for (x = 0; x < width * 5 / 7; ++x)
			((uint16_t *)mem)[x] =
				colors_bottom[x * 4 / (width * 5 / 7)];
		for (; x < width * 6 / 7; ++x)
			((uint16_t *)mem)[x] =
				colors_bottom[(x - width * 5 / 7) * 3
					      / (width / 7) + 4];
		for (; x < width; ++x)
			((uint16_t *)mem)[x] = colors_bottom[7];
		mem += stride;
	}
}

static void
fill_smpte_rgb32(const struct rgb_info *rgb, unsigned char *mem,
		 unsigned int width, unsigned int height, unsigned int stride)
{
	const uint32_t colors_top[] = {
		MAKE_RGBA(rgb, 192, 192, 192, 255),	/* grey */
		MAKE_RGBA(rgb, 192, 192, 0, 255),	/* yellow */
		MAKE_RGBA(rgb, 0, 192, 192, 255),	/* cyan */
		MAKE_RGBA(rgb, 0, 192, 0, 255),		/* green */
		MAKE_RGBA(rgb, 192, 0, 192, 255),	/* magenta */
		MAKE_RGBA(rgb, 192, 0, 0, 255),		/* red */
		MAKE_RGBA(rgb, 0, 0, 192, 255),		/* blue */
	};
	const uint32_t colors_middle[] = {
		MAKE_RGBA(rgb, 0, 0, 192, 255),		/* blue */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
		MAKE_RGBA(rgb, 192, 0, 192, 255),	/* magenta */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
		MAKE_RGBA(rgb, 0, 192, 192, 255),	/* cyan */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
		MAKE_RGBA(rgb, 192, 192, 192, 255),	/* grey */
	};
	const uint32_t colors_bottom[] = {
		MAKE_RGBA(rgb, 0, 33, 76, 255),		/* in-phase */
		MAKE_RGBA(rgb, 255, 255, 255, 255),	/* super white */
		MAKE_RGBA(rgb, 50, 0, 106, 255),	/* quadrature */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
		MAKE_RGBA(rgb, 9, 9, 9, 255),		/* 3.5% */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* 7.5% */
		MAKE_RGBA(rgb, 29, 29, 29, 255),	/* 11.5% */
		MAKE_RGBA(rgb, 19, 19, 19, 255),	/* black */
	};
	unsigned int x;
	unsigned int y;

	for (y = 0; y < height * 6 / 9; ++y) {
		for (x = 0; x < width; ++x)
			((uint32_t *)mem)[x] = colors_top[x * 7 / width];
		mem += stride;
	}

	for (; y < height * 7 / 9; ++y) {
		for (x = 0; x < width; ++x)
			((uint32_t *)mem)[x] = colors_middle[x * 7 / width];
		mem += stride;
	}

	for (; y < height; ++y) {
		for (x = 0; x < width * 5 / 7; ++x)
			((uint32_t *)mem)[x] =
				colors_bottom[x * 4 / (width * 5 / 7)];
		for (; x < width * 6 / 7; ++x)
			((uint32_t *)mem)[x] =
				colors_bottom[(x - width * 5 / 7) * 3
					      / (width / 7) + 4];
		for (; x < width; ++x)
			((uint32_t *)mem)[x] = colors_bottom[7];
		mem += stride;
	}
}

static void
fill_smpte(const struct format_info *info, void *planes[3], unsigned int width,
	   unsigned int height, unsigned int stride)
{
	unsigned char *u, *v;

	switch (info->format) {
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		return fill_smpte_yuv_packed(&info->yuv, planes[0], width,
					     height, stride);

	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		u = info->yuv.order & YUV_YCbCr ? planes[1] : planes[1] + 1;
		v = info->yuv.order & YUV_YCrCb ? planes[1] : planes[1] + 1;
		return fill_smpte_yuv_planar(&info->yuv, planes[0], u, v,
					     width, height, stride);

	case DRM_FORMAT_YVU420:
		return fill_smpte_yuv_planar(&info->yuv, planes[0], planes[1],
					     planes[2], width, height, stride);

	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
		return fill_smpte_rgb16(&info->rgb, planes[0],
					width, height, stride);
	case DRM_FORMAT_XRGB8888:
		return fill_smpte_rgb32(&info->rgb, planes[0],
					width, height, stride);
	}
}

/* swap these for big endian.. */
#define RED   2
#define GREEN 1
#define BLUE  0

static void
make_pwetty(void *data, int width, int height, int stride)
{
#ifdef HAVE_CAIRO
	cairo_surface_t *surface;
	cairo_t *cr;
	int x, y;

	surface = cairo_image_surface_create_for_data(data,
						      CAIRO_FORMAT_ARGB32,
						      width, height,
						      stride);
	cr = cairo_create(surface);
	cairo_surface_destroy(surface);

	cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
	for (x = 0; x < width; x += 250)
		for (y = 0; y < height; y += 250) {
			char buf[64];

			cairo_move_to(cr, x, y - 20);
			cairo_line_to(cr, x, y + 20);
			cairo_move_to(cr, x - 20, y);
			cairo_line_to(cr, x + 20, y);
			cairo_new_sub_path(cr);
			cairo_arc(cr, x, y, 10, 0, M_PI * 2);
			cairo_set_line_width(cr, 4);
			cairo_set_source_rgb(cr, 0, 0, 0);
			cairo_stroke_preserve(cr);
			cairo_set_source_rgb(cr, 1, 1, 1);
			cairo_set_line_width(cr, 2);
			cairo_stroke(cr);

			snprintf(buf, sizeof buf, "%d, %d", x, y);
			cairo_move_to(cr, x + 20, y + 20);
			cairo_text_path(cr, buf);
			cairo_set_source_rgb(cr, 0, 0, 0);
			cairo_stroke_preserve(cr);
			cairo_set_source_rgb(cr, 1, 1, 1);
			cairo_fill(cr);
		}

	cairo_destroy(cr);
#endif
}

static void
fill_tiles_yuv_planar(const struct yuv_info *yuv,
		      unsigned char *y_mem, unsigned char *u_mem,
		      unsigned char *v_mem, unsigned int width,
		      unsigned int height, unsigned int stride)
{
	unsigned int cs = yuv->chroma_stride;
	unsigned int xsub = yuv->xsub;
	unsigned int ysub = yuv->ysub;
	unsigned int x;
	unsigned int y;

	for (y = 0; y < height; ++y) {
		for (x = 0; x < width; ++x) {
			div_t d = div(x+y, width);
			uint32_t rgb32 = 0x00130502 * (d.quot >> 6)
				       + 0x000a1120 * (d.rem >> 6);
			struct color_yuv color =
				MAKE_YUV_601((rgb32 >> 16) & 0xff,
					     (rgb32 >> 8) & 0xff, rgb32 & 0xff);

			y_mem[x] = color.y;
			u_mem[x/xsub*cs] = color.u;
			v_mem[x/xsub*cs] = color.v;
		}

		y_mem += stride;
		if ((y + 1) % ysub == 0) {
			u_mem += stride * cs / xsub;
			v_mem += stride * cs / xsub;
		}
	}
}

static void
fill_tiles_yuv_packed(const struct yuv_info *yuv, unsigned char *mem,
		      unsigned int width, unsigned int height,
		      unsigned int stride)
{
	unsigned char *y_mem = (yuv->order & YUV_YC) ? mem : mem + 1;
	unsigned char *c_mem = (yuv->order & YUV_CY) ? mem : mem + 1;
	unsigned int u = (yuv->order & YUV_YCrCb) ? 2 : 0;
	unsigned int v = (yuv->order & YUV_YCbCr) ? 2 : 0;
	unsigned int x;
	unsigned int y;

	for (y = 0; y < height; ++y) {
		for (x = 0; x < width; x += 2) {
			div_t d = div(x+y, width);
			uint32_t rgb32 = 0x00130502 * (d.quot >> 6)
				       + 0x000a1120 * (d.rem >> 6);
			struct color_yuv color =
				MAKE_YUV_601((rgb32 >> 16) & 0xff,
					     (rgb32 >> 8) & 0xff, rgb32 & 0xff);

			y_mem[2*x] = color.y;
			c_mem[2*x+u] = color.u;
			y_mem[2*x+2] = color.y;
			c_mem[2*x+v] = color.v;
		}

		y_mem += stride;
		c_mem += stride;
	}
}

static void
fill_tiles_rgb16(const struct rgb_info *rgb, unsigned char *mem,
		 unsigned int width, unsigned int height, unsigned int stride)
{
	unsigned int i, j;

	for (j = 0; j < height; j++) {
		uint16_t *ptr = (uint16_t*)((char*)mem + j * stride);
		for (i = 0; i < width; i++) {
			div_t d = div(i+j, width);
			uint32_t rgb = 0x00130502 * (d.quot >> 6) + 0x000a1120 * (d.rem >> 6);
			unsigned char *rgbp = (unsigned char *)&rgb;

			*(ptr++) = 0x8000 |
					(rgbp[RED] >> 3) << 10 |
					(rgbp[GREEN] >> 3) << 5 |
					(rgbp[BLUE] >> 3);
		}
	}
}

static void
fill_tiles_rgb32(const struct rgb_info *rgb, unsigned char *mem,
		 unsigned int width, unsigned int height, unsigned int stride)
{
	unsigned int i, j;

	for (j = 0; j < height; j++) {
		uint32_t *ptr = (uint32_t*)((char*)mem + j * stride);
		for (i = 0; i < width; i++) {
			div_t d = div(i, width);
			ptr[i] =
				0x00130502 * (d.quot >> 6) +
				0x000a1120 * (d.rem >> 6);
		}
	}

	make_pwetty(mem, width, height, stride);
}

static void
fill_tiles(const struct format_info *info, void *planes[3], unsigned int width,
	   unsigned int height, unsigned int stride)
{
	unsigned char *u, *v;

	switch (info->format) {
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		return fill_tiles_yuv_packed(&info->yuv, planes[0],
					     width, height, stride);

	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		u = info->yuv.order & YUV_YCbCr ? planes[1] : planes[1] + 1;
		v = info->yuv.order & YUV_YCrCb ? planes[1] : planes[1] + 1;
		return fill_tiles_yuv_planar(&info->yuv, planes[0], u, v,
					     width, height, stride);

	case DRM_FORMAT_YVU420:
		return fill_tiles_yuv_planar(&info->yuv, planes[0], planes[1],
					     planes[2], width, height, stride);

	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
		return fill_tiles_rgb16(&info->rgb, planes[0],
					width, height, stride);
	case DRM_FORMAT_XRGB8888:
		return fill_tiles_rgb32(&info->rgb, planes[0],
					width, height, stride);
	}
}

static void
fill_plain(const struct format_info *info, void *planes[3], unsigned int width,
	   unsigned int height, unsigned int stride)
{
	memset(planes[0], 0x77, stride * height);
}

/*
 * fill_pattern - Fill a buffer with a test pattern
 * @format: Pixel format
 * @pattern: Test pattern
 * @buffer: Buffer memory
 * @width: Width in pixels
 * @height: Height in pixels
 * @stride: Line stride (pitch) in bytes
 *
 * Fill the buffer with the test pattern specified by the pattern parameter.
 * Supported formats vary depending on the selected pattern.
 */
static void
fill_pattern(unsigned int format, enum fill_pattern pattern,
	     void *planes[3],
	     unsigned int width, unsigned int height, unsigned int stride)
{
	const struct format_info *info = NULL;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(format_info); ++i) {
		if (format_info[i].format == format) {
			info = &format_info[i];
			break;
		}
	}

	if (info == NULL)
		return;

	switch (pattern) {
	case PATTERN_TILES:
		return fill_tiles(info, planes, width, height, stride);

	case PATTERN_SMPTE:
		return fill_smpte(info, planes, width, height, stride);

	case PATTERN_PLAIN:
		return fill_plain(info, planes, width, height, stride);

	default:
		printf("Error: unsupported test pattern %u.\n", pattern);
		break;
	}
}

/* -----------------------------------------------------------------------------
 * Buffers management
 */

static struct kms_bo *
allocate_buffer(struct kms_driver *kms,
		int width, int height, int *stride)
{
	struct kms_bo *bo;
	unsigned bo_attribs[] = {
		KMS_WIDTH,   0,
		KMS_HEIGHT,  0,
		KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
		KMS_TERMINATE_PROP_LIST
	};
	int ret;

	bo_attribs[1] = width;
	bo_attribs[3] = height;

	ret = kms_bo_create(kms, bo_attribs, &bo);
	if (ret) {
		fprintf(stderr, "failed to alloc buffer: %s\n",
			strerror(-ret));
		return NULL;
	}

	ret = kms_bo_get_prop(bo, KMS_PITCH, stride);
	if (ret) {
		fprintf(stderr, "failed to retreive buffer stride: %s\n",
			strerror(-ret));
		kms_bo_destroy(&bo);
		return NULL;
	}

	return bo;
}

static struct kms_bo *
create_test_buffer(struct kms_driver *kms, unsigned int format,
		   int width, int height, int handles[4],
		   int pitches[4], int offsets[4], enum fill_pattern pattern)
{
	struct kms_bo *bo;
	int ret, stride;
	void *planes[3];
	void *virtual;

	bo = allocate_buffer(kms, width, height, &pitches[0]);
	if (!bo)
		return NULL;

	ret = kms_bo_map(bo, &virtual);
	if (ret) {
		fprintf(stderr, "failed to map buffer: %s\n",
			strerror(-ret));
		kms_bo_destroy(&bo);
		return NULL;
	}

	/* just testing a limited # of formats to test single
	 * and multi-planar path.. would be nice to add more..
	 */
	switch (format) {
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		pitches[0] = width * 2;
		offsets[0] = 0;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[0]);

		planes[0] = virtual;
		break;

	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		pitches[0] = width;
		offsets[0] = 0;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[0]);
		pitches[1] = width;
		offsets[1] = width * height;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[1]);

		planes[0] = virtual;
		planes[1] = virtual + offsets[1];
		break;

	case DRM_FORMAT_YVU420:
		pitches[0] = width;
		offsets[0] = 0;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[0]);
		pitches[1] = width / 2;
		offsets[1] = width * height;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[1]);
		pitches[2] = width / 2;
		offsets[2] = offsets[1] + (width * height) / 4;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[2]);

		planes[0] = virtual;
		planes[1] = virtual + offsets[1];
		planes[2] = virtual + offsets[2];
		break;

	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
		pitches[0] = width * 2;
		offsets[0] = 0;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[0]);

		planes[0] = virtual;
		break;

	case DRM_FORMAT_RGB888:
		pitches[0] = width * 3;
		offsets[0] = 0;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[0]);

		planes[0] = virtual;
		break;

	case DRM_FORMAT_XRGB8888:
		pitches[0] = width * 4;
		offsets[0] = 0;
		kms_bo_get_prop(bo, KMS_HANDLE, &handles[0]);

		planes[0] = virtual;
		break;
	}

	fill_pattern(format, pattern, planes, width, height, pitches[0]);
	kms_bo_unmap(bo);

	return bo;
}

/* -------------------------------------------------------------------------- */

void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct connector *c;
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
set_plane(struct kms_driver *kms, struct connector *c, struct plane *p)
{
	drmModePlaneRes *plane_resources;
	drmModePlane *ovr;
	uint32_t handles[4], pitches[4], offsets[4] = {0}; /* we only use [0] */
	uint32_t plane_id = 0;
	struct kms_bo *plane_bo;
	uint32_t plane_flags = 0, format;
	int ret, crtc_x, crtc_y, crtc_w, crtc_h;
	unsigned int i;

	format = format_fourcc(p->format_str);
	if (format == 0) {
		fprintf(stderr, "Unknown format: %s\n", p->format_str);
		return -1;
	}

	/* find an unused plane which can be connected to our crtc */
	plane_resources = drmModeGetPlaneResources(fd);
	if (!plane_resources) {
		fprintf(stderr, "drmModeGetPlaneResources failed: %s\n",
			strerror(errno));
		return -1;
	}

	for (i = 0; i < plane_resources->count_planes && !plane_id; i++) {
		ovr = drmModeGetPlane(fd, plane_resources->planes[i]);
		if (!ovr) {
			fprintf(stderr, "drmModeGetPlane failed: %s\n",
				strerror(errno));
			return -1;
		}

		if ((ovr->possible_crtcs & (1 << c->pipe)) && !ovr->crtc_id)
			plane_id = ovr->plane_id;

		drmModeFreePlane(ovr);
	}

	fprintf(stderr, "testing %dx%d@%s overlay plane\n",
			p->w, p->h, p->format_str);

	if (!plane_id) {
		fprintf(stderr, "failed to find plane!\n");
		return -1;
	}

	plane_bo = create_test_buffer(kms, format, p->w, p->h, handles,
				      pitches, offsets, PATTERN_TILES);
	if (plane_bo == NULL)
		return -1;

	/* just use single plane format for now.. */
	if (drmModeAddFB2(fd, p->w, p->h, format,
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

	return 0;
}

static void
set_mode(struct connector *c, int count, struct plane *p, int plane_count,
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

	bo = create_test_buffer(kms, DRM_FORMAT_XRGB8888, width, height, handles,
				pitches, offsets, PATTERN_SMPTE);
	if (bo == NULL)
		return;

	ret = drmModeAddFB(fd, width, height, 24, 32, pitches[0], handles[0], &fb_id);
	if (ret) {
		fprintf(stderr, "failed to add fb (%ux%u): %s\n",
			width, height, strerror(errno));
		return;
	}

	x = 0;
	for (i = 0; i < count; i++) {
		if (c[i].mode == NULL)
			continue;

		printf("setting mode %s on connector %d, crtc %d\n",
		       c[i].mode_str, c[i].id, c[i].crtc);

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
	
	other_bo = create_test_buffer(kms, DRM_FORMAT_XRGB8888, width, height, handles,
				      pitches, offsets, PATTERN_PLAIN);
	if (other_bo == NULL)
		return;

	ret = drmModeAddFB(fd, width, height, 32, 32, pitches[0], handles[0],
			   &other_fb_id);
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

extern char *optarg;
extern int optind, opterr, optopt;
static char optstr[] = "ecpmfs:P:v";

void usage(char *name)
{
	fprintf(stderr, "usage: %s [-ecpmf]\n", name);
	fprintf(stderr, "\t-e\tlist encoders\n");
	fprintf(stderr, "\t-c\tlist connectors\n");
	fprintf(stderr, "\t-p\tlist CRTCs and planes (pipes)\n");
	fprintf(stderr, "\t-m\tlist modes\n");
	fprintf(stderr, "\t-f\tlist framebuffers\n");
	fprintf(stderr, "\t-v\ttest vsynced page flipping\n");
	fprintf(stderr, "\t-s <connector_id>:<mode>\tset a mode\n");
	fprintf(stderr, "\t-s <connector_id>@<crtc_id>:<mode>\tset a mode\n");
	fprintf(stderr, "\t-P <connector_id>:<w>x<h>\tset a plane\n");
	fprintf(stderr, "\t-P <connector_id>:<w>x<h>@<format>\tset a plane\n");
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

int main(int argc, char **argv)
{
	int c;
	int encoders = 0, connectors = 0, crtcs = 0, planes = 0, framebuffers = 0;
	int test_vsync = 0;
	char *modules[] = { "i915", "radeon", "nouveau", "vmwgfx", "omapdrm", "exynos" };
	unsigned int i;
	int count = 0, plane_count = 0;
	struct connector con_args[2];
	struct plane plane_args[2] = {0};
	
	opterr = 0;
	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
		case 'e':
			encoders = 1;
			break;
		case 'c':
			connectors = 1;
			break;
		case 'p':
			crtcs = 1;
			planes = 1;
			break;
		case 'm':
			modes = 1;
			break;
		case 'f':
			framebuffers = 1;
			break;
		case 'v':
			test_vsync = 1;
			break;
		case 's':
			con_args[count].crtc = -1;
			if (sscanf(optarg, "%d:%64s",
				   &con_args[count].id,
				   con_args[count].mode_str) != 2 &&
			    sscanf(optarg, "%d@%d:%64s",
				   &con_args[count].id,
				   &con_args[count].crtc,
				   con_args[count].mode_str) != 3)
				usage(argv[0]);
			count++;				      
			break;
		case 'P':
			strcpy(plane_args[plane_count].format_str, "XR24");
			if (sscanf(optarg, "%d:%dx%d@%4s",
					&plane_args[plane_count].con_id,
					&plane_args[plane_count].w,
					&plane_args[plane_count].h,
					plane_args[plane_count].format_str) != 4 &&
				sscanf(optarg, "%d:%dx%d",
					&plane_args[plane_count].con_id,
					&plane_args[plane_count].w,
					&plane_args[plane_count].h) != 3)
				usage(argv[0]);
			plane_count++;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	if (argc == 1)
		encoders = connectors = crtcs = planes = modes = framebuffers = 1;

	for (i = 0; i < ARRAY_SIZE(modules); i++) {
		printf("trying to load module %s...", modules[i]);
		fd = drmOpen(modules[i], NULL);
		if (fd < 0) {
			printf("failed.\n");
		} else {
			printf("success.\n");
			break;
		}
	}

	if (test_vsync && !page_flipping_supported()) {
		fprintf(stderr, "page flipping not supported by drm.\n");
		return -1;
	}

	if (i == ARRAY_SIZE(modules)) {
		fprintf(stderr, "failed to load any modules, aborting.\n");
		return -1;
	}

	resources = drmModeGetResources(fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed: %s\n",
			strerror(errno));
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
		getchar();
	}

	drmModeFreeResources(resources);

	return 0;
}
