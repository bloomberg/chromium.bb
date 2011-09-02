// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests PPB_Fullscreen_Dev.

#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "native_client/tests/ppapi_test_lib/testable_callback.h"

#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"

namespace {

// Common display resolutions.
PP_Size kSize320x200 = PP_MakeSize(320, 200);       // CGA
PP_Size kSize1280x800 = PP_MakeSize(1280, 800);     // WXGA: 13" MB Pro
PP_Size kSize1366x768 = PP_MakeSize(1366, 768);     // X2** Lenovo
PP_Size kSize1440x900 = PP_MakeSize(1440, 900);     // 13" MB Air
PP_Size kSize1600x900 = PP_MakeSize(1600, 900);     // T4** Lenovo
PP_Size kSize1680x1050 = PP_MakeSize(1680, 1050);   // WSXGA+: 15" MB Pro
PP_Size kSize1900x1080 = PP_MakeSize(1900, 1080);   // W/T5** Lenovo
PP_Size kSize1920x1080 = PP_MakeSize(1920, 1080);   // HD: 42" Mitsubishi
PP_Size kSize1920x1200 = PP_MakeSize(1920, 1200);   // WUXGA: 24" HP, 17" MB Pro
PP_Size kSize2560x1600 = PP_MakeSize(2560, 1600);   // WQXGA: 30" HP Monitor
PP_Size kSize2560x2048 = PP_MakeSize(2560, 2048);   // QSXGA

bool IsSizeRange(PP_Size size, PP_Size min_size, PP_Size max_size) {
  return (min_size.width <= size.width && size.width <= max_size.width &&
          min_size.height <= size.height && size.height <= max_size.height);
}

PP_Resource g_graphics2d = kInvalidResource;

bool CreateGraphics2D(PP_Resource* graphics2d) {
  PP_Size size = PP_MakeSize(90, 90);
  PP_Bool not_always_opaque = PP_FALSE;
  *graphics2d =
      PPBGraphics2D()->Create(pp_instance(), &size, not_always_opaque);
  return (*graphics2d != kInvalidResource);
}

////////////////////////////////////////////////////////////////////////////////
// Test cases
////////////////////////////////////////////////////////////////////////////////

// Test for the availability of PPB_FULLSCREEN_DEV_INTERFACE.
void TestGetInterface() {
  printf("--- TestGetInterface\n");
  EXPECT(PPBFullscreenDev() != NULL);
  TEST_PASSED;
}

// Test
//   PP_Bool (*IsFullscreen)(PP_Instance instance);
void TestIsFullscreenTrue() {
  printf("--- TestIsFullscreenTrue\n");
  EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_TRUE);
  TEST_PASSED;
}

void TestIsFullscreenFalse() {
  printf("--- TestIsFullscreenFalse\n");
  EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_FALSE);
  TEST_PASSED;
}

// Test
//   PP_Bool (*SetFullscreen)(PP_Instance instance, PP_Bool fullscreen);

bool g_fullscreen_pending = false;

// TODO(polina): investigate:
// -- this error
//    ERROR:render_widget_fullscreen_pepper.cc(142)] Not implemented reached in
//    virtual void<unnamed>::PepperWidget::setFocus(bool)
// -- flakiness: sometimes the screen goes black after this function, sometimes
// only after the 2nd call or not at all, sometimes there is part of the white
// window left, sometimes IsFullscreen returns true and sometimes falls after
// this completes in DidChangeView.
void TestSetFullscreenTrue() {
  printf("--- TestSetFullscreenTrue\n");
  const PPB_Fullscreen_Dev* ppb = PPBFullscreenDev();
  if (ppb->IsFullscreen(pp_instance()) == PP_FALSE) {
    EXPECT(CreateGraphics2D(&g_graphics2d));
    // The transition is asynchronous and ends at the next DidChangeView().
    g_fullscreen_pending = true;
    EXPECT(ppb->SetFullscreen(pp_instance(), PP_TRUE) == PP_TRUE);
    EXPECT(ppb->IsFullscreen(pp_instance()) == PP_FALSE);
    // No 2D or 3D device can be bound during transition.
    EXPECT(PPBGraphics2D()->IsGraphics2D(g_graphics2d) == PP_TRUE);
    EXPECT(PPBInstance()->BindGraphics(pp_instance(), g_graphics2d) ==
           PP_FALSE);
    // The transition ends at the next DidChangeView().
  } else {
    // No change.
    EXPECT(ppb->SetFullscreen(pp_instance(), PP_TRUE) == PP_TRUE);
    EXPECT(ppb->IsFullscreen(pp_instance()) == PP_TRUE);
    TEST_PASSED;
  }
}

