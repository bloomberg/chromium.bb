/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Tiago Vignatti
 */
/*
 * \file setbacklight.c
 * Test program to get a backlight connector and set its brightness value.
 * Queries for the connectors id can be performed using drm/tests/modeprint
 * program.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "libbacklight.h"

static uint32_t
get_drm_connector_type(struct udev_device *drm_device, uint32_t connector_id)
{
	const char *filename;
	int fd, i, connector_type;
	drmModeResPtr res;
	drmModeConnectorPtr connector;

	filename = udev_device_get_devnode(drm_device);
	fd = open(filename, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		printf("couldn't open drm_device\n");
		return -1;
	}

	res = drmModeGetResources(fd);
	if (res == 0) {
		printf("Failed to get resources from card\n");
		close(fd);
		return -1;
	}

	for (i = 0; i < res->count_connectors; i++) {
		connector = drmModeGetConnector(fd, res->connectors[i]);
		if (!connector)
			continue;

		if ((connector->connection == DRM_MODE_DISCONNECTED) ||
		    (connector->connector_id != connector_id)) {
			drmModeFreeConnector(connector);
			continue;
		}

		connector_type = connector->connector_type;
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);

		close(fd);
		return connector_type;
	}

	close(fd);
	drmModeFreeResources(res);
	return -1;
}

/* returns a value between 0-255 range, where higher is brighter */
static uint32_t
get_normalized_backlight(struct backlight *backlight)
{
	long brightness, max_brightness;
	long norm;

	brightness = backlight_get_brightness(backlight);
	max_brightness = backlight_get_max_brightness(backlight);

	/* convert it to a scale of 0 to 255 */
	norm = (brightness * 255)/(max_brightness);

	return (int) norm;
}

static void
set_backlight(struct udev_device *drm_device, int connector_id, int blight)
{
	int connector_type;
	long max_brightness, brightness, actual_brightness;
	struct backlight *backlight;
	long new_blight;

	connector_type = get_drm_connector_type(drm_device, connector_id);
	if (connector_type < 0)
		return;

	backlight = backlight_init(drm_device, connector_type);
	if (!backlight) {
		printf("backlight adjust failed\n");
		return;
	}

	max_brightness = backlight_get_max_brightness(backlight);
	printf("Max backlight: %ld\n", max_brightness);

	brightness = backlight_get_brightness(backlight);
	printf("Cached backlight: %ld\n", brightness);

	actual_brightness = backlight_get_actual_brightness(backlight);
	printf("Hardware backlight: %ld\n", actual_brightness);

	printf("normalized current brightness: %d\n",
	       get_normalized_backlight(backlight));

	/* denormalized value */
	new_blight = (blight * max_brightness) / 255;

	backlight_set_brightness(backlight, new_blight);
	printf("Setting brightness to: %ld (norm: %d)\n", new_blight, blight);

	backlight_destroy(backlight);
}

int
main(int argc, char **argv)
{
	int blight, connector_id;
	const char *path;
	struct udev *udev;
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	struct udev_device *drm_device;

	if (argc < 3) {
		printf("Please add connector_id and brightness values from 0-255\n");
		return 1;
	}

	connector_id = atoi(argv[1]);
	blight = atoi(argv[2]);

	udev = udev_new();
	if (udev == NULL) {
		printf("failed to initialize udev context\n");
		return 1;
	}

	e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "drm");
	udev_enumerate_add_match_sysname(e, "card[0-9]*");

	udev_enumerate_scan_devices(e);
	drm_device = NULL;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		drm_device = udev_device_new_from_syspath(udev, path);
		break;
	}

	if (drm_device == NULL) {
		printf("no drm device found\n");
		return 1;
	}

	set_backlight(drm_device, connector_id, blight);

	udev_device_unref(drm_device);
	return 0;
}
