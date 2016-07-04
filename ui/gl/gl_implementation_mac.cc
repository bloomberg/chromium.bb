// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_implementation.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gl/gl_context_stub_with_extensions.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_osmesa_api_implementation.h"

namespace gl {

void GetAllowedGLImplementations(std::vector<GLImplementation>* impls) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUnsafeES3APIs)) {
    impls->push_back(kGLImplementationDesktopGLCoreProfile);
  }
  impls->push_back(kGLImplementationDesktopGL);
  impls->push_back(kGLImplementationAppleGL);
  impls->push_back(kGLImplementationOSMesaGL);
}

bool InitializeDynamicGLBindings(GLImplementation implementation,
    GLContext* context) {
  switch (implementation) {
    case kGLImplementationOSMesaGL:
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      InitializeDynamicGLBindingsGL(context);
      break;
    case kGLImplementationMockGL:
      if (!context) {
        scoped_refptr<GLContextStubWithExtensions> mock_context(
            new GLContextStubWithExtensions());
        mock_context->SetGLVersionString("3.0");
        InitializeDynamicGLBindingsGL(mock_context.get());
      } else {
        InitializeDynamicGLBindingsGL(context);
      }
      break;
    default:
      return false;
  }

  return true;
}

bool GetGLWindowSystemBindingInfo(GLWindowSystemBindingInfo* info) {
  return false;
}

}  // namespace gl
