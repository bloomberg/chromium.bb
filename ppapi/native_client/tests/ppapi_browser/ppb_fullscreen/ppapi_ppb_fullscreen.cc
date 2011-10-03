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
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp_input_event.h"
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

bool IsSizeInRange(PP_Size size, PP_Size min_size, PP_Size max_size) {
  return (min_size.width <= size.width && size.width <= max_size.width &&
          min_size.height <= size.height && size.height <= max_size.height);
}

bool IsSizeEqual(PP_Size size, PP_Size expected) {
  return IsSizeInRange(size, expected, expected);
}

bool IsRectEqual(PP_Rect position, PP_Rect expected) {
  return (position.point.x == expected.point.x &&
          position.point.y == expected.point.y &&
          IsSizeEqual(position.size, expected.size));
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
bool g_normal_pending = false;

void TestSetFullscreenTrue() {
  printf("--- TestSetFullscreenTrue\n");
  const PPB_Fullscreen_Dev* ppb = PPBFullscreenDev();
  if (ppb->IsFullscreen(pp_instance()) == PP_FALSE) {
    // Transition to fullscreen.
    // This can only be done when processing a user gesture -
    // see HandleInputEvent().
    EXPECT(ppb->SetFullscreen(pp_instance(), PP_TRUE) == PP_FALSE);
    EXPECT(PPBInputEvent()->
           RequestInputEvents(pp_instance(),
                              PP_INPUTEVENT_CLASS_MOUSE) == PP_OK);
  } else {
    // No change.
    EXPECT(ppb->SetFullscreen(pp_instance(), PP_TRUE) == PP_FALSE);
    EXPECT(ppb->IsFullscreen(pp_instance()) == PP_TRUE);
    TEST_PASSED;
  }
}

void TestSetFullscreenFalse() {
  printf("--- TestSetFullscreenFalse\n");
  const PPB_Fullscreen_Dev* ppb = PPBFullscreenDev();
  if (ppb->IsFullscreen(pp_instance()) == PP_TRUE) {
    // Transition out of fullscreen.
    EXPECT(CreateGraphics2D(&g_graphics2d));
    // The transition is asynchronous and ends at the next DidChangeView().
    EXPECT(ppb->SetFullscreen(pp_instance(), PP_FALSE) == PP_TRUE);
    g_normal_pending = true;
    // Transition is pending, so additional requests fail.
    EXPECT(ppb->SetFullscreen(pp_instance(), PP_FALSE) == PP_FALSE);
    EXPECT(ppb->SetFullscreen(pp_instance(), PP_TRUE) == PP_FALSE);
    EXPECT(ppb->IsFullscreen(pp_instance()) == PP_TRUE);
    // No 2D or 3D device can be bound during transition.
    EXPECT(PPBGraphics2D()->IsGraphics2D(g_graphics2d) == PP_TRUE);
    EXPECT(PPBInstance()->BindGraphics(pp_instance(), g_graphics2d) ==
           PP_FALSE);
    // The transition ends at the next DidChangeView().
  } else {
    // No change.
    EXPECT(ppb->SetFullscreen(pp_instance(), PP_FALSE) == PP_FALSE);
    EXPECT(ppb->IsFullscreen(pp_instance()) == PP_FALSE);
    TEST_PASSED;
  }
}

// Test
//   PP_Bool (*GetScreenSize)(PP_Instance instance, struct PP_Size* size);

void TestGetScreenSizeHelper(PP_Size min_size, PP_Size max_size) {
  PP_Size size = PP_MakeSize(0, 0);
  EXPECT(PPBFullscreenDev()->GetScreenSize(pp_instance(), &size) == PP_TRUE);
  EXPECT(IsSizeInRange(size, min_size, max_size));
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
// PPP_InputEvent
////////////////////////////////////////////////////////////////////////////////

// Fullscreen transition requests are only possible when processing a user
// gesture (e.g. mouse click).
PP_Bool HandleInputEvent(PP_Instance instance, PP_Resource event) {
  PP_InputEvent_Type event_type = PPBInputEvent()->GetType(event);
  if (event_type != PP_INPUTEVENT_TYPE_MOUSEDOWN &&
      event_type != PP_INPUTEVENT_TYPE_MOUSEUP)
    return PP_FALSE;  // Not a mouse click.
  printf("--- PPP_InputEvent::HandleInputEvent\n");
  // We got the user gesture we needed, no need to handle events anymore.
  PPBInputEvent()->ClearInputEventRequest(pp_instance(),
                                          PP_INPUTEVENT_CLASS_MOUSE);
  EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_FALSE);
  EXPECT(CreateGraphics2D(&g_graphics2d));
  EXPECT(PPBFullscreenDev()->SetFullscreen(pp_instance(), PP_TRUE) == PP_TRUE);
  g_fullscreen_pending = true;
  // Transition is pending, so additional requests fail.
  EXPECT(PPBFullscreenDev()->SetFullscreen(pp_instance(), PP_TRUE) == PP_FALSE);
  EXPECT(PPBFullscreenDev()->SetFullscreen(pp_instance(), PP_FALSE) ==
         PP_FALSE);
  EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_FALSE);
  // No 2D or 3D device can be bound during transition.
  EXPECT(PPBGraphics2D()->IsGraphics2D(g_graphics2d) == PP_TRUE);
  EXPECT(PPBInstance()->BindGraphics(pp_instance(), g_graphics2d) ==
         PP_FALSE);
  // The transition ends at the next DidChangeView().
  return PP_FALSE;
}

