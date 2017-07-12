// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

namespace gl {
namespace init {

// TODO(fuchsia): Implement these functions.

bool InitializeGLOneOffPlatform() {
  NOTIMPLEMENTED();
  return false;
}

bool InitializeStaticGLBindings(GLImplementation implementation) {
  NOTIMPLEMENTED();
  return false;
}

void InitializeDebugGLBindings() {
  NOTIMPLEMENTED();
}

void ShutdownGLPlatform() {
  NOTIMPLEMENTED();
}

}  // namespace init
}  // namespace gl
