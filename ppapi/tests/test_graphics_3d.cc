// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_graphics_3d.h"

#include <GLES2/gl2.h>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/ppb_opengles.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Graphics3D);

bool TestGraphics3D::Init() {
  opengl_es2_ = reinterpret_cast<const PPB_OpenGLES2*>(
      pp::Module::Get()->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
  return opengl_es2_ && InitTestingInterface();
}

void TestGraphics3D::RunTest() {
  RUN_TEST(Frame);
}

std::string TestGraphics3D::TestFrame() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
  };
  pp::Graphics3D context(*instance_, pp::Graphics3D(), attribs);
  ASSERT_FALSE(context.is_null());

  // Clear color buffer to opaque red.
  opengl_es2_->ClearColor(context.pp_resource(), 1.0f, 0.0f, 0.0f, 1.0f);
  opengl_es2_->Clear(context.pp_resource(), GL_COLOR_BUFFER_BIT);
  // Check if the color buffer has opaque red.
  const uint8_t red_color[4] = {255, 0, 0, 255};
  std::string error = TestPixel(&context, width/2, height/2, red_color);
  if (!error.empty())
    return error;

  int32_t rv = SwapBuffersSync(&context);
  ASSERT_EQ(rv, PP_OK);
  PASS();
}

int32_t TestGraphics3D::SwapBuffersSync(pp::Graphics3D* context) {
  TestCompletionCallback callback(instance_->pp_instance(), true);
  int32_t rv = context->SwapBuffers(callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    return rv;

  return callback.WaitForResult();
}

std::string TestGraphics3D::TestPixel(
    pp::Graphics3D* context,
    int x, int y, const uint8_t expected_color[4]) {
  GLubyte pixel_color[4];
  opengl_es2_->ReadPixels(context->pp_resource(),
      x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);

  ASSERT_EQ(pixel_color[0], expected_color[0]);
  ASSERT_EQ(pixel_color[1], expected_color[1]);
  ASSERT_EQ(pixel_color[2], expected_color[2]);
  ASSERT_EQ(pixel_color[3], expected_color[3]);
  PASS();
}

