
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

/* old functions to be replaced */
drmModeFBPtr createFB(int fd, drmModeResPtr res);
void testCursor(int fd, uint32_t crtc);
void prettyColors(int fd, unsigned int handle);
void prettyCursor(int fd, unsigned int handle, unsigned int color);

/* structs for the demo_driver */

struct demo_driver;

struct demo_screen
{
	/* drm stuff */
	drmBO buffer;
	drmModeFBPtr fb;
	drmModeCrtcPtr crtc;
	drmModeOutputPtr output;

	struct drm_mode_modeinfo *mode;

	/* virtual buffer */
	uint32_t virt_x;
	uint32_t virt_y;
	uint32_t pitch;

	/* parent */
	struct demo_driver *driver;
};

#define DEMO_MAX_SCREENS 4
#define MAX_FIND_OUTPUTS 8

struct demo_driver
{
	/* drm stuff */
	int fd;
	drmModeResPtr res;

	/* screens */
	size_t numScreens;
	struct demo_screen screens[DEMO_MAX_SCREENS];
};

struct demo_driver* demoCreateDriver(void);
void demoUpdateRes(struct demo_driver *driver);
int demoCreateScreens(struct demo_driver *driver);
void demoTakeDownScreen(struct demo_screen *screen);
int demoFindConnectedOutputs(struct demo_driver *driver, drmModeOutputPtr *out, size_t max_out);
drmModeCrtcPtr demoFindFreeCrtc(struct demo_driver *driver, drmModeOutputPtr output);
void demoPanScreen(struct demo_screen *screen, uint16_t x, uint16_t y);
/* yet to be implemented */
void demoMouseActivate(struct demo_screen *screen);
void demoMouseMove(struct demo_screen *screen, uint16_t x, uint16_t y);

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

int main(int argc, char **argv)
{
	struct demo_driver *driver;
	int num;
	int i;

	printf("starting demo\n");

	driver = demoCreateDriver();

	if (!driver) {
		printf("failed to create driver\n");
		return 1;
	}

	num = demoCreateScreens(driver);
	if (num < 1) {
		printf("no screens attached or an error occured\n");
		return 1;
	}
	printf("created %i screens\n", num);

	for (i = 0; i < num; i++) {
		prettyColors(driver->fd, driver->screens[i].fb->handle);
	}
	sleep(1);

	for (i = 0; i < num; i++) {
		printf("%i: 100 0\n", i);
		demoPanScreen(&driver->screens[i], 100, 0);
		sleep(1);

		printf("%i: 0 100\n", i);
		demoPanScreen(&driver->screens[i], 0, 100);
		sleep(1);

		printf("%i: 100 100\n", i);
		demoPanScreen(&driver->screens[i], 100, 100);
		sleep(1);

		printf("%i: 0 0\n", i);
		demoPanScreen(&driver->screens[i], 0, 1);
		sleep(1);
		testCursor(driver->fd, driver->screens[i].crtc->crtc_id);
	}

	sleep(2);
    printf("ok\n");
    return 0;
}

int demoCreateScreens(struct demo_driver *driver)
{
	drmModeOutputPtr out[MAX_FIND_OUTPUTS];
	int num;
	int num_screens = 0;
	struct demo_screen *screen;
	int ret = 0;
	int i;

	num = demoFindConnectedOutputs(driver, out, MAX_FIND_OUTPUTS);
	if (num < 0)
		return 0;

	printf("found %i connected outputs\n", num);

	for (i = 0; i < num; i++) {
		screen = &driver->screens[i];

		screen->crtc = demoFindFreeCrtc(driver, out[i]);
		if (!screen->crtc) {
			printf("found no free crtc for output\n");
			drmModeFreeOutput(out[i]);
			continue;
		}

		screen->fb = createFB(driver->fd, driver->res);
		if (!screen->fb) {
			drmModeFreeOutput(out[i]);
			drmModeFreeCrtc(screen->crtc);
			screen->crtc = 0;
			printf("could not create framebuffer\n");
			continue;
		}

		screen->virt_x = SIZE_X;
		screen->virt_y = SIZE_Y;
		screen->pitch = PITCH;

		screen->output = out[i];
		screen->mode = &mode;
		screen->driver = driver;

		ret = drmModeSetCrtc(
			driver->fd,
			screen->crtc->crtc_id,
			screen->fb->buffer_id,
			0, 0,
			&screen->output->output_id, 1,
			screen->mode);

		if (ret) {
			printf("failed to set mode\n");
			demoTakeDownScreen(screen);
		} else {
			num_screens++;
		}

		demoUpdateRes(driver);
	}

	return num_screens;
}

