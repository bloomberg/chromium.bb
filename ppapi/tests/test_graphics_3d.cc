// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_graphics_3d.h"

#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/cpp/module.h"

REGISTER_TEST_CASE(Graphics3D);

bool TestGraphics3D::Init() {
  graphics_3d_ = reinterpret_cast<const PPB_Graphics3D_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_GRAPHICS_3D_DEV_INTERFACE));
  opengl_es2_ = reinterpret_cast<const PPB_OpenGLES2_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_OPENGLES2_DEV_INTERFACE));
  return graphics_3d_ && opengl_es2_ && InitTestingInterface();
}

void TestGraphics3D::RunTest() {
}

