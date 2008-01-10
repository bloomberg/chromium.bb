
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include "xf86drm.h"
#include "xf86drmMode.h"

int dpms_prop_id = 0;
const char* getConnectionText(drmModeConnection conn)
{
	switch (conn) {
	case DRM_MODE_CONNECTED:
		return "connected";
	case DRM_MODE_DISCONNECTED:
		return "disconnected";
	default:
		return "unknown";
	}

}

int printMode(struct drm_mode_modeinfo *mode)
{
#if 1
	printf("Mode: %s\n", mode->name);
	printf("\tclock       : %i\n", mode->clock);
	printf("\thdisplay    : %i\n", mode->hdisplay);
	printf("\thsync_start : %i\n", mode->hsync_start);
	printf("\thsync_end   : %i\n", mode->hsync_end);
	printf("\thtotal      : %i\n", mode->htotal);
	printf("\thskew       : %i\n", mode->hskew);
	printf("\tvdisplay    : %i\n", mode->vdisplay);
	printf("\tvsync_start : %i\n", mode->vsync_start);
	printf("\tvsync_end   : %i\n", mode->vsync_end);
	printf("\tvtotal      : %i\n", mode->vtotal);
	printf("\tvscan       : %i\n", mode->vscan);
	printf("\tvrefresh    : %i\n", mode->vrefresh);
	printf("\tflags       : %i\n", mode->flags);
#else
	printf("Mode: \"%s\" %ix%i %.0f\n", mode->name,
		mode->hdisplay, mode->vdisplay, mode->vrefresh / 1000.0);
#endif
	return 0;
}

int printOutput(int fd, drmModeResPtr res, drmModeOutputPtr output, uint32_t id)
{
	int i = 0, j;
	struct drm_mode_modeinfo *mode = NULL;
	drmModePropertyPtr props;
	unsigned char *name = NULL;

	printf("Output: %d-%d\n", output->output_type, output->output_type_id);
	printf("\tid           : %i\n", id);
	printf("\tcrtc id      : %i\n", output->crtc);
	printf("\tconn         : %s\n", getConnectionText(output->connection));
	printf("\tsize         : %ix%i (mm)\n", output->mmWidth, output->mmHeight);
	printf("\tcount_crtcs  : %i\n", output->count_crtcs);
	printf("\tcrtcs        : %i\n", output->crtcs);
	printf("\tcount_clones : %i\n", output->count_clones);
	printf("\tclones       : %i\n", output->clones);
	printf("\tcount_modes  : %i\n", output->count_modes);
	printf("\tcount_props  : %i\n", output->count_props);

	for (i = 0; i < output->count_props; i++) {
		props = drmModeGetProperty(fd, output->props[i]);
		name = NULL;
		if (props) {
			printf("Property: %s\n", props->name);
			printf("\tid:        %i\n", props->prop_id);
			printf("\tflags:     %i\n", props->flags);
			printf("\tvalues %d: ", props->count_values);
			for (j = 0; j < props->count_values; j++)
				printf("%lld ", props->values[j]);

			printf("\n\tenums %d: \n", props->count_enums);
			
			if (props->flags & DRM_MODE_PROP_BLOB) {
				drmModePropertyBlobPtr blob;

				blob = drmModeGetPropertyBlob(fd, output->prop_values[i]);
				if (blob) {
					printf("blob is %d length, %08X\n", blob->length, *(uint32_t *)blob->data);
					drmModeFreePropertyBlob(blob);
				}

			} else {
				if (!strncmp(props->name, "DPMS", 4))
					dpms_prop_id = props->prop_id;

				for (j = 0; j < props->count_enums; j++) {
				  printf("\t\t%lld = %s\n", props->enums[j].value, props->enums[j].name);
					if (output->prop_values[i] == props->enums[j].value)
						name = props->enums[j].name;

				}

				if (props->count_enums && name) {
					printf("\toutput property name %s %s\n", props->name, name);
				} else {
					printf("\toutput property id %s %lli\n", props->name, output->prop_values[i]);
				}
			}

			drmModeFreeProperty(props);
		}
	}

	for (i = 0; i < output->count_modes; i++) {
		mode = &output->modes[i];
		if (mode)
			printMode(mode);
		else
			printf("\t\tmode: Invalid mode %p\n", &output->modes[i]);
	}

	return 0;
}

int printCrtc(int fd, drmModeResPtr res, drmModeCrtcPtr crtc, uint32_t id)
{
	printf("Crtc\n");
	printf("\tid           : %i\n", id);
	printf("\tx            : %i\n", crtc->x);
	printf("\ty            : %i\n", crtc->y);
	printf("\twidth        : %i\n", crtc->width);
	printf("\theight       : %i\n", crtc->height);
	printf("\tmode         : %p\n", &crtc->mode);
	printf("\tnum outputs  : %i\n", crtc->count_outputs);
	printf("\toutputs      : %i\n", crtc->outputs);
	printf("\tnum possible : %i\n", crtc->count_possibles);
	printf("\tpossibles    : %i\n", crtc->possibles);

	return 0;
}

int printFrameBuffer(int fd, drmModeResPtr res, drmModeFBPtr fb)
{
	printf("Framebuffer\n");
	printf("\thandle    : %i\n", fb->handle);
	printf("\twidth     : %i\n", fb->width);
	printf("\theight    : %i\n", fb->height);
	printf("\tpitch     : %i\n", fb->pitch);;
	printf("\tbpp       : %i\n", fb->bpp);
	printf("\tdepth     : %i\n", fb->depth);
	printf("\tbuffer_id : %i\n", fb->buffer_id);

	return 0;
}

