// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test cases for PPB_Graphics3D functions.
// TODO(nfullagar): More comprehensive testing of the PPAPI interface.

#include <GLES2/gl2.h>
#include <string.h>
#include <sys/time.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/internal_utils.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"

namespace {

const int kWidth = 320;
const int kHeight = 200;
////////////////////////////////////////////////////////////////////////////////
// Test Cases
////////////////////////////////////////////////////////////////////////////////

// Tests the Graphics3D interface is available.
void TestGraphics3DInterface() {
  EXPECT(PPBGraphics3D() != NULL);
  TEST_PASSED;
}

// Tests the OpenGLES interface is available.
void TestOpenGLES2Interface() {
  EXPECT(PPBOpenGLES2() != NULL);
  TEST_PASSED;
}

// Tests PPB_Graphics3D::Create().
void TestCreate() {
  int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, kWidth,
      PP_GRAPHICS3DATTRIB_HEIGHT, kHeight,
      PP_GRAPHICS3DATTRIB_NONE};
  PP_Resource graphics3d_id = PPBGraphics3D()->
      Create(pp_instance(), kInvalidResource, attribs);
  EXPECT(graphics3d_id != kInvalidResource);
  PPBCore()->ReleaseResource(graphics3d_id);
  PP_Resource invalid_graphics3d_id = PPBGraphics3D()->
      Create(0, kInvalidResource, attribs);
  EXPECT(invalid_graphics3d_id == kInvalidResource);
  int32_t empty_attribs[] = {
      PP_GRAPHICS3DATTRIB_NONE};
  PP_Resource graphics3d_empty_attrib_id = PPBGraphics3D()->
      Create(pp_instance(), kInvalidResource, empty_attribs);
  EXPECT(graphics3d_empty_attrib_id != kInvalidResource);
  PPBCore()->ReleaseResource(graphics3d_empty_attrib_id);
  PP_Resource graphics3d_null_attrib_id = PPBGraphics3D()->
      Create(pp_instance(), kInvalidResource, NULL);
  EXPECT(graphics3d_null_attrib_id != kInvalidResource);
  PPBCore()->ReleaseResource(graphics3d_null_attrib_id);
  TEST_PASSED;
}

// Tests PPB_Graphics3D::IsGraphics3D().
void TestIsGraphics3D() {
  int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, kWidth,
      PP_GRAPHICS3DATTRIB_HEIGHT, kHeight,
      PP_GRAPHICS3DATTRIB_NONE};
  PP_Resource graphics3d_id = PPBGraphics3D()->
      Create(pp_instance(), kInvalidResource, attribs);
  EXPECT(graphics3d_id != kInvalidResource);
  EXPECT(PPBGraphics3D()->IsGraphics3D(graphics3d_id) == PP_TRUE);
  PPBCore()->ReleaseResource(graphics3d_id);
  TEST_PASSED;
}

// Tests glInitializePPAPI().
void Test_glInitializePPAPI() {
  GLboolean init_ppapi = glInitializePPAPI(ppb_get_interface());
  EXPECT(init_ppapi == true);
  TEST_PASSED;
}

// Test basic setup:
//   glSetCurrentContextPPAPI(), glGetIntegerv(), glGetString() and glGetError()
void TestBasicSetup() {
  int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, kWidth,
      PP_GRAPHICS3DATTRIB_HEIGHT, kHeight,
      PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 32,
      PP_GRAPHICS3DATTRIB_NONE};
  PP_Resource graphics3d_id = PPBGraphics3D()->
      Create(pp_instance(), kInvalidResource, attribs);
  EXPECT(graphics3d_id != kInvalidResource);
  glSetCurrentContextPPAPI(graphics3d_id);
  EXPECT(glGetString(GL_VERSION) != NULL);
  glGetString(GL_EXTENSIONS);
  int gl_max_texture_size;
  const int expected_lower_bound_max_texture_size = 256;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);
  EXPECT(gl_max_texture_size >= expected_lower_bound_max_texture_size);
  int gl_depth_size;
  const int expected_lower_bound_depth_size = 16;
  glGetIntegerv(GL_DEPTH_BITS, &gl_depth_size);
  EXPECT(gl_depth_size >= expected_lower_bound_depth_size);
  GLenum lastError = glGetError();
  EXPECT(lastError == GL_NO_ERROR);
  glEnable(GL_LINE_LOOP);
  lastError = glGetError();
  EXPECT(lastError == GL_INVALID_ENUM);
  glSetCurrentContextPPAPI(0);
  PPBCore()->ReleaseResource(graphics3d_id);
  TEST_PASSED;
}

