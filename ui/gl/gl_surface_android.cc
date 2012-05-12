// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_android.h"

#include <EGL/egl.h>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "ui/gl/android_native_window.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

namespace gfx {

bool GLSurface::InitializeOneOffInternal() {
  static bool initialized = false;
  if (initialized)
    return true;

  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  initialized = true;
  return true;
}
// static
scoped_refptr<GLSurface>
GLSurface::CreateViewGLSurface(bool software, gfx::AcceleratedWidget window) {
  if (software)
    return NULL;

  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2: {
      // window is unused
      scoped_refptr<AndroidViewSurface> surface(new AndroidViewSurface());
      if (!surface->Initialize())
        return NULL;
      return surface;
    }
    default:
      NOTREACHED();
      return NULL;
  }
}

// static
scoped_refptr<GLSurface>
GLSurface::CreateOffscreenGLSurface(bool software, const gfx::Size& size) {
  if (software)
    return NULL;

  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2: {
      scoped_refptr<PbufferGLSurfaceEGL> surface(
          new PbufferGLSurfaceEGL(false, size));
      if (!surface->Initialize())
        return NULL;
      return surface;
    }
    default:
      NOTREACHED();
      return NULL;
  }
}

AndroidViewSurface::AndroidViewSurface()
  : NativeViewGLSurfaceEGL(false, 0),
    pbuffer_surface_(new PbufferGLSurfaceEGL(false, Size(1, 1))),
    window_(NULL) {
}

AndroidViewSurface::~AndroidViewSurface() {
}

bool AndroidViewSurface::Initialize() {
  DCHECK(pbuffer_surface_.get());
  return pbuffer_surface_->Initialize();
}

void AndroidViewSurface::Destroy() {
  if (pbuffer_surface_.get()) {
    pbuffer_surface_->Destroy();
  } else {
    window_ = NULL;
  }
  NativeViewGLSurfaceEGL::Destroy();
}

bool AndroidViewSurface::IsOffscreen() {
  return false;
}

bool AndroidViewSurface::SwapBuffers() {
  if (!pbuffer_surface_.get())
    return NativeViewGLSurfaceEGL::SwapBuffers();
  return true;
}

gfx::Size AndroidViewSurface::GetSize() {
  if (pbuffer_surface_.get())
    return pbuffer_surface_->GetSize();
  else
    return NativeViewGLSurfaceEGL::GetSize();
}

EGLSurface AndroidViewSurface::GetHandle() {
  if (pbuffer_surface_.get())
    return pbuffer_surface_->GetHandle();
  else
    return NativeViewGLSurfaceEGL::GetHandle();
}

bool AndroidViewSurface::Resize(const gfx::Size& size) {
  if (pbuffer_surface_.get())
    return pbuffer_surface_->Resize(size);
  else if (GetHandle()) {
    DCHECK(window_ && window_->GetNativeHandle());
    // Deactivate and restore any currently active context.
    EGLContext context = eglGetCurrentContext();
    if (context != EGL_NO_CONTEXT) {
      eglMakeCurrent(GetDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE,
          EGL_NO_CONTEXT);
    }
    NativeViewGLSurfaceEGL::Destroy();
    if (CreateWindowSurface(window_)) {
      if (context != EGL_NO_CONTEXT)
        eglMakeCurrent(GetDisplay(), GetHandle(), GetHandle(), context);
    }
  }
  return true;
}

bool AndroidViewSurface::CreateWindowSurface(AndroidNativeWindow* window) {
  DCHECK(window->GetNativeHandle());
  window_ = window;
  EGLSurface surface = eglCreateWindowSurface(GetDisplay(),
                                              GetConfig(),
                                              window->GetNativeHandle(),
                                              NULL);
  if (surface == EGL_NO_SURFACE) {
    LOG(ERROR) << "eglCreateWindowSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  SetHandle(surface);
  return true;
}

void AndroidViewSurface::SetNativeWindow(AndroidNativeWindow* window) {
  if (window->GetNativeHandle()) {
    DCHECK(pbuffer_surface_.get());
    pbuffer_surface_->Destroy();
    pbuffer_surface_ = NULL;

    CreateWindowSurface(window);
  } else {
    DCHECK(GetHandle());
    NativeViewGLSurfaceEGL::Destroy();
    window_ = NULL;

    pbuffer_surface_ = new PbufferGLSurfaceEGL(false, Size(1,1));
    pbuffer_surface_->Initialize();
  }
}

}  // namespace gfx
