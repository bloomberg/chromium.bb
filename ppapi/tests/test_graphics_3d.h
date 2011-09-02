// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_GRAPHICS_3D_H_
#define PAPPI_TESTS_TEST_GRAPHICS_3D_H_

#include <string>
#include "ppapi/tests/test_case.h"

struct PPB_OpenGLES2_Dev;

namespace pp {
class Graphics3D_Dev;
}  // namespace pp

class TestGraphics3D : public TestCase {
 public:
  TestGraphics3D(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  // Various tests.
  std::string TestFrame();

  // Utils used by various tests.
  int32_t SwapBuffersSync(pp::Graphics3D_Dev* context);
  std::string TestPixel(pp::Graphics3D_Dev* context,
                        int x, int y, const uint8_t expected_color[4]);

  // OpenGL ES2 interface.
  const PPB_OpenGLES2_Dev* opengl_es2_;
};

#endif  // PAPPI_TESTS_TEST_GRAPHICS_3D_H_