struct RenderInfo {
  PP_Resource graphics3d_id;
  int32_t frame_counter;
  int32_t frame_end;
  int32_t frame_increment;
};

void TestSwapCallback(void* user_data, int32_t result) {
  EXPECT(result == PP_OK);
  RenderInfo* info = static_cast<RenderInfo *>(user_data);
  // Set graphics3d_id to the main context, so we can use normal gl style calls
  // instead of going through the PPAPI PPBOpenGLES2 interface.
  glSetCurrentContextPPAPI(info->graphics3d_id);
  glViewport(0, 0, kWidth, kHeight);
  float blue = float(info->frame_counter) / float(info->frame_end);
  glClearColor(0.0f, 0.0f, blue, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  info->frame_counter += info->frame_increment;
  if (info->frame_counter < info->frame_end) {
    PP_CompletionCallback cc =
        PP_MakeCompletionCallback(TestSwapCallback, info);
    int32_t result = PPBGraphics3D()->SwapBuffers(info->graphics3d_id, cc);
    CHECK(PP_OK_COMPLETIONPENDING == result);
  } else {
    PPBCore()->ReleaseResource(info->graphics3d_id);
    delete info;
    TEST_PASSED;
  }
  glSetCurrentContextPPAPI(0);
}

// Tests PPB_Graphics3D::SwapBuffers().  This test will render a visible
// result to the screen -- fading in a solid blue rectangle.
void TestSwapBuffers() {
  int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, kWidth,
      PP_GRAPHICS3DATTRIB_HEIGHT, kHeight,
      PP_GRAPHICS3DATTRIB_NONE};
  PP_Resource graphics3d_id = PPBGraphics3D()->
      Create(pp_instance(), kInvalidResource, attribs);
  EXPECT(graphics3d_id != kInvalidResource);
  int32_t success = PPBInstance()->BindGraphics(pp_instance(), graphics3d_id);
  EXPECT(success == PP_TRUE);
  RenderInfo* render_info = new RenderInfo;
  render_info->graphics3d_id = graphics3d_id;
  render_info->frame_counter = 0;
  render_info->frame_end = 256;
  render_info->frame_increment = 2;
  PP_CompletionCallback cc = PP_MakeCompletionCallback(
      TestSwapCallback, render_info);
  PPBCore()->CallOnMainThread(0, cc, PP_OK);
}

