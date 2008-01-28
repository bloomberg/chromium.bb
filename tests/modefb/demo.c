#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "linux/fb.h"
#include "sys/ioctl.h"


void setMode(struct fb_var_screeninfo *var);
void pan(int fd, struct fb_var_screeninfo *var, int x, int y);
void cursor(int fd);

int main(int argc, char **argv)
{
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	const char* name = "/dev/fb0";

	int fd = open(name, O_RDONLY);

	if (fd == -1) {
		printf("open %s : %s\n", name, strerror(errno));
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
	cursor(fd);
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
 * Currently isn't supported in the driver
 */
void cursor(int fd)
{
	struct fb_cursor cur;
	void *data = malloc(64 * 64 * 4);
	memset(&cur, 0, sizeof(cur));

	cur.set = FB_CUR_SETIMAGE | FB_CUR_SETPOS | FB_CUR_SETSIZE;
	cur.enable = 1;
	cur.image.dx = 1;
	cur.image.dy = 1;
	cur.image.width = 2;
	cur.image.height = 2;
	cur.image.depth = 32;
	cur.image.data = data;

	if (ioctl(fd, FBIO_CURSOR, &cur))
		printf("cursor error: %s\n", strerror(errno));

	sleep(2);

	memset(&cur, 0, sizeof(cur));
	cur.set = FB_CUR_SETPOS;
	cur.enable = 0;
	cur.image.dx = 100;
	cur.image.dy = 100;

	if (ioctl(fd, FBIO_CURSOR, &cur))
		printf("cursor error: %s\n", strerror(errno));

	free(data);
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
