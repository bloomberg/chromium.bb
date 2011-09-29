// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_cgl.h"

#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_surface_cgl.h"

namespace gfx {

GLContextCGL::GLContextCGL(GLShareGroup* share_group)
  : GLContext(share_group),
    context_(NULL) {
}

GLContextCGL::~GLContextCGL() {
  Destroy();
}

bool GLContextCGL::Initialize(GLSurface* compatible_surface) {
  DCHECK(compatible_surface);

  CGLError res = CGLCreateContext(
      static_cast<CGLPixelFormatObj>(GLSurfaceCGL::GetPixelFormat()),
      share_group() ?
          static_cast<CGLContextObj>(share_group()->GetHandle()) : NULL,
      reinterpret_cast<CGLContextObj*>(&context_));
  if (res != kCGLNoError) {
    LOG(ERROR) << "Error creating context.";
    Destroy();
    return false;
  }

  return true;
}

void GLContextCGL::Destroy() {
  if (context_) {
    CGLDestroyContext(static_cast<CGLContextObj>(context_));
    context_ = NULL;
  }
}

bool GLContextCGL::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  if (CGLSetPBuffer(static_cast<CGLContextObj>(context_),
                    static_cast<CGLPBufferObj>(surface->GetHandle()),
                    0,
                    0,
                    0) != kCGLNoError) {
    LOG(ERROR) << "Error attaching pbuffer to context.";
    Destroy();
    return false;
  }

  if (CGLSetCurrentContext(
      static_cast<CGLContextObj>(context_)) != kCGLNoError) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  SetCurrent(this, surface);
  surface->OnMakeCurrent(this);
  return true;
}

void GLContextCGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(NULL, NULL);
  CGLSetCurrentContext(NULL);
  CGLSetPBuffer(static_cast<CGLContextObj>(context_), NULL, 0, 0, 0);
}

bool GLContextCGL::IsCurrent(GLSurface* surface) {
  bool native_context_is_current = CGLGetCurrentContext() == context_;

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetCurrent() == this));

  if (!native_context_is_current)
    return false;

  if (surface) {
    CGLPBufferObj current_surface = NULL;
    GLenum face;
    GLint level;
    GLint screen;
    CGLGetPBuffer(static_cast<CGLContextObj>(context_),
                  &current_surface,
                  &face,
                  &level,
                  &screen);
    if (current_surface != surface->GetHandle())
      return false;
  }

  return true;
}

void* GLContextCGL::GetHandle() {
  return context_;
}

void GLContextCGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent(NULL));
  LOG(WARNING) << "GLContex: GLContextCGL::SetSwapInterval is ignored.";
}

}  // namespace gfx