void TestResizeAndSwapCallback(void* user_data, int32_t result) {
  const int kMinSize = 1;
  EXPECT(result == PP_OK);
  RenderInfo* info = static_cast<RenderInfo *>(user_data);
  int32_t new_width = kWidth - info->frame_counter * 2;
  int32_t new_height = kHeight - info->frame_counter * 2;
  if (new_width < kMinSize) new_width = kMinSize;
  if (new_height < kMinSize) new_height = kMinSize;
  // Normally ResizeBuffers() would be in response to DidChangeView(...),
  // This test calls it to see if any unexpected crashes or return values occur.
  int32_t resize_result = PPBGraphics3D()->ResizeBuffers(info->graphics3d_id,
      new_width, new_height);
  EXPECT(resize_result == PP_OK);
  glSetCurrentContextPPAPI(info->graphics3d_id);
  glViewport(0, 0, kWidth, kHeight);
  float green = float(info->frame_counter) / float(info->frame_end);
  glClearColor(0.0f, green, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  info->frame_counter += info->frame_increment;
  if (info->frame_counter < info->frame_end) {
    PP_CompletionCallback cc =
        PP_MakeCompletionCallback(TestResizeAndSwapCallback, info);
    int32_t result = PPBGraphics3D()->SwapBuffers(info->graphics3d_id, cc);
    CHECK(PP_OK_COMPLETIONPENDING == result);
  } else {
    PPBCore()->ReleaseResource(info->graphics3d_id);
    delete info;
    TEST_PASSED;
  }
  glSetCurrentContextPPAPI(0);
}

// Tests PPB_Graphics3D::ResizeBuffers().
// The resize tests will render as a flashing green filled rectangle.
void TestResizeBuffers(int32_t* attribs) {
  PP_Resource graphics3d_id = PPBGraphics3D()->
      Create(pp_instance(), kInvalidResource, attribs);
  EXPECT(graphics3d_id != kInvalidResource);
  int32_t success = PPBInstance()->BindGraphics(pp_instance(), graphics3d_id);
  EXPECT(success == PP_TRUE);
  int32_t result;
  // Attempt negative width & heights
  result = PPBGraphics3D()->ResizeBuffers(graphics3d_id, -1000, 0);
  EXPECT(result == PP_ERROR_BADARGUMENT);
  result = PPBGraphics3D()->ResizeBuffers(graphics3d_id, 100, -1);
  EXPECT(result == PP_ERROR_BADARGUMENT);
  result = PPBGraphics3D()->ResizeBuffers(graphics3d_id, -1, -1);
  EXPECT(result == PP_ERROR_BADARGUMENT);
  RenderInfo* render_info = new RenderInfo;
  render_info->graphics3d_id = graphics3d_id;
  render_info->frame_counter = 0;
  render_info->frame_end = 256;
  render_info->frame_increment = 8;
  PP_CompletionCallback cc = PP_MakeCompletionCallback(
      TestResizeAndSwapCallback, render_info);
  PPBCore()->CallOnMainThread(0, cc, PP_OK);
}

// Tests PPB_Graphics3D::ResizeBuffers() w/o depth buffer attached.
void TestResizeBuffersWithoutDepthBuffer() {
  int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, kWidth,
      PP_GRAPHICS3DATTRIB_HEIGHT, kHeight,
      PP_GRAPHICS3DATTRIB_NONE};
  TestResizeBuffers(attribs);
}

// Tests PPB_Graphics3D::ResizeBuffers() w/ depth buffer attached.
void TestResizeBuffersWithDepthBuffer() {
  int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, kWidth,
      PP_GRAPHICS3DATTRIB_HEIGHT, kHeight,
      PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 16,
      PP_GRAPHICS3DATTRIB_NONE};
  TestResizeBuffers(attribs);
}

// Tests glTerminatePPAPI.
void Test_glTerminatePPAPI() {
  GLboolean terminate = glTerminatePPAPI();
  EXPECT(terminate == true);
  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestGraphics3DInterface", TestGraphics3DInterface);
  RegisterTest("TestOpenGLES2Interface", TestOpenGLES2Interface);
  RegisterTest("TestCreate", TestCreate);
  RegisterTest("TestIsGraphics3D", TestIsGraphics3D);
  RegisterTest("Test_glInitializePPAPI", Test_glInitializePPAPI);
  RegisterTest("TestBasicSetup", TestBasicSetup);
  RegisterTest("TestSwapBuffers", TestSwapBuffers);
  RegisterTest("TestResizeBuffersWithoutDepthBuffer",
      TestResizeBuffersWithoutDepthBuffer);
  RegisterTest("TestResizeBuffersWithDepthBuffer",
      TestResizeBuffersWithDepthBuffer);
  RegisterTest("Test_glTerminatePPAPI", Test_glTerminatePPAPI);
}

void SetupPluginInterfaces() {
  // none
}
