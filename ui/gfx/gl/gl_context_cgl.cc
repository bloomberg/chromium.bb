// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_cgl.h"

#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_surface_cgl.h"

namespace gfx {

GLContextCGL::GLContextCGL()
  : context_(NULL) {
}

GLContextCGL::~GLContextCGL() {
  Destroy();
}

bool GLContextCGL::Initialize(GLContext* shared_context,
                              GLSurface* compatible_surface) {
  DCHECK(compatible_surface);

  CGLError res = CGLCreateContext(
      static_cast<CGLPixelFormatObj>(GLSurfaceCGL::GetPixelFormat()),
      shared_context ?
          static_cast<CGLContextObj>(shared_context->GetHandle()) : NULL,
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

  return true;
}

void GLContextCGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  CGLSetCurrentContext(NULL);
  CGLSetPBuffer(static_cast<CGLContextObj>(context_), NULL, 0, 0, 0);
}

bool GLContextCGL::IsCurrent(GLSurface* surface) {
  if (CGLGetCurrentContext() != context_)
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
  NOTREACHED() << "Attempt to call SetSwapInterval on a GLContextCGL.";
}

}  // namespace gfx
