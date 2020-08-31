// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_display_egl_util_ozone.h"
#include "ui/gl/init/ozone_util.h"
#include "ui/ozone/public/ozone_platform.h"

namespace gl {
namespace init {

bool InitializeGLOneOffPlatform() {
  if (HasGLOzone()) {
    gl::GLDisplayEglUtil::SetInstance(gl::GLDisplayEglUtilOzone::GetInstance());
    return GetGLOzone()->InitializeGLOneOffPlatform();
  }

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

bool InitializeStaticGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  if (HasGLOzone(implementation)) {
    return GetGLOzone(implementation)
        ->InitializeStaticGLBindings(implementation);
  }

  switch (implementation) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      SetGLImplementation(implementation);
      InitializeStaticGLBindingsGL();
      return true;
    default:
      NOTREACHED();
  }

  return false;
}

void ShutdownGLPlatform() {
  if (HasGLOzone()) {
    GetGLOzone()->ShutdownGL();
    return;
  }

  ClearBindingsGL();
}

}  // namespace init
}  // namespace gl
