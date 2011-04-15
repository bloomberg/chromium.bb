// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_egl.h"

#include "build/build_config.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/angle/include/EGL/egl.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/egl_util.h"

// This header must come after the above third-party include, as
// it brings in #defines that cause conflicts.
#include "ui/gfx/gl/gl_bindings.h"

#if defined(OS_LINUX)
extern "C" {
#include <X11/Xlib.h>
}
#define EGL_HAS_PBUFFERS 1
#endif

namespace gfx {

std::string GLContextEGL::GetExtensions() {
  const char* extensions = eglQueryString(GLSurfaceEGL::GetDisplay(),
                                          EGL_EXTENSIONS);
  if (!extensions)
    return GLContext::GetExtensions();

  return GLContext::GetExtensions() + " " + extensions;
}

GLContextEGL::GLContextEGL(GLSurfaceEGL* surface)
    : surface_(surface),
      context_(NULL)
{
}

GLContextEGL::~GLContextEGL() {
  Destroy();
}

bool GLContextEGL::Initialize(GLContext* shared_context) {
  DCHECK(!context_);

  static const EGLint kContextAttributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  context_ = eglCreateContext(
      GLSurfaceEGL::GetDisplay(),
      GLSurfaceEGL::GetConfig(),
      shared_context ? shared_context->GetHandle() : NULL,
      kContextAttributes);
  if (!context_) {
    LOG(ERROR) << "eglCreateContext failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  if (!MakeCurrent()) {
    LOG(ERROR) << "MakeCurrent failed.";
    Destroy();
    return false;
  }

  if (!InitializeCommon()) {
    LOG(ERROR) << "GLContext::InitializeCommon failed.";
    Destroy();
    return false;
  }

  return true;
}

void GLContextEGL::Destroy() {
  if (context_) {
    if (!eglDestroyContext(GLSurfaceEGL::GetDisplay(), context_)) {
      LOG(ERROR) << "eglDestroyContext failed with error "
                 << GetLastEGLErrorString();
    }

    context_ = NULL;
  }

  if (surface_.get()) {
    surface_->Destroy();
    surface_.reset();
  }
}

bool GLContextEGL::MakeCurrent() {
  DCHECK(context_);
  if (IsCurrent())
      return true;

  if (!eglMakeCurrent(GLSurfaceEGL::GetDisplay(),
                      surface_->GetHandle(),
                      surface_->GetHandle(),
                      context_)) {
    VLOG(1) << "eglMakeCurrent failed with error "
            << GetLastEGLErrorString();
    return false;
  }

  return true;
}

bool GLContextEGL::IsCurrent() {
  DCHECK(context_);
  return context_ == eglGetCurrentContext();
}

bool GLContextEGL::IsOffscreen() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->IsOffscreen();
}

bool GLContextEGL::SwapBuffers() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->SwapBuffers();
}

gfx::Size GLContextEGL::GetSize() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->GetSize();
}

void* GLContextEGL::GetHandle() {
  return context_;
}

void GLContextEGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  if (!eglSwapInterval(GLSurfaceEGL::GetDisplay(), interval)) {
    LOG(ERROR) << "eglSwapInterval failed with error "
               << GetLastEGLErrorString();
  }
}

}  // namespace gfx
