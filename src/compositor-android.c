/*
 * Copyright © 2012 Collabora, Ltd.
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "compositor.h"
#include "android-framebuffer.h"
#include "log.h"

struct android_compositor;

struct android_output {
	struct android_compositor *compositor;
	struct weston_output base;

	struct weston_mode mode;
	struct android_framebuffer *fb;
	EGLSurface egl_surface;
};

struct android_seat {
	struct weston_seat base;
};

struct android_compositor {
	struct weston_compositor base;

	struct android_seat *seat;
};

static inline struct android_output *
to_android_output(struct weston_output *base)
{
	return container_of(base, struct android_output, base);
}

static inline struct android_compositor *
to_android_compositor(struct weston_compositor *base)
{
	return container_of(base, struct android_compositor, base);
}

static const char *
egl_error_string(EGLint code)
{
#define MYERRCODE(x) case x: return #x;
	switch (code) {
	MYERRCODE(EGL_SUCCESS)
	MYERRCODE(EGL_NOT_INITIALIZED)
	MYERRCODE(EGL_BAD_ACCESS)
	MYERRCODE(EGL_BAD_ALLOC)
	MYERRCODE(EGL_BAD_ATTRIBUTE)
	MYERRCODE(EGL_BAD_CONTEXT)
	MYERRCODE(EGL_BAD_CONFIG)
	MYERRCODE(EGL_BAD_CURRENT_SURFACE)
	MYERRCODE(EGL_BAD_DISPLAY)
	MYERRCODE(EGL_BAD_SURFACE)
	MYERRCODE(EGL_BAD_MATCH)
	MYERRCODE(EGL_BAD_PARAMETER)
	MYERRCODE(EGL_BAD_NATIVE_PIXMAP)
	MYERRCODE(EGL_BAD_NATIVE_WINDOW)
	MYERRCODE(EGL_CONTEXT_LOST)
	default:
		return "unknown";
	}
#undef MYERRCODE
}

static void
print_egl_error_state(void)
{
	EGLint code;

	code = eglGetError();
	weston_log("EGL error state: %s (0x%04lx)\n",
		egl_error_string(code), (long)code);
}

static int
android_output_make_current(struct android_output *output)
{
	struct android_compositor *compositor = output->compositor;
	EGLBoolean ret;
	static int errored;

	ret = eglMakeCurrent(compositor->base.egl_display, output->egl_surface,
			     output->egl_surface, compositor->base.egl_context);
	if (ret == EGL_FALSE) {
		if (errored)
			return -1;
		errored = 1;
		weston_log("Failed to make EGL context current.\n");
		print_egl_error_state();
		return -1;
	}

	return 0;
}

static void
android_finish_frame(void *data)
{
	struct android_output *output = data;

	weston_output_finish_frame(&output->base,
				   weston_compositor_get_time());
}

static void
android_output_repaint(struct weston_output *base, pixman_region32_t *damage)
{
	struct android_output *output = to_android_output(base);
	struct android_compositor *compositor = output->compositor;
	struct weston_surface *surface;
	struct wl_event_loop *loop;
	EGLBoolean ret;
	static int errored;

	if (android_output_make_current(output) < 0)
		return;

	wl_list_for_each_reverse(surface, &compositor->base.surface_list, link)
		weston_surface_draw(surface, &output->base, damage);

	wl_signal_emit(&output->base.frame_signal, output);

	ret = eglSwapBuffers(compositor->base.egl_display, output->egl_surface);
	if (ret == EGL_FALSE && !errored) {
		errored = 1;
		weston_log("Failed in eglSwapBuffers.\n");
		print_egl_error_state();
	}

	/* FIXME: does Android have a way to signal page flip done? */
	loop = wl_display_get_event_loop(compositor->base.wl_display);
	wl_event_loop_add_idle(loop, android_finish_frame, output);
}

static void
android_output_destroy(struct weston_output *base)
{
	struct android_output *output = to_android_output(base);

	wl_list_remove(&output->base.link);
	weston_output_destroy(&output->base);

	android_framebuffer_destroy(output->fb);

	free(output);
}

static struct android_output *
android_output_create(struct android_compositor *compositor)
{
	struct android_output *output;

	output = calloc(1, sizeof *output);
	if (!output)
		return NULL;

	output->fb = android_framebuffer_create();
	if (!output->fb) {
		free(output);
		return NULL;
	}

	output->compositor = compositor;
	return output;
}

static void
android_compositor_add_output(struct android_compositor *compositor,
			      struct android_output *output)
{
	float mm_width, mm_height;

	output->base.repaint = android_output_repaint;
	output->base.destroy = android_output_destroy;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	/* only one static mode in list */
	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = output->fb->width;
	output->mode.height = output->fb->height;
	output->mode.refresh = ceilf(1000.0f * output->fb->refresh_rate);
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current = &output->mode;
	output->base.origin = &output->mode;
	output->base.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	output->base.make = "unknown";
	output->base.model = "unknown";

	mm_width  = output->fb->width / output->fb->xdpi * 25.4f;
	mm_height = output->fb->height / output->fb->ydpi * 25.4f;
	weston_output_init(&output->base, &compositor->base,
			   0, 0, round(mm_width), round(mm_height),
			   WL_OUTPUT_FLIPPED);
	wl_list_insert(compositor->base.output_list.prev, &output->base.link);
}

