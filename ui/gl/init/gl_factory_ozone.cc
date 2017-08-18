// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/ozone_util.h"

namespace gl {
namespace init {

std::vector<GLImplementation> GetAllowedGLImplementations() {
  DCHECK(GetSurfaceFactoryOzone());
  return GetSurfaceFactoryOzone()->GetAllowedGLImplementations();
}

bool GetGLWindowSystemBindingInfo(GLWindowSystemBindingInfo* info) {
  if (HasGLOzone())
    return GetGLOzone()->GetGLWindowSystemBindingInfo(info);

  return false;
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");

  if (HasGLOzone()) {
    return GetGLOzone()->CreateGLContext(share_group, compatible_surface,
                                         attribs);
  }

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
      return scoped_refptr<GLContext>(new GLContextStub(share_group));
    case kGLImplementationStubGL: {
      scoped_refptr<GLContextStub> stub_context =
          new GLContextStub(share_group);
      stub_context->SetUseStubApi(true);
      return stub_context;
    }
    default:
      NOTREACHED();
  }
  return nullptr;
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateViewGLSurface(window);

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub());
    default:
      NOTREACHED();
  }

  return nullptr;
}

scoped_refptr<GLSurface> CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateSurfacelessViewGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateSurfacelessViewGLSurface(window);

  return nullptr;
}

scoped_refptr<GLSurface> CreateOffscreenGLSurfaceWithFormat(
    const gfx::Size& size, GLSurfaceFormat format) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");

  if (!format.IsCompatible(GLSurfaceFormat())) {
    NOTREACHED() << "FATAL: Ozone only supports default-format surfaces.";
    return nullptr;
  }

  if (HasGLOzone())
    return GetGLOzone()->CreateOffscreenGLSurface(size);

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub);
    default:
      NOTREACHED();
  }

  return nullptr;
}

}  // namespace init
}  // namespace gl
