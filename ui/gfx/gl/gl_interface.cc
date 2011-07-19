// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_interface.h"

namespace gfx {

GLInterface* GLInterface::interface_;

void GLInterface::SetGLInterface(GLInterface* gl_interface) {
  interface_ = gl_interface;
}

GLInterface* GLInterface::GetGLInterface() {
  return interface_;
}

}  // namespace gfx