static void
android_seat_destroy(struct android_seat *seat)
{
	weston_seat_release(&seat->base);
	free(seat);
}

static struct android_seat *
android_seat_create(struct android_compositor *compositor)
{
	struct android_seat *seat;

	seat = calloc(1, sizeof *seat);
	if (!seat)
		return NULL;

	weston_seat_init(&seat->base, &compositor->base);
	compositor->base.seat = &seat->base;

	return seat;
}

static int
android_egl_choose_config(struct android_compositor *compositor,
			  struct android_framebuffer *fb,
			  const EGLint *attribs)
{
	EGLBoolean ret;
	EGLint count = 0;
	EGLint matched = 0;
	EGLConfig *configs;
	int i;

	/*
	 * The logic is copied from Android frameworks/base/services/
	 * surfaceflinger/DisplayHardware/DisplayHardware.cpp
	 */

	compositor->base.egl_config = NULL;

	ret = eglGetConfigs(compositor->base.egl_display, NULL, 0, &count);
	if (ret == EGL_FALSE || count < 1)
		return -1;

	configs = calloc(count, sizeof *configs);
	if (!configs)
		return -1;

	ret = eglChooseConfig(compositor->base.egl_display, attribs, configs,
			      count, &matched);
	if (ret == EGL_FALSE || matched < 1)
		goto out;

	for (i = 0; i < matched; ++i) {
		EGLint id;
		ret = eglGetConfigAttrib(compositor->base.egl_display,
					 configs[i], EGL_NATIVE_VISUAL_ID,
					 &id);
		if (ret == EGL_FALSE)
			continue;
		if (id > 0 && fb->format == id) {
			compositor->base.egl_config = configs[i];
			break;
		}
	}

out:
	free(configs);
	if (!compositor->base.egl_config)
		return -1;

	return 0;
}

static int
android_init_egl(struct android_compositor *compositor,
		 struct android_output *output)
{
	EGLint eglmajor, eglminor;
	int ret;

	static const EGLint context_attrs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const EGLint config_attrs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	compositor->base.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (compositor->base.egl_display == EGL_NO_DISPLAY) {
		weston_log("Failed to create EGL display.\n");
		print_egl_error_state();
		return -1;
	}

	ret = eglInitialize(compositor->base.egl_display, &eglmajor, &eglminor);
	if (!ret) {
		weston_log("Failed to initialise EGL.\n");
		print_egl_error_state();
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		weston_log("Failed to bind EGL_OPENGL_ES_API.\n");
		print_egl_error_state();
		return -1;
	}

	ret = android_egl_choose_config(compositor, output->fb, config_attrs);
	if (ret < 0) {
		weston_log("Failed to find an EGL config.\n");
		print_egl_error_state();
		return -1;
	}

	compositor->base.egl_context =
		eglCreateContext(compositor->base.egl_display,
				 compositor->base.egl_config,
				 EGL_NO_CONTEXT,
				 context_attrs);
	if (compositor->base.egl_context == EGL_NO_CONTEXT) {
		weston_log("Failed to create a GL ES 2 context.\n");
		print_egl_error_state();
		return -1;
	}

	output->egl_surface =
		eglCreateWindowSurface(compositor->base.egl_display,
				       compositor->base.egl_config,
				       output->fb->native_window,
				       NULL);
	if (output->egl_surface == EGL_NO_SURFACE) {
		weston_log("Failed to create FB EGLSurface.\n");
		print_egl_error_state();
		return -1;
	}

	if (android_output_make_current(output) < 0)
		return -1;

	return 0;
}

static void
android_fini_egl(struct android_compositor *compositor)
{
	eglMakeCurrent(compositor->base.egl_display,
		       EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	eglTerminate(compositor->base.egl_display);
	eglReleaseThread();
}

static void
android_compositor_destroy(struct weston_compositor *base)
{
	struct android_compositor *compositor = to_android_compositor(base);

	android_seat_destroy(compositor->seat);

	/* destroys outputs, too */
	weston_compositor_shutdown(&compositor->base);

	android_fini_egl(compositor);

	free(compositor);
}

static struct weston_compositor *
android_compositor_create(struct wl_display *display, int argc, char *argv[],
			  const char *config_file)
{
	struct android_compositor *compositor;
	struct android_output *output;

	compositor = calloc(1, sizeof *compositor);
	if (compositor == NULL)
		return NULL;

	compositor->base.destroy = android_compositor_destroy;

	compositor->base.focus = 1;

	/* FIXME: all cleanup on failure is missing */

	output = android_output_create(compositor);
	if (!output)
		return NULL;

	if (android_init_egl(compositor, output) < 0)
		return NULL;

	if (weston_compositor_init(&compositor->base, display, argc, argv,
				   config_file) < 0)
		return NULL;

	android_compositor_add_output(compositor, output);

	compositor->seat = android_seat_create(compositor);
	if (!compositor->seat)
		return NULL;

	return &compositor->base;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int argc, char *argv[],
	     const char *config_file)
{
	return android_compositor_create(display, argc, argv, config_file);
}
