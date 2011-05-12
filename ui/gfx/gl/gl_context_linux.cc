// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context_egl.h"
#include "ui/gfx/gl/gl_context_glx.h"
#include "ui/gfx/gl/gl_context_osmesa.h"
#include "ui/gfx/gl/gl_context_stub.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_surface_glx.h"
#include "ui/gfx/gl/gl_surface_stub.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"

namespace gfx {

GLContext* GLContext::CreateGLContext(GLSurface* compatible_surface,
                                      GLContext* shared_context) {
  scoped_ptr<GLSurface> surface(compatible_surface);

  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_ptr<GLContextOSMesa> context(
          new GLContextOSMesa(static_cast<GLSurfaceOSMesa*>(
              surface.release())));
      if (!context->Initialize(OSMESA_RGBA, shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationEGLGLES2: {
      scoped_ptr<GLContextEGL> context(
          new GLContextEGL(
              static_cast<GLSurfaceEGL*>(surface.release())));
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationDesktopGL: {
      scoped_ptr<GLContextGLX> context(
          new GLContextGLX(
              static_cast<GLSurfaceGLX*>(surface.release())));
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new GLContextStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gfx
