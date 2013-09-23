// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/surface_factory_ozone.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_implementation_linux.h"
#include "ui/gl/gl_osmesa_api_implementation.h"

namespace gfx {

namespace {

void GL_BINDING_CALL MarshalClearDepthToClearDepthf(GLclampd depth) {
  glClearDepthf(static_cast<GLclampf>(depth));
}

void GL_BINDING_CALL MarshalDepthRangeToDepthRangef(GLclampd z_near,
                                                    GLclampd z_far) {
  glDepthRangef(static_cast<GLclampf>(z_near), static_cast<GLclampf>(z_far));
}

}  // namespace

void GetAllowedGLImplementations(std::vector<GLImplementation>* impls) {
  impls->push_back(kGLImplementationEGLGLES2);
  impls->push_back(kGLImplementationOSMesaGL);
}

bool InitializeGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (GetGLImplementation() != kGLImplementationNone)
    return true;

  switch (implementation) {
    case kGLImplementationOSMesaGL:
      return InitializeGLBindingsOSMesaGL();
    case kGLImplementationEGLGLES2:
      if (!gfx::SurfaceFactoryOzone::GetInstance()->LoadEGLGLES2Bindings())
        return false;
      SetGLImplementation(kGLImplementationEGLGLES2);
      InitializeGLBindingsGL();
      InitializeGLBindingsEGL();

      // These two functions take single precision float rather than double
      // precision float parameters in GLES.
      ::gfx::g_driver_gl.fn.glClearDepthFn = MarshalClearDepthToClearDepthf;
      ::gfx::g_driver_gl.fn.glDepthRangeFn = MarshalDepthRangeToDepthRangef;
      break;
    case kGLImplementationMockGL: {
      SetGLGetProcAddressProc(GetMockGLProcAddress);
      SetGLImplementation(kGLImplementationMockGL);
      InitializeGLBindingsGL();
      break;
    }
    default:
      NOTIMPLEMENTED()
          << "Unsupported GL type for Ozone surface implementation";
      return false;
  }

  return true;
}

bool InitializeGLExtensionBindings(GLImplementation implementation,
                                   GLContext* context) {
  switch (implementation) {
    case kGLImplementationOSMesaGL:
      InitializeGLExtensionBindingsGL(context);
      InitializeGLExtensionBindingsOSMESA(context);
      break;
    case kGLImplementationEGLGLES2:
      InitializeGLExtensionBindingsGL(context);
      InitializeGLExtensionBindingsEGL(context);
      break;
    case kGLImplementationMockGL:
      InitializeGLExtensionBindingsGL(context);
      break;
    default:
      return false;
  }

  return true;
}

void InitializeDebugGLBindings() {
}

void ClearGLBindings() {
  ClearGLBindingsEGL();
  ClearGLBindingsGL();
  SetGLImplementation(kGLImplementationNone);
  UnloadGLNativeLibraries();
}

bool GetGLWindowSystemBindingInfo(GLWindowSystemBindingInfo* info) {
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
      return GetGLWindowSystemBindingInfoEGL(info);
    default:
      return false;
  }
  return false;
}

}  // namespace gfx
