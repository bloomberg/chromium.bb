#ifndef LIBBACKLIGHT_H
#define LIBBACKLIGHT_H
#include <libudev.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

enum backlight_type {
	BACKLIGHT_RAW,
	BACKLIGHT_PLATFORM,
	BACKLIGHT_FIRMWARE
};

struct backlight {
	char *path;
	int max_brightness;
	int brightness;
	enum backlight_type type;
};

/*
 * Find and set up a backlight for a valid udev connector device, i.e. one
 * matching drm subsytem and with status of connected.
 */
struct backlight *backlight_init(struct udev_device *drm_device,
				 uint32_t connector_type);

/* Free backlight resources */
void backlight_destroy(struct backlight *backlight);

/* Provide the maximum backlight value */
long backlight_get_max_brightness(struct backlight *backlight);

/* Provide the cached backlight value */
long backlight_get_brightness(struct backlight *backlight);

/* Provide the hardware backlight value */
long backlight_get_actual_brightness(struct backlight *backlight);

/* Set the backlight to a value between 0 and max */
long backlight_set_brightness(struct backlight *backlight, long brightness);

#ifdef __cplusplus
}
#endif

#endif /* LIBBACKLIGHT_H */
