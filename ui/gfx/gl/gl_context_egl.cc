// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_egl.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "third_party/angle/include/EGL/egl.h"
#include "ui/gfx/gl/egl_util.h"
#include "ui/gfx/gl/gl_surface.h"

// This header must come after the above third-party include, as
// it brings in #defines that cause conflicts.
#include "ui/gfx/gl/gl_bindings.h"

#if defined(USE_X11)
extern "C" {
#include <X11/Xlib.h>
}
#endif

namespace gfx {

std::string GLContextEGL::GetExtensions() {
  const char* extensions = eglQueryString(display_,
                                          EGL_EXTENSIONS);
  if (!extensions)
    return GLContext::GetExtensions();

  return GLContext::GetExtensions() + " " + extensions;
}

GLContextEGL::GLContextEGL(GLShareGroup* share_group)
    : GLContext(share_group),
      context_(NULL),
      display_(NULL),
      config_(NULL) {
}

GLContextEGL::~GLContextEGL() {
  Destroy();
}

bool GLContextEGL::Initialize(
    GLSurface* compatible_surface, GpuPreference gpu_preference) {
  DCHECK(compatible_surface);
  DCHECK(!context_);

  static const EGLint kContextAttributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  display_ = compatible_surface->GetDisplay();
  config_ = compatible_surface->GetConfig();

  context_ = eglCreateContext(
      display_,
      config_,
      share_group() ? share_group()->GetHandle() : NULL,
      kContextAttributes);
  if (!context_) {
    LOG(ERROR) << "eglCreateContext failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  return true;
}

void GLContextEGL::Destroy() {
  if (context_) {
    if (!eglDestroyContext(display_, context_)) {
      LOG(ERROR) << "eglDestroyContext failed with error "
                 << GetLastEGLErrorString();
    }

    context_ = NULL;
  }
}

bool GLContextEGL::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
      return true;

  TRACE_EVENT0("gpu", "GLContextEGL::MakeCurrent");

  if (!eglMakeCurrent(display_,
                      surface->GetHandle(),
                      surface->GetHandle(),
                      context_)) {
    DVLOG(1) << "eglMakeCurrent failed with error "
             << GetLastEGLErrorString();
    return false;
  }

  SetCurrent(this, surface);
  if (!InitializeExtensionBindings()) {
    ReleaseCurrent(surface);
    return false;
  }

  if (!surface->OnMakeCurrent(this)) {
    LOG(ERROR) << "Could not make current.";
    return false;
  }

  return true;
}

void GLContextEGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(NULL, NULL);
  eglMakeCurrent(display_,
                 EGL_NO_SURFACE,
                 EGL_NO_SURFACE,
                 EGL_NO_CONTEXT);
}

bool GLContextEGL::IsCurrent(GLSurface* surface) {
  DCHECK(context_);

  bool native_context_is_current = context_ == eglGetCurrentContext();

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetCurrent() == this));

  if (!native_context_is_current)
    return false;

  if (surface) {
    if (surface->GetHandle() != eglGetCurrentSurface(EGL_DRAW))
      return false;
  }

  return true;
}

void* GLContextEGL::GetHandle() {
  return context_;
}

void GLContextEGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent(NULL));
  if (!eglSwapInterval(display_, interval)) {
    LOG(ERROR) << "eglSwapInterval failed with error "
               << GetLastEGLErrorString();
  }
}

}  // namespace gfx
