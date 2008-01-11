
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include "xf86drm.h"
#include "xf86drmMode.h"

#define SIZE_X 2048
#define SIZE_Y 2048
/* Pitch needs to be power of two */
#define PITCH 2048

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
void prettyColors(int fd, unsigned int handle);

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

	prettyColors(fd, framebuffer->handle);

	printf("0 0\n");
	drmModeSetCrtc(fd, crtc->crtc_id, framebuffer->buffer_id, 0, 0, &out[0]->output_id, 1, &mode);
	sleep(2);

	printf("0 100\n");
	drmModeSetCrtc(fd, crtc->crtc_id, framebuffer->buffer_id, 0, 100, &out[0]->output_id, 1, &mode);
	sleep(2);

	printf("100 0\n");
	drmModeSetCrtc(fd, crtc->crtc_id, framebuffer->buffer_id, 100, 0, &out[0]->output_id, 1, &mode);
	sleep(2);

	printf("100 100\n");
	drmModeSetCrtc(fd, crtc->crtc_id, framebuffer->buffer_id, 100, 100, &out[0]->output_id, 1, &mode);
	sleep(2);

	/* turn the crtc off just in case */
	drmModeSetCrtc(fd, crtc->crtc_id, 0, 0, 0, 0, 0, 0);

    drmModeFreeResources(res);
    printf("Ok\n");

    return 0;
}

drmModeFBPtr createFB(int fd, drmModeResPtr res)
{
	drmModeFBPtr frame;
	unsigned int fb = 0;
	int ret = 0;
	drmBO bo;

	ret = drmBOCreate(fd, SIZE_X * SIZE_Y * 4, 0, 0,
		DRM_BO_FLAG_READ |
		DRM_BO_FLAG_WRITE |
		DRM_BO_FLAG_MEM_TT |
		DRM_BO_FLAG_MEM_VRAM |
		DRM_BO_FLAG_NO_EVICT,
		DRM_BO_HINT_DONT_FENCE, &bo);

	if (ret)
		goto err;

	ret = drmModeAddFB(fd, SIZE_X, SIZE_Y, 32, 32, PITCH * 4, &bo, &fb);

	if (ret)
		goto err_bo;

	frame = drmModeGetFB(fd, fb);

	if (!frame)
		goto err_bo;

	return frame;

err_bo:
	drmBOUnreference(fd, &bo);
err:
	printf("Something went wrong when creating a fb, using one of the predefined ones\n");

	return drmModeGetFB(fd, res->fbs[0]);
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

void draw(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int v, unsigned int *ptr)
{
	int i, j;

	for (i = x; i < x + w; i++)
		for(j = y; j < y + h; j++)
			ptr[(i * PITCH) + j] = v;

}

void prettyColors(int fd, unsigned int handle)
{
	drmBO bo;
	unsigned int *ptr;
	int i;

	drmBOReference(fd, handle, &bo);
	drmBOMap(fd, &bo, DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE, 0, (void**)&ptr);

	for (i = 0; i < (SIZE_X*SIZE_Y); i++)
		ptr[i] = 0xFFFFFFFF;

	for (i = 0; i < 8; i++)
		draw(i * 40, i * 40, 40, 40, 0, ptr);


	draw(200, 100, 40, 40, 0xff00ff, ptr);
	draw(100, 200, 40, 40, 0xff00ff, ptr);

	drmBOUnmap(fd, &bo);
}
