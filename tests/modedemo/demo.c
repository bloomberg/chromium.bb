
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include "xf86drm.h"
#include "xf86drmMode.h"
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

drmModeFBPtr createFB(int fd, drmModeResPtr res);
int findConnectedOutputs(int fd, drmModeResPtr res, drmModeOutputPtr *out);
drmModeCrtcPtr findFreeCrtc(int fd, drmModeResPtr res);

int main(int argc, char **argv)
{
	int fd;
	const char *driver = "i915"; /* hardcoded for now */
	drmModeResPtr res;
	drmModeFBPtr framebuffer;
	int numOutputs;
	drmModeOutputPtr out[8];
	drmModeCrtcPtr crtc;

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

	framebuffer = createFB(fd, res);
	if (framebuffer == NULL) {
		printf("Failed to create framebuffer\n");
		return 1;
	}

	numOutputs = findConnectedOutputs(fd, res, out);
	if (numOutputs < 1) {
		printf("Failed to find connected outputs\n");
		return 1;
	}

	crtc = findFreeCrtc(fd, res);
	if (numOutputs < 1) {
		printf("Couldn't find a free crtc\n");
		return 1;
	}


	drmModeSetCrtc(fd, crtc->crtc_id, framebuffer->buffer_id, 0, 0, &out[0]->output_id, 1, &mode);
	sleep(2);
	drmModeSetCrtc(fd, crtc->crtc_id, framebuffer->buffer_id, 0, 500, &out[0]->output_id, 1, &mode);
	sleep(2);
	drmModeSetCrtc(fd, crtc->crtc_id, framebuffer->buffer_id, 500, 0, &out[0]->output_id, 1, &mode);
	sleep(2);
	drmModeSetCrtc(fd, crtc->crtc_id, framebuffer->buffer_id, 500, 500, &out[0]->output_id, 1, &mode);

    drmModeFreeResources(res);
    printf("Ok\n");

    return 0;
}

drmModeFBPtr createFB(int fd, drmModeResPtr res)
{
	/* Haveing problems getting drmBOCreate to work with me. */
	return drmModeGetFB(fd, res->fbs[1]);
}

int findConnectedOutputs(int fd, drmModeResPtr res, drmModeOutputPtr *out)
{
	int count = 0;
	int i;

	drmModeOutputPtr output;

	for (i = 0; i < res->count_outputs; i++) {
		output = drmModeGetOutput(fd, res->outputs[i]);

		if (!output || output->connection != DRM_MODE_CONNECTED)
			continue;

		out[count++] = output;
	}

	return count;
}

drmModeCrtcPtr findFreeCrtc(int fd, drmModeResPtr res)
{
	return drmModeGetCrtc(fd, res->crtcs[0]);
}
