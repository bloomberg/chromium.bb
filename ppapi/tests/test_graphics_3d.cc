// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_graphics_3d.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/module.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

const int32_t kInvalidContext = 0;

REGISTER_TEST_CASE(Graphics3D);

bool TestGraphics3D::Init() {
  opengl_es2_ = static_cast<const PPB_OpenGLES2*>(
      pp::Module::Get()->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
  glInitializePPAPI(pp::Module::Get()->get_browser_interface());
  return opengl_es2_ && CheckTestingInterface();
}

void TestGraphics3D::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestGraphics3D, FramePPAPI, filter);
  RUN_CALLBACK_TEST(TestGraphics3D, FrameGL, filter);
  RUN_CALLBACK_TEST(TestGraphics3D, ExtensionsGL, filter);
}

std::string TestGraphics3D::TestFramePPAPI() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
  };
  pp::Graphics3D context(instance_, attribs);
  ASSERT_FALSE(context.is_null());

  const uint8_t red_color[4] = {255, 0, 0, 255};

  // Access OpenGLES API through the PPAPI interface.
  // Clear color buffer to opaque red.
  opengl_es2_->ClearColor(context.pp_resource(), 1.0f, 0.0f, 0.0f, 1.0f);
  opengl_es2_->Clear(context.pp_resource(), GL_COLOR_BUFFER_BIT);
  // Check if the color buffer has opaque red.
  std::string error = CheckPixelPPAPI(&context, width/2, height/2, red_color);
  if (!error.empty())
    return error;

  int32_t rv = SwapBuffersSync(&context);
  ASSERT_EQ(rv, PP_OK);

  PASS();
}

std::string TestGraphics3D::TestFrameGL() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
  };
  pp::Graphics3D context(instance_, attribs);
  ASSERT_FALSE(context.is_null());

  const uint8_t red_color[4] = {255, 0, 0, 255};
  // Perform same operations as TestFramePPAPI, but use OpenGLES API directly.
  // This is how most developers will use OpenGLES.
  glSetCurrentContextPPAPI(context.pp_resource());
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  std::string error = CheckPixelGL(width/2, height/2, red_color);
  glSetCurrentContextPPAPI(kInvalidContext);
  if (!error.empty())
    return error;

  int32_t rv = SwapBuffersSync(&context);
  ASSERT_EQ(rv, PP_OK);

  PASS();
}

std::string TestGraphics3D::TestExtensionsGL() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
  };
  pp::Graphics3D context(instance_, attribs);
  ASSERT_FALSE(context.is_null());

  glSetCurrentContextPPAPI(context.pp_resource());
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Ask about a couple of extensions via glGetString.  If an extension is
  // available, try a couple of trivial calls.  This test is not intended
  // to be exhaustive; check the source can compile, link, and run without
  // crashing.
  ASSERT_NE(glGetString(GL_VERSION), NULL);
  const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  if (strstr(ext, "GL_EXT_occlusion_query_boolean")) {
    GLuint a_query;
    GLboolean is_a_query;
    glGenQueriesEXT(1, &a_query);
    ASSERT_NE(a_query, 0);
    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, a_query);
    is_a_query = glIsQueryEXT(a_query);
    ASSERT_EQ(is_a_query, GL_TRUE);
    glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
    glDeleteQueriesEXT(1, &a_query);
  }
  if (strstr(ext, "GL_ANGLE_instanced_arrays")) {
    glDrawArraysInstancedANGLE(GL_TRIANGLE_STRIP, 0, 0, 0);
  }
  glSetCurrentContextPPAPI(kInvalidContext);

  int32_t rv = SwapBuffersSync(&context);
  ASSERT_EQ(rv, PP_OK);

  PASS();
}

int32_t TestGraphics3D::SwapBuffersSync(pp::Graphics3D* context) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  int32_t rv = context->SwapBuffers(callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    return rv;

  return callback.WaitForResult();
}

std::string TestGraphics3D::CheckPixelPPAPI(
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

std::string TestGraphics3D::CheckPixelGL(
    int x, int y, const uint8_t expected_color[4]) {
  GLubyte pixel_color[4];
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);

  ASSERT_EQ(pixel_color[0], expected_color[0]);
  ASSERT_EQ(pixel_color[1], expected_color[1]);
  ASSERT_EQ(pixel_color[2], expected_color[2]);
  ASSERT_EQ(pixel_color[3], expected_color[3]);
  PASS();
}

