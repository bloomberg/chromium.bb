// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context_cgl.h"
#include "ui/gfx/gl/gl_context_osmesa.h"
#include "ui/gfx/gl/gl_context_stub.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_cgl.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"

namespace gfx {

GLContext* GLContext::CreateGLContext(GLSurface* compatible_surface,
                                      GLContext* shared_context) {
  scoped_ptr<GLSurface> surface(compatible_surface);

  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
      scoped_ptr<GLContextCGL> context(
          new GLContextCGL(
              static_cast<GLSurfaceCGL*>(surface.release())));
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationOSMesaGL: {
      scoped_ptr<GLContextOSMesa> context(
          new GLContextOSMesa(
              static_cast<GLSurfaceOSMesa*>(surface.release())));
      if (!context->Initialize(OSMESA_RGBA, shared_context))
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
