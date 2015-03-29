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

#ifdef ENABLE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef ENABLE_EGL

#ifndef EGL_EXT_platform_base
typedef EGLDisplay (*PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform,
						       void *native_display,
						       const EGLint *attrib_list);
typedef EGLSurface (*PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC) (EGLDisplay dpy,
								EGLConfig config,
								void *native_window,
								const EGLint *attrib_list);
#endif

static inline void *
weston_platform_get_egl_proc_address(const char *address)
{
	const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

	if (extensions
	    && (strstr(extensions, "EGL_EXT_platform_wayland")
	        || strstr(extensions, "EGL_KHR_platform_wayland"))) {
		return (void *) eglGetProcAddress(address);
	}

	return NULL;
}

static inline EGLDisplay
weston_platform_get_egl_display(EGLenum platform, void *native_display,
				const EGLint *attrib_list)
{
	static PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;

	if (!get_platform_display) {
		get_platform_display = weston_platform_get_egl_proc_address(
			"eglGetPlatformDisplayEXT");
	}

	if (get_platform_display)
		return get_platform_display(platform,
					    native_display, attrib_list);

	return eglGetDisplay((EGLNativeDisplayType) native_display);
}

static inline EGLSurface
weston_platform_create_egl_surface(EGLDisplay dpy, EGLConfig config,
				   void *native_window,
				   const EGLint *attrib_list)
{
	static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC
		create_platform_window = NULL;

	if (!create_platform_window) {
		create_platform_window = weston_platform_get_egl_proc_address(
				"eglCreatePlatformWindowSurfaceEXT");
	}

	if (create_platform_window)
		return create_platform_window(dpy, config,
					      native_window,
					      attrib_list);

	return eglCreateWindowSurface(dpy, config,
				      (EGLNativeWindowType) native_window,
				      attrib_list);
}

#else /* ENABLE_EGL */

static inline void *
weston_platform_get_egl_display(void *platform, void *native_display,
				const int *attrib_list)
{
	return NULL;
}

static inline void *
weston_platform_create_egl_surface(void *dpy, void *config,
				   void *native_window,
				   const int *attrib_list)
{
	return NULL;
}
#endif /* ENABLE_EGL */

#ifdef  __cplusplus
}
#endif

#endif /* WESTON_PLATFORM_H */