void TestSetFullscreenFalse() {
  printf("--- TestSetFullscreenFalse\n");
  EXPECT(CreateGraphics2D(&g_graphics2d));
  // The transition is syncrhonous.
  EXPECT(PPBFullscreenDev()->SetFullscreen(pp_instance(), PP_FALSE) == PP_TRUE);
  EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_FALSE);
  // 2D and 3D device can be bound right away.
  EXPECT(PPBInstance()->BindGraphics(pp_instance(), g_graphics2d) == PP_TRUE);
  PPBCore()->ReleaseResource(g_graphics2d);
  TEST_PASSED;
}

// Test
//   PP_Bool (*GetScreenSize)(PP_Instance instance, struct PP_Size* size);

void TestGetScreenSizeHelper(PP_Size min_size, PP_Size max_size) {
  PP_Size size = PP_MakeSize(0, 0);
  EXPECT(PPBFullscreenDev()->GetScreenSize(pp_instance(), &size) == PP_TRUE);
  EXPECT(IsSizeRange(size, min_size, max_size));
}

void TestGetScreenSize() {
  printf("--- TestGetScreenSize\n");
  TestGetScreenSizeHelper(kSize320x200, kSize2560x2048);
  TEST_PASSED;
}

void TestGetScreenSize2560x1600() {
  printf("--- TestGetScreenSize2560x1600\n");
  TestGetScreenSizeHelper(kSize2560x1600, kSize2560x1600);
  TEST_PASSED;
}

void TestGetScreenSize1920x1200() {
  printf("--- TestGetScreenSize1920x1200\n");
  TestGetScreenSizeHelper(kSize1920x1200, kSize1920x1200);
  TEST_PASSED;
}

////////////////////////////////////////////////////////////////////////////////
// PPP_Instance
////////////////////////////////////////////////////////////////////////////////

// DidChangeView completes transition to fullscreen mode.
void DidChangeView(PP_Instance instance,
                   const struct PP_Rect* position,
                   const struct PP_Rect* clip) {
  if (!g_fullscreen_pending)
    return;
  printf("--- PPP_Instance::DidChangeView: fullscreen_pending=true\n");
  g_fullscreen_pending = false;
  EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_TRUE);
  EXPECT(PPBGraphics2D()->IsGraphics2D(g_graphics2d) == PP_TRUE);
  // We should not be able to bind 2D and 3D devices.
  EXPECT(PPBInstance()->BindGraphics(pp_instance(), g_graphics2d) == PP_TRUE);
  PPBCore()->ReleaseResource(g_graphics2d);
  PostTestMessage("TestSetFullscreenTrue", "PASSED");
}

const PPP_Instance ppp_instance_interface = {
  DidCreateDefault,
  DidDestroyDefault,
  DidChangeView,
  DidChangeFocusDefault,
  HandleDocumentLoadDefault
};

}  // namespace

void SetupTests() {
  RegisterTest("TestGetInterface", TestGetInterface);
  RegisterTest("TestIsFullscreenTrue", TestIsFullscreenTrue);
  RegisterTest("TestIsFullscreenFalse", TestIsFullscreenFalse);
  RegisterTest("TestSetFullscreenTrue", TestSetFullscreenTrue);
  RegisterTest("TestSetFullscreenFalse", TestSetFullscreenFalse);
  RegisterTest("TestGetScreenSize", TestGetScreenSize);
  RegisterTest("TestGetScreenSize2560x1600", TestGetScreenSize2560x1600);
  RegisterTest("TestGetScreenSize1920x1200", TestGetScreenSize1920x1200);
}

void SetupPluginInterfaces() {
  RegisterPluginInterface(PPP_INSTANCE_INTERFACE, &ppp_instance_interface);
}
