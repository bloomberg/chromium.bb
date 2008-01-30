#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "linux/fb.h"
#include "sys/ioctl.h"
#include "xf86drm.h"
#include "xf86drmMode.h"

void pretty(int fd);
void setMode(struct fb_var_screeninfo *var);
void pan(int fd, struct fb_var_screeninfo *var, int x, int y);
void cursor(int fd, int drmfd);
void prettyCursor(int fd, unsigned int handle, unsigned int color);

extern void sleep(int);

int main(int argc, char **argv)
{
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	const char* driver = "i915";
	const char* name = "/dev/fb0";

	int fd = open(name, O_RDONLY);
	int drmfd = drmOpen(driver, NULL);

	if (fd == -1) {
		printf("open %s : %s\n", name, strerror(errno));
		return 1;
	}

	if (drmfd < 0) {
		printf("drmOpen failed\n");
		return 1;
	}

	memset(&var, 0, sizeof(struct fb_var_screeninfo));
	memset(&fix, 0, sizeof(struct fb_fix_screeninfo));

	if (ioctl(fd, FBIOGET_VSCREENINFO, &var))
		printf("var  %s\n", strerror(errno));
	if	(ioctl(fd, FBIOGET_FSCREENINFO, &fix))
		printf("fix %s\n", strerror(errno));

	setMode(&var);
	printf("pan: 0, 0\n");
	pan(fd, &var, 0, 0);
	sleep(2);
	printf("pan: 100, 0\n");
	pan(fd, &var, 100, 0);
	sleep(2);
	printf("pan: 0, 100\n");
	pan(fd, &var, 0, 100);
	sleep(2);
	printf("pan: 100, 100\n");
	pan(fd, &var, 100, 100);
	sleep(2);
	printf("pan: 0, 0\n");
	pan(fd, &var, 0, 0);
	sleep(2);

	printf("cursor\n");
	cursor(fd, drmfd);
	return 0;
}

void pan(int fd, struct fb_var_screeninfo *var, int x, int y)
{
	var->xoffset = x;
	var->yoffset = y;

	var->activate = FB_ACTIVATE_NOW;

	if (ioctl(fd, FBIOPUT_VSCREENINFO, var))
		printf("pan error: %s\n", strerror(errno));
}

/*
 * Cursor support removed from the fb kernel interface
 * using drm instead.
 */
void cursor(int fd, int drmfd)
{
	drmModeResPtr res = drmModeGetResources(drmfd);
	uint32_t crtc = res->crtcs[1]; /* select crtc here */
	drmBO bo;
	int ret;
	ret = drmBOCreate(drmfd, 64 * 64 * 4, 0, 0,
		DRM_BO_FLAG_READ |
		DRM_BO_FLAG_WRITE |
		DRM_BO_FLAG_MEM_VRAM |
		DRM_BO_FLAG_NO_EVICT,
		DRM_BO_HINT_DONT_FENCE, &bo);

	if (ret) {
		printf("failed to create buffer: %s\n", strerror(ret));
		return;
	}

	prettyCursor(drmfd, bo.handle, 0xFFFF00FF);
	drmModeSetCursor(drmfd, crtc, &bo, 64, 64);
	drmModeMoveCursor(drmfd, crtc, 0, 0);
	sleep(2);
	drmModeMoveCursor(drmfd, crtc, 40, 40);
	prettyCursor(drmfd, bo.handle, 0xFFFF0000);
	sleep(2);
	drmModeSetCursor(drmfd, crtc, 0, 0, 0);
	drmBOUnreference(drmfd, &bo);
}

void prettyCursor(int drmfd, unsigned int handle, unsigned int color)
{
	drmBO bo;
	unsigned int *ptr;
	int i;

	drmBOReference(drmfd, handle, &bo);
	drmBOMap(drmfd, &bo, DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE, 0, (void**)&ptr);

	for (i = 0; i < (64 * 64); i++)
		ptr[i] = color;

	drmBOUnmap(drmfd, &bo);
	drmBOUnreference(drmfd, &bo);
}

struct drm_mode
{
	int clock;
	int hdisplay;
	int hsync_start;
	int hsync_end;
	int htotal;
	int hskew;
	int vdisplay;
	int vsync_start;
	int vsync_end;
	int vtotal;
	int vscan;
	int vrefresh;
	int flags;
};

struct drm_mode mode =
{
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

void setMode(struct fb_var_screeninfo *var) {
	var->activate = FB_ACTIVATE_NOW;
	var->xres = mode.hdisplay;
	var->right_margin = mode.hsync_start - mode.hdisplay;
	var->hsync_len = mode.hsync_end - mode.hsync_start;
	var->left_margin = mode.htotal - mode.hsync_end;
	var->yres = mode.vdisplay;
	var->lower_margin = mode.vsync_start - mode.vdisplay;
	var->vsync_len = mode.vsync_end - mode.vsync_start;
	var->upper_margin = mode.vtotal - mode.vsync_end;
	var->pixclock = 10000000 / mode.htotal * 1000 / mode.vtotal * 100;
	/* avoid overflow */
	var->pixclock = var->pixclock * 1000 / mode.vrefresh;
}
