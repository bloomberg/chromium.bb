/*
 * Copyright © 2012 John Kåre Alsaker
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

#include "config.h"

#include "compositor.h"

#ifdef ENABLE_EGL

#include <EGL/egl.h>

#else

typedef int EGLint;
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef intptr_t EGLNativeDisplayType;
typedef intptr_t EGLNativeWindowType;
#define EGL_DEFAULT_DISPLAY NULL

#endif

struct gl_renderer_interface {
	const EGLint *opaque_attribs;
	const EGLint *alpha_attribs;

	int (*create)(struct weston_compositor *ec,
		      EGLNativeDisplayType display,
		      const EGLint *attribs,
		      const EGLint *visual_id);

	EGLDisplay (*display)(struct weston_compositor *ec);

	int (*output_create)(struct weston_output *output,
			     EGLNativeWindowType window);

	void (*output_destroy)(struct weston_output *output);

	EGLSurface (*output_surface)(struct weston_output *output);

	void (*set_border)(struct weston_compositor *ec,
			   int32_t width, int32_t height,
			   void *data, int32_t *edges);

	void (*print_egl_error_state)(void);
};

struct gl_renderer_interface gl_renderer_interface;
