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
 */

#include "compositor.h"

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

static void
repaint_surfaces(struct weston_output *output, pixman_region32_t *damage)
{
	struct weston_compositor *compositor = output->compositor;
	struct weston_surface *surface;

	wl_list_for_each_reverse(surface, &compositor->surface_list, link)
		if (surface->plane == &compositor->primary_plane)
			weston_surface_draw(surface, output, damage);
}

WL_EXPORT void
gles2_renderer_repaint_output(struct weston_output *output,
			      pixman_region32_t *output_damage)
{
	struct weston_compositor *compositor = output->compositor;
	EGLBoolean ret;
	static int errored;
	int32_t width, height;

	width = output->current->width +
		output->border.left + output->border.right;
	height = output->current->height +
		output->border.top + output->border.bottom;

	glViewport(0, 0, width, height);

	ret = eglMakeCurrent(compositor->egl_display, output->egl_surface,
			     output->egl_surface, compositor->egl_context);
	if (ret == EGL_FALSE) {
		if (errored)
			return;
		errored = 1;
		weston_log("Failed to make EGL context current.\n");
		print_egl_error_state();
		return;
	}

	/* if debugging, redraw everything outside the damage to clean up
	 * debug lines from the previous draw on this buffer:
	 */
	if (compositor->fan_debug) {
		pixman_region32_t undamaged;
		pixman_region32_init(&undamaged);
		pixman_region32_subtract(&undamaged, &output->region,
					 output_damage);
		compositor->fan_debug = 0;
		repaint_surfaces(output, &undamaged);
		compositor->fan_debug = 1;
		pixman_region32_fini(&undamaged);
	}

	repaint_surfaces(output, output_damage);

	wl_signal_emit(&output->frame_signal, output);

	ret = eglSwapBuffers(compositor->egl_display, output->egl_surface);
	if (ret == EGL_FALSE && !errored) {
		errored = 1;
		weston_log("Failed in eglSwapBuffers.\n");
		print_egl_error_state();
	}

}