const PPP_InputEvent ppp_input_event_interface = {
    &HandleInputEvent
};

////////////////////////////////////////////////////////////////////////////////
// PPP_Instance
////////////////////////////////////////////////////////////////////////////////

PP_Size GetScreenSize() {
  PP_Size screen_size = PP_MakeSize(0, 0);
  CHECK(PPBFullscreenDev()->GetScreenSize(pp_instance(), &screen_size));
  return screen_size;
}

bool HasMidScreen(const PP_Rect* position) {
  static PP_Size screen_size = GetScreenSize();
  static int32_t mid_x = screen_size.width / 2;
  static int32_t mid_y = screen_size.height / 2;
  PP_Point origin = position->point;
  PP_Size size = position->size;
  return (origin.x < mid_x && mid_x < origin.x + size.width &&
          origin.y < mid_y && mid_y < origin.y + size.height);
}

// DidChangeView completes transition to/from fullscreen mode.
// In fact, we get two of them when going to fullscreen, one when the plugin
// is placed in the middle of the window and one when the window is resized,
// placing the plugin in the middle of the screen.
// The size of the plugin is not affected.
bool g_saw_first_didchangeview = false;
PP_Rect g_normal_position = PP_MakeRectFromXYWH(0, 0, 0, 0);
void DidChangeView(PP_Instance instance,
                   const struct PP_Rect* position,
                   const struct PP_Rect* clip) {
  printf("--- PPP_Instance::DidChangeView: fullscreen_pending=%d "
         "position={ point=(%d,%d), size=%dx%d } "
         "clip={ point=(%d,%d), size=%dx%d }\n",
         g_fullscreen_pending,
         position->point.x, position->point.y,
         position->size.width, position->size.height,
         clip->point.x, clip->point.y,
         clip->size.width, clip->size.height);
  // Remember the original position on the first DidChangeView.
  if (g_normal_position.size.width == 0 && g_normal_position.size.height == 0) {
    g_normal_position = PP_MakeRectFromXYWH(position->point.x,
                                            position->point.y,
                                            position->size.width,
                                            position->size.height);
  }

  const char* test = NULL;
  if (g_fullscreen_pending && !g_saw_first_didchangeview) {
    g_saw_first_didchangeview = true;
    EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_TRUE);
    EXPECT(IsSizeEqual(position->size, g_normal_position.size));
    // Wait for the 2nd DidChangeView.
  } else if (g_fullscreen_pending) {
    test = "TestSetFullscreenTrue";
    g_fullscreen_pending = false;
    g_saw_first_didchangeview = false;  // Reset.
    EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_TRUE);
    EXPECT(HasMidScreen(position));
    EXPECT(IsSizeEqual(position->size, g_normal_position.size));
  } else if (g_normal_pending) {
    test = "TestSetFullscreenFalse";
    g_normal_pending = false;
    EXPECT(PPBFullscreenDev()->IsFullscreen(pp_instance()) == PP_FALSE);
    EXPECT(IsRectEqual(*position, g_normal_position));
  }
  if (test != NULL) {
    // We should now be able to bind 2D and 3D devices.
    EXPECT(PPBGraphics2D()->IsGraphics2D(g_graphics2d) == PP_TRUE);
    EXPECT(PPBInstance()->BindGraphics(pp_instance(), g_graphics2d) == PP_TRUE);
    PPBCore()->ReleaseResource(g_graphics2d);
    PostTestMessage(test, "PASSED");
  }
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
  RegisterPluginInterface(PPP_INPUT_EVENT_INTERFACE,
                          &ppp_input_event_interface);
}
