// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(USE_EGL)
#include <EGL/egl.h>
#endif

#include "ui/gl/gl_bindings.h"

#if defined(USE_GLX)
#include "ui/gfx/x/x11_types.h"
#endif

#if defined(OS_WIN)
#include "ui/gl/gl_surface_wgl.h"
#endif

#if defined(USE_EGL)
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gl {

#if defined(OS_WIN)
std::string DriverWGL::GetPlatformExtensions() {
  const char* str = nullptr;
  str = wglGetExtensionsStringARB(GLSurfaceWGL::GetDisplayDC());
  if (str)
    return str;
  return wglGetExtensionsStringEXT();
}
#endif

#if defined(USE_EGL)
std::string DriverEGL::GetPlatformExtensions() {
  EGLDisplay display = GLSurfaceEGL::GetHardwareDisplay();
  if (display == EGL_NO_DISPLAY)
    return "";
  const char* str = eglQueryString(display, EGL_EXTENSIONS);
  return str ? std::string(str) : "";
}

void DriverEGL::UpdateConditionalExtensionBindings() {
  // For the moment, only two extensions can be conditionally disabled
  // through GPU driver bug workarounds mechanism:
  //   EGL_KHR_fence_sync
  //   EGL_KHR_wait_sync

  // In theory it's OK to allow disabling other EGL extensions, as far as they
  // are not the ones used in GLSurfaceEGL::InitializeOneOff().

  std::string extensions(GetPlatformExtensions());
  extensions += " ";

  ext.b_EGL_KHR_fence_sync =
      extensions.find("EGL_KHR_fence_sync ") != std::string::npos;
  ext.b_EGL_KHR_wait_sync =
      extensions.find("EGL_KHR_wait_sync ") != std::string::npos;
  if (!ext.b_EGL_KHR_wait_sync) {
    fn.eglWaitSyncKHRFn = nullptr;
  }
}

// static
std::string DriverEGL::GetClientExtensions() {
  const char* str = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  return str ? std::string(str) : "";
}
#endif

#if defined(USE_GLX)
std::string DriverGLX::GetPlatformExtensions() {
  Display* display = gfx::GetXDisplay();
  const int screen = (display == EGL_NO_DISPLAY ? 0 : DefaultScreen(display));
  const char* str = glXQueryExtensionsString(display, screen);
  return str ? std::string(str) : "";
}
#endif

}  // namespace gl