int printRes(int fd, drmModeResPtr res)
{
	int i;
	drmModeOutputPtr output;
	drmModeCrtcPtr crtc;
	drmModeFBPtr fb;

	for (i = 0; i < res->count_outputs; i++) {
		output = drmModeGetOutput(fd, res->outputs[i]);

		if (!output)
			printf("Could not get output %i\n", i);
		else {
			printOutput(fd, res, output, res->outputs[i]);
			drmModeFreeOutput(output);
		}
	}

	for (i = 0; i < res->count_crtcs; i++) {
		crtc = drmModeGetCrtc(fd, res->crtcs[i]);

		if (!crtc)
			printf("Could not get crtc %i\n", i);
		else {
			printCrtc(fd, res, crtc, res->crtcs[i]);
			drmModeFreeCrtc(crtc);
		}
	}

	for (i = 0; i < res->count_fbs; i++) {
		fb = drmModeGetFB(fd, res->fbs[i]);

		if (!fb)
			printf("Could not get fb %i\n", res->fbs[i]);
		else {
			printFrameBuffer(fd, res, fb);
			drmModeFreeFB(fb);
		}
	}

	return 0;
}

static struct drm_mode_modeinfo mode = {
	.name = "Test mode",
	.clock = 25200,
	.hdisplay = 640,
	.hsync_start = 656,
	.hsync_end = 752,
	.htotal = 800,
	.hskew = 0,
	.vdisplay = 480,
	.vsync_start = 490,
	.vsync_end = 492,
	.vtotal = 525,
	.vscan = 0,
	.vrefresh = 60000, /* vertical refresh * 1000 */
	.flags = 10,
};

int testMode(int fd, drmModeResPtr res)
{
	uint32_t output = res->outputs[0];
	uint32_t newMode = 0;
	int ret = 0;
	int error = 0;

	printf("Test: adding mode to output %i\n", output);

	/* printMode(&mode); */

	printf("\tAttaching mode %i to output %i\n", newMode, output);

	ret = drmModeAttachMode(fd, output, &mode);

	if (ret)
		goto err_mode;

	printf("\tDetaching mode %i from output %i\n", newMode, output);
	ret = drmModeDetachMode(fd, output, &mode);

	if (ret)
		goto err_mode;
	return 0;

err_mode:

	printf("\tFailed\n");

	if (error)
		printf("\tFailed to delete mode %i\n", newMode);
	return 1;
}

/*
int testFrameBufferGet(int fd, uint32_t fb)
{
	drmModeFBPtr frame;

	printf("Test: get framebuffer %i\n", fb);

	frame = drmModeGetFB(fd, fb);

	if (!frame) {
		printf("\tFailed\n");
	} else {
		printFrameBuffer(fd, frame);
		drmModeFreeFB(frame);
	}

	return 0;
}
*/

int testFrameBufferAdd(int fd, drmModeResPtr res)
{
	uint32_t fb = 0;
	int ret = 0;
	drmModeFBPtr frame = 0;
	drmBO bo;

	printf("Test: adding framebuffer\n");

	printf("\tCreating BO\n");

	/* TODO */
	ret = drmBOCreate(fd, 800 * 600 * 4, 0, 0,
		DRM_BO_FLAG_READ |
		DRM_BO_FLAG_WRITE |
		DRM_BO_FLAG_MEM_TT |
		DRM_BO_FLAG_MEM_VRAM |
		DRM_BO_FLAG_NO_EVICT,
		DRM_BO_HINT_DONT_FENCE, &bo);

	printf("\tgot %i\n", ret);
	if (ret)
		goto err;

	printf("\tAdding FB\n");
	ret = drmModeAddFB(fd, 640, 480, 32, 8, 0, &bo, &fb);
	if (ret)
		goto err_bo;

	frame = drmModeGetFB(fd, fb);

	if (!frame) {
		printf("Couldn't retrive created framebuffer\n");
	} else {
		printFrameBuffer(fd, res, frame);
		drmModeFreeFB(frame);
	}

	printf("\tRemoveing FB\n");

	ret = drmModeRmFB(fd, fb);

	if (ret) {
		printf("\tFailed this shouldn't happen!\n");
		goto err_bo;
	}

	printf("\tRemoveing BO\n");

	ret = drmBOUnreference(fb, &bo);

	return 0;
	
err_bo:
	drmBOUnreference(fd, &bo);

err:
	printf("\tFailed\n");

	return 1;
}

int testDPMS(int fd, drmModeResPtr res)
{
	int output_id;
	int i;

	for (i = 0; i < res->count_outputs; i++) {
		output_id = res->outputs[i];
		/* turn output off */
		drmModeOutputSetProperty(fd, output_id, dpms_prop_id, 3);
		sleep(2);
		drmModeOutputSetProperty(fd, output_id, dpms_prop_id, 0);
	}
	return 1;

}

int main(int argc, char **argv)
{
	int fd;
	const char *driver = "i915"; /* hardcoded for now */
	drmModeResPtr res;

	printf("Starting test\n");

	fd = drmOpen(driver, NULL);

	if (fd < 0) {
		printf("Failed to open the card fb\n");
		return 1;
	}

	res = drmModeGetResources(fd);
	if (res == 0) {
		printf("Failed to get resources from card\n");
		drmClose(fd);
		return 1;
	}

	printRes(fd, res);

	testMode(fd, res);

	testFrameBufferAdd(fd, res);

	/* try dpms test */
	testDPMS(fd, res);
	drmModeFreeResources(res);
	printf("Ok\n");

	return 0;
}
