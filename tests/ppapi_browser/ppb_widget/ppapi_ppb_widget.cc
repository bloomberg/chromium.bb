// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppp_messaging.h"

namespace {

const PP_Bool kVertical = PP_TRUE;

// The only widgets that are currently implemented are scrollbars.

// Test the PPB_Widget_Dev::IsWidget() method.
void TestIsWidget() {
  EXPECT(PPBWidgetDev()->IsWidget(kInvalidResource) == PP_FALSE);
  EXPECT(PPBWidgetDev()->IsWidget(kNotAResource) == PP_FALSE);

  PP_Resource url_loader = PPBURLLoader()->Create(pp_instance());
  CHECK(url_loader != kInvalidResource);
  EXPECT(PPBWidgetDev()->IsWidget(url_loader) == PP_FALSE);
  PPBCore()->ReleaseResource(url_loader);

  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(), kVertical);
  EXPECT(PPBWidgetDev()->IsWidget(scrollbar) == PP_TRUE);

  PPBCore()->ReleaseResource(scrollbar);

  EXPECT(PPBWidgetDev()->IsWidget(scrollbar) == PP_FALSE);

  TEST_PASSED;
}

// Test the PPB_Widget_Dev::GetLocation() and SetLocation() methods.
void TestLocation() {
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(), kVertical);
  PP_Rect location = { {1, 2}, {3, 4} };
  PPBWidgetDev()->SetLocation(scrollbar, &location);
  PP_Rect get_location = { {0, 0}, {1, 1} };
  PPBWidgetDev()->GetLocation(scrollbar, &get_location);
  EXPECT(
      get_location.point.x == location.point.x &&
      get_location.point.y == location.point.y &&
      get_location.size.width == location.size.width &&
      get_location.size.height == location.size.height);

  PP_Rect far_and_big = { {1000000, 1000000}, {1000000, 1000000} };
  PPBWidgetDev()->SetLocation(scrollbar, &far_and_big);
  PPBWidgetDev()->GetLocation(scrollbar, &get_location);
  EXPECT(
      get_location.point.x == far_and_big.point.x &&
      get_location.point.y == far_and_big.point.y &&
      get_location.size.width == far_and_big.size.width &&
      get_location.size.height == far_and_big.size.height);

  PP_Rect neg_location = { {-1, -2}, {3, 4} };
  PPBWidgetDev()->SetLocation(scrollbar, &neg_location);
  PPBWidgetDev()->GetLocation(scrollbar, &get_location);
  EXPECT(
      get_location.point.x == neg_location.point.x &&
      get_location.point.y == neg_location.point.y &&
      get_location.size.width == neg_location.size.width &&
      get_location.size.height == neg_location.size.height);

  // TODO(bbudge) Add test for negative size components.
  // http://code.google.com/p/chromium/issues/detail?id=90792

  PPBCore()->ReleaseResource(scrollbar);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestIsWidget", TestIsWidget);
  RegisterTest("TestLocation", TestLocation);
}

void SetupPluginInterfaces() {
}

