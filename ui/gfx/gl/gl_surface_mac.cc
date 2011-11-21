// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_cgl.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"
#include "ui/gfx/gl/gl_surface_stub.h"

#if defined(USE_AURA)
#include "ui/gfx/gl/gl_surface_nsview.h"
#endif

namespace gfx {

bool GLSurface::InitializeOneOffInternal() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
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
    bool software,
    gfx::PluginWindowHandle window) {
#if defined(USE_AURA)
  if (software)
    return NULL;

  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
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
  return CreateOffscreenGLSurface(software, gfx::Size(1,1));
#endif
}

scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    bool software,
    const gfx::Size& size) {
  if (software)
    return NULL;

  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(new GLSurfaceOSMesa(OSMESA_RGBA,
                                                           size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceCGL(size));
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
