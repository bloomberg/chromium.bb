// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_egl_x11.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/gl_surface_osmesa_x11.h"
#include "ui/gl/gl_surface_stub.h"

namespace gl {

scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "GLSurface::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(new GLSurfaceOSMesaX11(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new NativeViewGLSurfaceGLX(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      DCHECK(window != gfx::kNullAcceleratedWidget);
      scoped_refptr<GLSurface> surface(new NativeViewGLSurfaceEGLX11(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  TRACE_EVENT0("gpu", "GLSurface::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new GLSurfaceOSMesa(SURFACE_OSMESA_RGBA, size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(
          new UnmappedNativeViewGLSurfaceGLX(size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceEGL(size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return gfx::GetXDisplay();
}

}  // namespace gl
