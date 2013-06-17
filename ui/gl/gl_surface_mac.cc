// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/mesa/src/include/GL/osmesa.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_cgl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"

#if defined(USE_AURA)
#include "ui/gl/gl_surface_nsview.h"
#endif

namespace gfx {

bool GLSurface::InitializeOneOffInternal() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationAppleGL:
      if (!GLSurfaceCGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceCGL::InitializeOneOff failed.";
        return false;
      }
      break;
    default:
      break;
  }
  return true;
}

scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "GLSurface::CreateViewGLSurface");
#if defined(USE_AURA)
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationAppleGL: {
      scoped_refptr<GLSurface> surface(new GLSurfaceNSView(window));
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
#else
  return CreateOffscreenGLSurface(gfx::Size(1, 1));
#endif
}

scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  TRACE_EVENT0("gpu", "GLSurface::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(new GLSurfaceOSMesa(OSMESA_RGBA,
                                                           size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL:
    case kGLImplementationAppleGL: {
      scoped_refptr<GLSurface> surface(new NoOpGLSurfaceCGL(size));
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

}  // namespace gfx
