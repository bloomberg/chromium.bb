// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_implementation.h"

#include "ui/gl/gl_context_stub_with_extensions.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_glx_api_implementation.h"
#include "ui/gl/gl_implementation_osmesa.h"
#include "ui/gl/gl_osmesa_api_implementation.h"

namespace gl {

void GetAllowedGLImplementations(std::vector<GLImplementation>* impls) {
  impls->push_back(kGLImplementationDesktopGL);
  impls->push_back(kGLImplementationEGLGLES2);
  impls->push_back(kGLImplementationOSMesaGL);
}

bool GetGLWindowSystemBindingInfo(GLWindowSystemBindingInfo* info) {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      return GetGLWindowSystemBindingInfoGLX(info);
    case kGLImplementationEGLGLES2:
      return GetGLWindowSystemBindingInfoEGL(info);
    default:
      return false;
  }
  return false;
}

}  // namespace gl
