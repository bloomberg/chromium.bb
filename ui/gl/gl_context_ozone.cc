// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sys_info.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

namespace gfx {

// static
scoped_refptr<GLContext> GLContext::CreateGLContext(
    GLShareGroup* share_group,
    GLSurface* compatible_surface,
    GpuPreference gpu_preference) {
  if (GetGLImplementation() == kGLImplementationMockGL)
    return scoped_refptr<GLContext>(new GLContextStub());

  scoped_refptr<GLContext> context;
  if (!compatible_surface->GetHandle()) {
    DLOG(ERROR) << "Surface has no associated handle.\n";
    return NULL;
  }

  context = new GLContextEGL(share_group);

  if (!context->Initialize(compatible_surface, gpu_preference))
    return NULL;
  return context;
}

}  // namespace gfx

