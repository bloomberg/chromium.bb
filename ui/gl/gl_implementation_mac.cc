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

bool GetGLWindowSystemBindingInfo(GLWindowSystemBindingInfo* info) {
  return false;
}

}  // namespace gl
