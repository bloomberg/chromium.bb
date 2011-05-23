// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface_egl.h"

#include "build/build_config.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/angle/include/EGL/egl.h"
#include "ui/gfx/gl/egl_util.h"

// This header must come after the above third-party include, as
// it brings in #defines that cause conflicts.
#include "ui/gfx/gl/gl_bindings.h"

#if defined(OS_LINUX)
extern "C" {
#include <X11/Xlib.h>
}
#endif

namespace gfx {

namespace {
EGLConfig g_config;
EGLDisplay g_display;
}

GLSurfaceEGL::GLSurfaceEGL() {
}

GLSurfaceEGL::~GLSurfaceEGL() {
}

bool GLSurfaceEGL::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

#ifdef OS_LINUX
  EGLNativeDisplayType native_display = XOpenDisplay(NULL);
#else
  EGLNativeDisplayType native_display = EGL_DEFAULT_DISPLAY;
#endif
  g_display = eglGetDisplay(native_display);
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
    EGL_DEPTH_SIZE, 16,
    EGL_STENCIL_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
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

  scoped_array<EGLConfig> configs(new EGLConfig[num_configs]);
  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       configs.get(),
                       num_configs,
                       &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  g_config = configs[0];

  initialized = true;
  return true;
}

EGLDisplay GLSurfaceEGL::GetDisplay() {
  return g_display;
}

EGLConfig GLSurfaceEGL::GetConfig() {
  return g_config;
}

NativeViewGLSurfaceEGL::NativeViewGLSurfaceEGL(gfx::PluginWindowHandle window)
    : window_(window),
      surface_(NULL)
{
}

NativeViewGLSurfaceEGL::~NativeViewGLSurfaceEGL() {
  Destroy();
}

bool NativeViewGLSurfaceEGL::Initialize() {
  DCHECK(!surface_);

  // Create a surface for the native window.
  surface_ = eglCreateWindowSurface(g_display,
                                    g_config,
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
    if (!eglDestroySurface(g_display, surface_)) {
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
  if (!eglSwapBuffers(g_display, surface_)) {
    VLOG(1) << "eglSwapBuffers failed with error "
            << GetLastEGLErrorString();
    return false;
  }

  return true;
}

gfx::Size NativeViewGLSurfaceEGL::GetSize() {
  EGLint width;
  EGLint height;
  if (!eglQuerySurface(g_display, surface_, EGL_WIDTH, &width) ||
      !eglQuerySurface(g_display, surface_, EGL_HEIGHT, &height)) {
    NOTREACHED() << "eglQuerySurface failed with error "
                 << GetLastEGLErrorString();
    return gfx::Size();
  }

  return gfx::Size(width, height);
}

EGLSurface NativeViewGLSurfaceEGL::GetHandle() {
  return surface_;
}

PbufferGLSurfaceEGL::PbufferGLSurfaceEGL(const gfx::Size& size)
    : size_(size),
      surface_(NULL) {
}

PbufferGLSurfaceEGL::~PbufferGLSurfaceEGL() {
  Destroy();
}

bool PbufferGLSurfaceEGL::Initialize() {
  DCHECK(!surface_);

  const EGLint pbuffer_attribs[] = {
    EGL_WIDTH, size_.width(),
    EGL_HEIGHT, size_.height(),
    EGL_NONE
  };

  surface_ = eglCreatePbufferSurface(g_display,
                                     g_config,
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
    if (!eglDestroySurface(g_display, surface_)) {
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