void demoTakeDownScreen(struct demo_screen *screen)
{
	/* TODO Unrefence the BO */
	/* TODO Destroy FB */
	/* TODO take down the mode */

	drmModeFreeOutput(screen->output);
	drmModeFreeCrtc(screen->crtc);
}

drmModeCrtcPtr demoFindFreeCrtc(struct demo_driver *driver, drmModeOutputPtr output)
{
	drmModeCrtcPtr crtc;
	int i, j, used = 0;
	drmModeResPtr res = driver->res;


	for (i = 0; i < res->count_crtcs; i++) {
		used = 0;
		for (j = 0; j < DEMO_MAX_SCREENS; j++) {
			crtc = driver->screens[j].crtc;

			if (crtc && crtc->crtc_id == res->crtcs[i])
				used = 1;
		}

		if (!used) {
			crtc = drmModeGetCrtc(driver->fd, res->crtcs[i]);
			break;
		} else {
			crtc = 0;
		}
	}

	return crtc;
}

struct demo_driver* demoCreateDriver(void)
{
	struct demo_driver* driver = malloc(sizeof(struct demo_driver));

	memset(driver, 0, sizeof(struct demo_driver));

	driver->fd = drmOpenControl(0);

	if (driver->fd < 0) {
		printf("Failed to open the card fb\n");
		goto err_driver;
	}

	demoUpdateRes(driver);
	if (!driver->res) {
		printf("could not retrive resources\n");
		goto err_res;
	}

	return driver;

err_res:
	drmClose(driver->fd);
err_driver:
	free(driver);
	return NULL;
}

void demoUpdateRes(struct demo_driver *driver)
{
	if (driver->res)
		drmModeFreeResources(driver->res);

	driver->res = drmModeGetResources(driver->fd);

	if (!driver->res)
		printf("failed to get resources from kernel\n");
}

int demoFindConnectedOutputs(struct demo_driver *driver, drmModeOutputPtr *out, size_t max_out)
{
	int count = 0;
	int i;
	int fd = driver->fd;
	drmModeResPtr res = driver->res;

	drmModeOutputPtr output;

	for (i = 0; i < res->count_outputs && count < max_out; i++) {
		output = drmModeGetOutput(fd, res->outputs[i]);

		if (!output)
			continue;

		if (output->connection != DRM_MODE_CONNECTED) {
			drmModeFreeOutput(output);
			continue;
		}

		out[count++] = output;
	}

	return count;
}

void demoPanScreen(struct demo_screen *screen, uint16_t x, uint16_t y)
{
	drmModeSetCrtc(
		screen->driver->fd,
		screen->crtc->crtc_id,
		screen->fb->buffer_id,
		x, y,
		&screen->output->output_id, 1,
		screen->mode);
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

	if (ret) {
		printf("failed to create framebuffer (ret %d)\n",ret);
		goto err;
	}

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
	return 0;
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

void testCursor(int fd, uint32_t crtc)
{
	drmBO bo;
	int ret;
	ret = drmBOCreate(fd, 64 * 64 * 4, 0, 0,
		DRM_BO_FLAG_READ |
		DRM_BO_FLAG_WRITE |
		DRM_BO_FLAG_MEM_VRAM |
		DRM_BO_FLAG_NO_EVICT,
		DRM_BO_HINT_DONT_FENCE, &bo);

	prettyCursor(fd, bo.handle, 0xFFFF00FF);
	printf("set cursor\n");
	drmModeSetCursor(fd, crtc, &bo, 64, 64);
	printf("move cursor 0, 0\n");
	drmModeMoveCursor(fd, crtc, 0, 0);
	sleep(1);
	prettyCursor(fd, bo.handle, 0xFFFF0000);
	printf("move cursor 40, 40\n");
	drmModeMoveCursor(fd, crtc, 40, 40);
	sleep(1);
	printf("move cursor 100, 100\n");
	drmModeMoveCursor(fd, crtc, 100, 100);
	sleep(1);
	drmModeSetCursor(fd, crtc, NULL, 0, 0);
}

void prettyCursor(int fd, unsigned int handle, unsigned int color)
{
	drmBO bo;
	unsigned int *ptr;
	int i;

	drmBOReference(fd, handle, &bo);
	drmBOMap(fd, &bo, DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE, 0, (void**)&ptr);

	for (i = 0; i < (64 * 64); i++)
		ptr[i] = color;

	drmBOUnmap(fd, &bo);
	drmBOUnreference(fd, &bo);
}
