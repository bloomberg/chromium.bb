// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_cgl.h"

#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_surface_cgl.h"

namespace gfx {

GLContextCGL::GLContextCGL(GLSurfaceCGL* surface)
  : surface_(surface),
    context_(NULL) {
}

GLContextCGL::~GLContextCGL() {
  Destroy();
}

bool GLContextCGL::Initialize(GLContext* shared_context) {
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

bool GLContextCGL::MakeCurrent() {
  if (IsCurrent())
    return true;

  if (CGLSetPBuffer(static_cast<CGLContextObj>(context_),
                    static_cast<CGLPBufferObj>(surface_->GetHandle()),
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

bool GLContextCGL::IsCurrent() {
  return CGLGetCurrentContext() == context_;
}

bool GLContextCGL::IsOffscreen() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->IsOffscreen();
}

bool GLContextCGL::SwapBuffers() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->SwapBuffers();
}

gfx::Size GLContextCGL::GetSize() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->GetSize();
}

void* GLContextCGL::GetHandle() {
  return context_;
}

void GLContextCGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  NOTREACHED() << "Attempt to call SetSwapInterval on a GLContextCGL.";
}

}  // namespace gfx
