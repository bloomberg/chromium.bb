// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface_egl.h"

#include "build/build_config.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/gl/egl_util.h"

// This header must come after the above third-party include, as
// it brings in #defines that cause conflicts.
#include "ui/gfx/gl/gl_bindings.h"

#if defined(USE_X11) && !defined(USE_WAYLAND)
extern "C" {
#include <X11/Xlib.h>
}
#endif

#if defined(USE_WAYLAND)
#include "ui/wayland/wayland_display.h"
#endif

namespace gfx {

namespace {
EGLConfig g_config;
EGLDisplay g_display;
EGLNativeDisplayType g_native_display;
EGLConfig g_software_config;
EGLDisplay g_software_display;
EGLNativeDisplayType g_software_native_display;
}

GLSurfaceEGL::GLSurfaceEGL() : software_(false) {
}

GLSurfaceEGL::~GLSurfaceEGL() {
}

bool GLSurfaceEGL::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

#if defined(USE_WAYLAND)
  g_native_display = ui::WaylandDisplay::Connect(NULL)->display();
#elif defined(USE_X11)
  g_native_display = XOpenDisplay(NULL);
#else
  g_native_display = EGL_DEFAULT_DISPLAY;
#endif
  g_display = eglGetDisplay(g_native_display);
  if (!g_display) {
    LOG(ERROR) << "eglGetDisplay failed with error " << GetLastEGLErrorString();
    return false;
  }

  if (!eglInitialize(g_display, NULL, NULL)) {
    LOG(ERROR) << "eglInitialize failed with error " << GetLastEGLErrorString();
    return false;
  }

  // Choose an EGL configuration.
  static const EGLint kConfigAttribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_ALPHA_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT
#if defined(USE_WAYLAND)
    | EGL_PIXMAP_BIT,
#else
    | EGL_PBUFFER_BIT,
#endif
    EGL_NONE
  };

  EGLint num_configs;
  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       NULL,
                       0,
                       &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  if (num_configs == 0) {
    LOG(ERROR) << "No suitable EGL configs found.";
    return false;
  }

  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       &g_config,
                       1,
                       &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  initialized = true;

#if defined(USE_X11)
  return true;
#else
  g_software_native_display = EGL_SOFTWARE_DISPLAY_ANGLE;
#endif
  g_software_display = eglGetDisplay(g_software_native_display);
  if (!g_software_display) {
    return true;
  }

  if (!eglInitialize(g_software_display, NULL, NULL)) {
    return true;
  }

  if (!eglChooseConfig(g_software_display,
                       kConfigAttribs,
                       NULL,
                       0,
                       &num_configs)) {
    g_software_display = NULL;
    return true;
  }

  if (num_configs == 0) {
    g_software_display = NULL;
    return true;
  }

  if (!eglChooseConfig(g_software_display,
                       kConfigAttribs,
                       &g_software_config,
                       1,
                       &num_configs)) {
    g_software_display = NULL;
    return false;
  }

  return true;
}

EGLDisplay GLSurfaceEGL::GetDisplay() {
  return software_ ? g_software_display : g_display;
}

EGLConfig GLSurfaceEGL::GetConfig() {
  return software_ ? g_software_config : g_config;
}

EGLDisplay GLSurfaceEGL::GetHardwareDisplay() {
  return g_display;
}

EGLDisplay GLSurfaceEGL::GetSoftwareDisplay() {
  return g_software_display;
}

EGLNativeDisplayType GLSurfaceEGL::GetNativeDisplay() {
  return g_native_display;
}

NativeViewGLSurfaceEGL::NativeViewGLSurfaceEGL(bool software,
                                               gfx::PluginWindowHandle window)
    : window_(window),
      surface_(NULL)
{
  software_ = software;
}

NativeViewGLSurfaceEGL::~NativeViewGLSurfaceEGL() {
  Destroy();
}

bool NativeViewGLSurfaceEGL::Initialize() {
  DCHECK(!surface_);

  if (!GetDisplay()) {
    LOG(ERROR) << "Trying to create surface with invalid display.";
    return false;
  }

  // Create a surface for the native window.
  surface_ = eglCreateWindowSurface(GetDisplay(),
                                    GetConfig(),
                                    window_,
                                    NULL);

  if (!surface_) {
    LOG(ERROR) << "eglCreateWindowSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  return true;
}

void NativeViewGLSurfaceEGL::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

bool NativeViewGLSurfaceEGL::IsOffscreen() {
  return false;
}

bool NativeViewGLSurfaceEGL::SwapBuffers() {
  if (!eglSwapBuffers(GetDisplay(), surface_)) {
    VLOG(1) << "eglSwapBuffers failed with error "
            << GetLastEGLErrorString();
    return false;
  }

  return true;
}

gfx::Size NativeViewGLSurfaceEGL::GetSize() {
  EGLint width;
  EGLint height;
  if (!eglQuerySurface(GetDisplay(), surface_, EGL_WIDTH, &width) ||
      !eglQuerySurface(GetDisplay(), surface_, EGL_HEIGHT, &height)) {
    NOTREACHED() << "eglQuerySurface failed with error "
                 << GetLastEGLErrorString();
    return gfx::Size();
  }

  return gfx::Size(width, height);
}

EGLSurface NativeViewGLSurfaceEGL::GetHandle() {
  return surface_;
}

PbufferGLSurfaceEGL::PbufferGLSurfaceEGL(bool software, const gfx::Size& size)
    : size_(size),
      surface_(NULL) {
  software_ = software;
}

PbufferGLSurfaceEGL::~PbufferGLSurfaceEGL() {
  Destroy();
}

bool PbufferGLSurfaceEGL::Initialize() {
  DCHECK(!surface_);

  if (!GetDisplay()) {
    LOG(ERROR) << "Trying to create surface with invalid display.";
    return false;
  }

  const EGLint pbuffer_attribs[] = {
    EGL_WIDTH, size_.width(),
    EGL_HEIGHT, size_.height(),
    EGL_NONE
  };

  surface_ = eglCreatePbufferSurface(GetDisplay(),
                                     GetConfig(),
                                     pbuffer_attribs);
  if (!surface_) {
    LOG(ERROR) << "eglCreatePbufferSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  return true;
}

void PbufferGLSurfaceEGL::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

bool PbufferGLSurfaceEGL::IsOffscreen() {
  return true;
}

bool PbufferGLSurfaceEGL::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a PbufferGLSurfaceEGL.";
  return false;
}

gfx::Size PbufferGLSurfaceEGL::GetSize() {
  return size_;
}

EGLSurface PbufferGLSurfaceEGL::GetHandle() {
  return surface_;
}

}  // namespace gfx
