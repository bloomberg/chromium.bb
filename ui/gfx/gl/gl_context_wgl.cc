// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the GLContextWGL and PbufferGLContext classes.

#include "ui/gfx/gl/gl_context_wgl.h"

#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"

namespace gfx {

GLContextWGL::GLContextWGL(GLSurfaceWGL* surface)
    : surface_(surface),
      context_(NULL) {
  DCHECK(surface);
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
        static_cast<HDC>(surface_->GetHandle()));
    if (extensions) {
      return GLContext::GetExtensions() + " " + extensions;
    }
  }

  return GLContext::GetExtensions();
}

bool GLContextWGL::Initialize(GLContext* shared_context) {
  // TODO(apatrick): When contexts and surfaces are separated, we won't be
  // able to use surface_ here. Either use a display device context or a
  // surface that the context is compatible with not necessarily limited to
  // rendering to.
  context_ = wglCreateContext(static_cast<HDC>(surface_->GetHandle()));
  if (!context_) {
    LOG(ERROR) << "Failed to create GL context.";
    Destroy();
    return false;
  }

  if (shared_context) {
    if (!wglShareLists(
        static_cast<HGLRC>(shared_context->GetHandle()),
        context_)) {
      LOG(ERROR) << "Could not share GL contexts.";
      Destroy();
      return false;
    }
  }

  return true;
}

void GLContextWGL::Destroy() {
  if (context_) {
    wglDeleteContext(context_);
    context_ = NULL;
  }

  if (surface_.get()) {
    surface_->Destroy();
    surface_.reset();
  }
}

bool GLContextWGL::MakeCurrent() {
  if (IsCurrent()) {
    return true;
  }

  if (!wglMakeCurrent(static_cast<HDC>(surface_->GetHandle()),
      context_)) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  return true;
}

bool GLContextWGL::IsCurrent() {
  return wglGetCurrentDC() == surface_->GetHandle() &&
      wglGetCurrentContext() == context_;
}

bool GLContextWGL::IsOffscreen() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->IsOffscreen();
}

bool GLContextWGL::SwapBuffers() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->SwapBuffers();
}

gfx::Size GLContextWGL::GetSize() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->GetSize();
}

void* GLContextWGL::GetHandle() {
  return context_;
}

void GLContextWGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  if (HasExtension("WGL_EXT_swap_control") && wglSwapIntervalEXT) {
    wglSwapIntervalEXT(interval);
  }
}

}  // namespace gfx
