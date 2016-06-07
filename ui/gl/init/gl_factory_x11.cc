// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_glx.h"
#include "ui/gl/gl_context_osmesa.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace gl {
namespace init {

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         GpuPreference gpu_preference) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL:
      return InitializeGLContext(new GLContextOSMesa(share_group),
                                 compatible_surface, gpu_preference);
    case kGLImplementationDesktopGL:
      return InitializeGLContext(new GLContextGLX(share_group),
                                 compatible_surface, gpu_preference);
    case kGLImplementationEGLGLES2:
      return InitializeGLContext(new GLContextEGL(share_group),
                                 compatible_surface, gpu_preference);
    case kGLImplementationMockGL:
      return new GLContextStub(share_group);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace init
}  // namespace gl
