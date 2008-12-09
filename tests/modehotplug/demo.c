
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include "xf86drm.h"
#include "xf86drmMode.h"

/* structs for the demo_driver */

#define DEMO_MAX_OUTPUTS 8

struct demo_driver
{
	/* drm stuff */
	int fd;
	drmModeResPtr res;
	uint32_t counter;

	drmModeOutputPtr outputs[DEMO_MAX_OUTPUTS];
};

struct demo_driver* demoCreateDriver(void);
void demoUpdateRes(struct demo_driver *driver);

void demoPopulateOutputs(struct demo_driver *driver);
void demoHotplug(struct demo_driver *driver);

const char* demoGetStatus(drmModeOutputPtr out);

int main(int argc, char **argv)
{
	struct demo_driver *driver;
	uint32_t temp;
	int i;

	printf("starting demo\n");

	driver = demoCreateDriver();

	if (!driver) {
		printf("failed to create driver\n");
		return 1;
	}

	driver->counter = drmModeGetHotplug(driver->fd);
	demoPopulateOutputs(driver);
	while (driver->counter != (temp = drmModeGetHotplug(driver->fd))) {
		demoPopulateOutputs(driver);
		driver->counter = temp;
	}

	for (i = 0; i < driver->res->count_outputs && i < DEMO_MAX_OUTPUTS; i++) {
		printf("Output %u is %s\n",
			driver->outputs[i]->output_id,
			demoGetStatus(driver->outputs[i]));
	}

	while(1) {
		usleep(100000);
		temp = drmModeGetHotplug(driver->fd);
		if (temp == driver->counter)
			continue;

		demoHotplug(driver);
		driver->counter = temp;
	}

	return 0;
}

const char* demoGetStatus(drmModeOutputPtr output)
{
	switch (output->connection) {
		case DRM_MODE_CONNECTED:
			return "connected";
		case DRM_MODE_DISCONNECTED:
			return "disconnected";
		default:
			return "unknown";
	}
}

void demoHotplug(struct demo_driver *driver)
{
	drmModeResPtr res = driver->res;
	int i;
	drmModeOutputPtr temp, current;

	for (i = 0; i < res->count_outputs && i < DEMO_MAX_OUTPUTS; i++) {
		temp = drmModeGetOutput(driver->fd, res->outputs[i]);
		current = driver->outputs[i];

		if (temp->connection != current->connection) {
			printf("Output %u became %s was %s\n",
				temp->output_id,
				demoGetStatus(temp),
				demoGetStatus(current));
		}

		drmModeFreeOutput(current);
		driver->outputs[i] = temp;
	}
}

void demoPopulateOutputs(struct demo_driver *driver)
{
	drmModeResPtr res = driver->res;
	int i;

	for (i = 0; i < res->count_outputs && i < DEMO_MAX_OUTPUTS; i++) {
		drmModeFreeOutput(driver->outputs[i]);
		driver->outputs[i] = drmModeGetOutput(driver->fd, res->outputs[i]);
	}
}

struct demo_driver* demoCreateDriver(void)
{
	struct demo_driver* driver = malloc(sizeof(struct demo_driver));

	memset(driver, 0, sizeof(struct demo_driver));

	driver->fd = drmOpen("i915", NULL);

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
