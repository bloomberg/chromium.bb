// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the GLContextWGL and PbufferGLContext classes.

#include "ui/gfx/gl/gl_context_wgl.h"

#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_wgl.h"

namespace gfx {

GLContextWGL::GLContextWGL(GLShareGroup* share_group)
    : GLContext(share_group),
      context_(NULL) {
}

GLContextWGL::~GLContextWGL() {
  Destroy();
}

std::string GLContextWGL::GetExtensions() {
  if (wglGetExtensionsStringARB) {
    // TODO(apatrick): When contexts and surfaces are separated, we won't be
    // able to use surface_ here. Either use a display device context or the
    // surface that was passed to MakeCurrent.
    const char* extensions = wglGetExtensionsStringARB(
        GLSurfaceWGL::GetDisplay());
    if (extensions) {
      return GLContext::GetExtensions() + " " + extensions;
    }
  }

  return GLContext::GetExtensions();
}

bool GLContextWGL::Initialize(GLSurface* compatible_surface) {
  GLSurfaceWGL* surface_wgl = static_cast<GLSurfaceWGL*>(compatible_surface);

  // TODO(apatrick): When contexts and surfaces are separated, we won't be
  // able to use surface_ here. Either use a display device context or a
  // surface that the context is compatible with not necessarily limited to
  // rendering to.
  context_ = wglCreateContext(static_cast<HDC>(surface_wgl->GetHandle()));
  if (!context_) {
    LOG(ERROR) << "Failed to create GL context.";
    Destroy();
    return false;
  }

  if (share_group()) {
    HGLRC share_handle = static_cast<HGLRC>(share_group()->GetHandle());
    if (share_handle) {
      if (!wglShareLists(share_handle, context_)) {
        LOG(ERROR) << "Could not share GL contexts.";
        Destroy();
        return false;
      }
    }
  }

  return true;
}

void GLContextWGL::Destroy() {
  if (context_) {
    wglDeleteContext(context_);
    context_ = NULL;
  }
}

bool GLContextWGL::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  if (!wglMakeCurrent(static_cast<HDC>(surface->GetHandle()), context_)) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  SetCurrent(this, surface);
  surface->OnMakeCurrent(this);
  return true;
}

void GLContextWGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(NULL, NULL);
  wglMakeCurrent(NULL, NULL);
}

bool GLContextWGL::IsCurrent(GLSurface* surface) {
  bool native_context_is_current =
      wglGetCurrentContext() == context_;

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetCurrent() == this));

  if (!native_context_is_current)
    return false;

  if (surface) {
    if (wglGetCurrentDC() != surface->GetHandle())
      return false;
  }

  return true;
}

void* GLContextWGL::GetHandle() {
  return context_;
}

void GLContextWGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent(NULL));
  if (HasExtension("WGL_EXT_swap_control") && wglSwapIntervalEXT) {
    wglSwapIntervalEXT(interval);
  } else {
      LOG(WARNING) <<
          "Could not disable vsync: driver does not "
          "support WGL_EXT_swap_control";
  }
}

}  // namespace gfx
