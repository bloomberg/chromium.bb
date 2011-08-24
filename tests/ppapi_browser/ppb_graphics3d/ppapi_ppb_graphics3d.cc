// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test cases for PPB_Graphics3D functions.
// TODO(nfullagar): More comprehensive testing of the PPAPI interface.

#include <string.h>

#include <GLES2/gl2.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/internal_utils.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/dev/pp_graphics_3d_dev.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
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
  EXPECT(PPBGraphics3DDev() != NULL);
  TEST_PASSED;
}

// Tests the OpenGLES interface is available.
void TestOpenGLES2Interface() {
  EXPECT(PPBOpenGLES2Dev() != NULL);
  TEST_PASSED;
}

// Tests PPB_Graphics3D::Create().
void TestCreate() {
  int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, kWidth,
      PP_GRAPHICS3DATTRIB_HEIGHT, kHeight,
      PP_GRAPHICS3DATTRIB_NONE};
  PP_Resource graphics3d_id = PPBGraphics3DDev()->
      Create(pp_instance(), kInvalidResource, attribs);
  EXPECT(graphics3d_id != kInvalidResource);
  PP_Resource invalid_graphics3d_id = PPBGraphics3DDev()->
      Create(0, kInvalidResource, attribs);
  EXPECT(invalid_graphics3d_id == kInvalidResource);
  int32_t empty_attribs[] = {
      PP_GRAPHICS3DATTRIB_NONE};
  PP_Resource graphics3d_empty_attrib_id = PPBGraphics3DDev()->
      Create(pp_instance(), kInvalidResource, empty_attribs);
  EXPECT(graphics3d_empty_attrib_id != kInvalidResource);
  PP_Resource graphics3d_null_attrib_id = PPBGraphics3DDev()->
      Create(pp_instance(), kInvalidResource, NULL);
  EXPECT(graphics3d_null_attrib_id != kInvalidResource);
  TEST_PASSED;
}

// Tests PPB_Graphics3D::IsGraphics3D().
void TestIsGraphics3D() {
  int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, kWidth,
      PP_GRAPHICS3DATTRIB_HEIGHT, kHeight,
      PP_GRAPHICS3DATTRIB_NONE};
  PP_Resource graphics3d_id = PPBGraphics3DDev()->
      Create(pp_instance(), kInvalidResource, attribs);
  EXPECT(graphics3d_id != kInvalidResource);
  EXPECT(PPBGraphics3DDev()->IsGraphics3D(graphics3d_id) == PP_TRUE);
  TEST_PASSED;
}

struct RenderInfo {
  PP_Resource graphics3d_id;
  int32_t frame_counter;
  int32_t frame_end;
};

void SwapCallback(void* user_data, int32_t result) {
  EXPECT(result == PP_OK);
  RenderInfo* info = static_cast<RenderInfo *>(user_data);
  // Set graphics3d_id to the main context, so we can use normal gl style calls
  // instead of going through the PPAPI PPBOpenGLES2 interface.
  glSetCurrentContextPPAPI(info->graphics3d_id);
  glViewport(0, 0, kWidth, kHeight);
  float blue = float(info->frame_counter) / float(info->frame_end);
  glClearColor(0.0f, 0.0f, blue, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ++info->frame_counter;
  if (info->frame_counter < info->frame_end) {
    PP_CompletionCallback cc = PP_MakeCompletionCallback(SwapCallback, info);
    int32_t result = PPBGraphics3DDev()->SwapBuffers(info->graphics3d_id, cc);
    CHECK(PP_OK_COMPLETIONPENDING == result);
  } else {
    delete info;
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
  PP_Resource graphics3d_id = PPBGraphics3DDev()->
      Create(pp_instance(), kInvalidResource, attribs);
  EXPECT(graphics3d_id != kInvalidResource);
  int32_t success = PPBInstance()->BindGraphics(pp_instance(), graphics3d_id);
  EXPECT(success == PP_TRUE);
  RenderInfo* render_info = new RenderInfo;
  render_info->graphics3d_id = graphics3d_id;
  render_info->frame_counter = 0;
  render_info->frame_end = 256;
  glInitializePPAPI(ppb_get_interface());
  PP_CompletionCallback cc = MakeTestableCompletionCallback(
        "SwapBufferCallback", SwapCallback, render_info);
  PPBCore()->CallOnMainThread(0, cc, PP_OK);
  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestGraphics3DInterface", TestGraphics3DInterface);
  RegisterTest("TestOpenGLES2Interface", TestOpenGLES2Interface);
  RegisterTest("TestCreate", TestCreate);
  RegisterTest("TestIsGraphics3D", TestIsGraphics3D);
  RegisterTest("TestSwapBuffers", TestSwapBuffers);
}

void SetupPluginInterfaces() {
  // none
}
