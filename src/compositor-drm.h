/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2015 Giulio Camuffo
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WESTON_COMPOSITOR_DRM_H
#define WESTON_COMPOSITOR_DRM_H

#include "compositor.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define WESTON_DRM_BACKEND_CONFIG_VERSION 1

enum weston_drm_backend_output_mode {
	/** The output is disabled */
	WESTON_DRM_BACKEND_OUTPUT_OFF,
	/** The output will use the current active mode */
	WESTON_DRM_BACKEND_OUTPUT_CURRENT,
	/** The output will use the preferred mode. A modeline can be provided
	 * by setting weston_backend_output_config::modeline in the form of
	 * "WIDTHxHEIGHT" or in the form of an explicit modeline calculated
	 * using e.g. the cvt tool. If a valid modeline is supplied it will be
	 * used, if invalid or NULL the preferred available mode will be used. */
	WESTON_DRM_BACKEND_OUTPUT_PREFERRED,
};

struct weston_drm_backend_output_config {
	struct weston_backend_output_config base;

	/** The pixel format to be used by the output. Valid values are:
	 * - NULL - The format set at backend creation time will be used;
	 * - "xrgb8888";
	 * - "rgb565"
	 * - "xrgb2101010"
	 */
	char *gbm_format;
	/** The seat to be used by the output. Set to NULL to use the
	 * default seat. */
	char *seat;
	/** The modeline to be used by the output. Refer to the documentation
	 * of WESTON_DRM_BACKEND_OUTPUT_PREFERRED for details. */
	char *modeline;
};

/** The backend configuration struct.
 *
 * weston_drm_backend_config contains the configuration used by a DRM
 * backend.
 */
struct weston_drm_backend_config {
	struct weston_backend_config base;

	/** The connector id of the output to be initialized.
	 *
	 * A value of 0 will enable all available outputs.
	 */
	int connector;

	/** The tty to be used. Set to 0 to use the current tty. */
	int tty;

	/** Whether to use the pixman renderer instead of the OpenGL ES renderer. */
	bool use_pixman;

	/** The seat to be used for input and output.
	 *
	 * If NULL the default "seat0" will be used.  The backend will
	 * take ownership of the seat_id pointer and will free it on
	 * backend destruction.
	 */
	char *seat_id;

	/** The pixel format of the framebuffer to be used.
	 *
	 * Valid values are:
	 * - NULL - The default format ("xrgb8888") will be used;
	 * - "xrgb8888";
	 * - "rgb565"
	 * - "xrgb2101010"
	 * The backend will take ownership of the format pointer and will free
	 * it on backend destruction.
	 */
	char *gbm_format;

	/** Callback used to configure the outputs.
	 *
	 * This function will be called by the backend when a new DRM
	 * output needs to be configured.
	 */
	enum weston_drm_backend_output_mode
		(*configure_output)(struct weston_compositor *compositor,
				    bool use_current_mode,
				    const char *name,
				    struct weston_drm_backend_output_config *output_config);
	bool use_current_mode;
};

#ifdef  __cplusplus
}
#endif

#endif /* WESTON_COMPOSITOR_DRM_H */
