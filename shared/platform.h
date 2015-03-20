/*
 * Copyright © 2015 Collabora, Ltd.
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

#ifndef WESTON_PLATFORM_H
#define WESTON_PLATFORM_H

#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef EGL_EXT_platform_base
static PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display_ext = NULL;
static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC create_platform_window_surface_ext = NULL;

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif
#endif /* EGL_EXT_platform_base */

static inline void
weston_platform_get_egl_proc_addresses(void)
{
#ifdef EGL_EXT_platform_base
	if (!get_platform_display_ext) {
		const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

		if (strstr(extensions, "EGL_EXT_platform_wayland")
		    || strstr(extensions, "EGL_KHR_platform_wayland")) {
			get_platform_display_ext =
				(void *) eglGetProcAddress("eglGetPlatformDisplayEXT");
			create_platform_window_surface_ext =
				(void *) eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
		}
	}
#endif
}

static inline EGLDisplay
weston_platform_get_egl_display(EGLenum platform, void *native_display,
				const EGLint *attrib_list)
{
#ifdef EGL_EXT_platform_base
	if (!get_platform_display_ext)
		weston_platform_get_egl_proc_addresses();

	if (get_platform_display_ext)
		return get_platform_display_ext(platform,
						native_display, attrib_list);
	else
#endif
		return eglGetDisplay((EGLNativeDisplayType) native_display);
}

static inline EGLSurface
weston_platform_create_egl_window(EGLDisplay dpy, EGLConfig config,
				  void *native_window,
				  const EGLint *attrib_list)
{
#ifdef EGL_EXT_platform_base
	if (!create_platform_window_surface_ext)
		weston_platform_get_egl_proc_addresses();

	if (create_platform_window_surface_ext)
		return create_platform_window_surface_ext(dpy, config,
							  native_window,
							  attrib_list);
#endif

	return eglCreateWindowSurface(dpy, config,
				      (EGLNativeWindowType) native_window,
				      attrib_list);
}

#ifdef  __cplusplus
}
#endif

#endif /* WESTON_PLATFORM_H */
