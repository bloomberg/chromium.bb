// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface.h"

#include <wayland-egl.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gl/egl_util.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/wayland/wayland_display.h"

namespace {

// Encapsulates a pixmap EGL surface.
class PixmapGLSurfaceEGL : public gfx::GLSurfaceEGL {
 public:
  explicit PixmapGLSurfaceEGL(bool software, const gfx::Size& size);
  virtual ~PixmapGLSurfaceEGL();

  // Implement GLSurface.
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual EGLSurface GetHandle();

 private:
  gfx::Size size_;
  EGLSurface surface_;
  EGLNativePixmapType pixmap_;

  DISALLOW_COPY_AND_ASSIGN(PixmapGLSurfaceEGL);
};

PixmapGLSurfaceEGL::PixmapGLSurfaceEGL(bool software, const gfx::Size& size)
    : size_(size),
      surface_(NULL),
      pixmap_(NULL) {
  software_ = software;
}

PixmapGLSurfaceEGL::~PixmapGLSurfaceEGL() {
  Destroy();
}

bool PixmapGLSurfaceEGL::Initialize() {
  DCHECK(!surface_);
  DCHECK(!pixmap_);

  pixmap_ = wl_egl_pixmap_create(
      size_.width(),
      size_.height(),
      0);
  surface_ = eglCreatePixmapSurface(gfx::GLSurfaceEGL::GetDisplay(),
                                    gfx::GLSurfaceEGL::GetConfig(),
                                    pixmap_,
                                    NULL);
  if (!surface_) {
    LOG(ERROR) << "eglCreatePixmapSurface failed with error "
               << gfx::GetLastEGLErrorString();
    Destroy();
    return false;
  }

  return true;
}

void PixmapGLSurfaceEGL::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(gfx::GLSurfaceEGL::GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << gfx::GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
  if (pixmap_) {
    wl_egl_pixmap_destroy(pixmap_);
    pixmap_ = NULL;
  }
}

bool PixmapGLSurfaceEGL::IsOffscreen() {
  return true;
}

bool PixmapGLSurfaceEGL::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a PixmapGLSurfaceEGL.";
  return false;
}

gfx::Size PixmapGLSurfaceEGL::GetSize() {
  return size_;
}

EGLSurface PixmapGLSurfaceEGL::GetHandle() {
  return surface_;
}

}

namespace gfx {

bool GLSurface::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  static const GLImplementation kAllowedGLImplementations[] = {
    kGLImplementationEGLGLES2
  };

  if (!InitializeRequestedGLBindings(
      kAllowedGLImplementations,
      kAllowedGLImplementations + arraysize(kAllowedGLImplementations),
      kGLImplementationEGLGLES2)) {
    LOG(ERROR) << "InitializeRequestedGLBindings failed.";
    return false;
  }

  if (!GLSurfaceEGL::InitializeOneOff()) {
    LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
    return false;
  }

  initialized = true;
  return true;
}

scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    bool software,
    gfx::PluginWindowHandle window) {

  if (software)
    return NULL;

  scoped_refptr<GLSurface> surface(
      new NativeViewGLSurfaceEGL(software, window));
  if (!surface->Initialize())
    return NULL;

  return surface;
}

scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    bool software,
    const gfx::Size& size) {
  if (software)
    return NULL;

  scoped_refptr<GLSurface> surface(new PixmapGLSurfaceEGL(software, size));
  if (!surface->Initialize())
    return NULL;

  return surface;
}

} // namespace gfx
