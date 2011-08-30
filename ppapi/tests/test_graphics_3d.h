// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_GRAPHICS_3D_H_
#define PAPPI_TESTS_TEST_GRAPHICS_3D_H_

#include "ppapi/tests/test_case.h"

struct PPB_Graphics3D_Dev;
struct PPB_OpenGLES2_Dev;

class TestGraphics3D : public TestCase {
 public:
  TestGraphics3D(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  // Used by the tests that access the C API directly.
  const PPB_Graphics3D_Dev* graphics_3d_;
  const PPB_OpenGLES2_Dev* opengl_es2_;
};

#endif  // PAPPI_TESTS_TEST_GRAPHICS_3D_H_
