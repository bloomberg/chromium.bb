// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_glx.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_glx_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl_x11.h"
#include "ui/gl/gl_surface_egl_x11_gles2.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/gl_surface_glx_x11.h"
#include "ui/gl/gl_surface_stub.h"

namespace gl {
namespace init {

std::vector<GLImplementation> GetAllowedGLImplementations() {
  std::vector<GLImplementation> impls;
  impls.push_back(kGLImplementationDesktopGL);
  impls.push_back(kGLImplementationEGLGLES2);
  impls.push_back(kGLImplementationEGLANGLE);
  impls.push_back(kGLImplementationSwiftShaderGL);
  return impls;
}

bool GetGLWindowSystemBindingInfo(const GLVersionInfo& gl_info,
                                  GLWindowSystemBindingInfo* info) {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      return GetGLWindowSystemBindingInfoGLX(gl_info, info);
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      return GetGLWindowSystemBindingInfoEGL(info);
    default:
      return false;
  }
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      return InitializeGLContext(new GLContextGLX(share_group),
                                 compatible_surface, attribs);
    case kGLImplementationSwiftShaderGL:
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      return InitializeGLContext(new GLContextEGL(share_group),
                                 compatible_surface, attribs);
    case kGLImplementationMockGL:
      return new GLContextStub(share_group);
    case kGLImplementationStubGL: {
      scoped_refptr<GLContextStub> stub_context =
          new GLContextStub(share_group);
      stub_context->SetUseStubApi(true);
      return stub_context;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      return InitializeGLSurface(new GLSurfaceGLXX11(window));
    case kGLImplementationSwiftShaderGL:
    case kGLImplementationEGLGLES2:
      DCHECK(window != gfx::kNullAcceleratedWidget);
      return InitializeGLSurface(new NativeViewGLSurfaceEGLX11GLES2(window));
    case kGLImplementationEGLANGLE:
      DCHECK(window != gfx::kNullAcceleratedWidget);
      return InitializeGLSurface(new NativeViewGLSurfaceEGLX11(window));
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateOffscreenGLSurfaceWithFormat(
    const gfx::Size& size, GLSurfaceFormat format) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      return InitializeGLSurfaceWithFormat(
          new UnmappedNativeViewGLSurfaceGLX(size), format);
    case kGLImplementationSwiftShaderGL:
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      if (GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
          size.width() == 0 && size.height() == 0) {
        return InitializeGLSurfaceWithFormat(new SurfacelessEGL(size), format);
      } else {
        return InitializeGLSurfaceWithFormat(new PbufferGLSurfaceEGL(size),
                                             format);
      }
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

void SetDisabledExtensionsPlatform(const std::string& disabled_extensions) {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  switch (implementation) {
    case kGLImplementationDesktopGL:
      SetDisabledExtensionsGLX(disabled_extensions);
      break;
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      SetDisabledExtensionsEGL(disabled_extensions);
      break;
    case kGLImplementationSwiftShaderGL:
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      break;
    default:
      NOTREACHED();
  }
}

bool InitializeExtensionSettingsOneOffPlatform() {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  switch (implementation) {
    case kGLImplementationDesktopGL:
      return InitializeExtensionSettingsOneOffGLX();
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      return InitializeExtensionSettingsOneOffEGL();
    case kGLImplementationSwiftShaderGL:
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace init
}  // namespace gl
