// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppp_messaging.h"

namespace {

// Test PPB_Scrollbar_Dev::Create() and PPB_Scrollbar_Dev::IsScrollbar().
struct PP_Var TestCreate() {
  // Create a vertical scrollbar.
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(), PP_TRUE);
  EXPECT(scrollbar != kInvalidResource);
  EXPECT(PPBScrollbarDev()->IsScrollbar(scrollbar));
  PPBCore()->ReleaseResource(scrollbar);

  // Create a horizontal scrollbar.
  scrollbar = PPBScrollbarDev()->Create(pp_instance(), PP_FALSE);
  EXPECT(scrollbar != kInvalidResource);
  EXPECT(PPBScrollbarDev()->IsScrollbar(scrollbar));
  PPBCore()->ReleaseResource(scrollbar);

  // Test that an invalid instance causes failure.
  scrollbar = PPBScrollbarDev()->Create(kInvalidInstance, PP_TRUE);
  EXPECT(scrollbar == kInvalidResource);
  EXPECT(!PPBScrollbarDev()->IsScrollbar(scrollbar));

  return TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::GetThickness().
struct PP_Var TestGetThickness() {
  // Thickness is a platform-defined constant; about all we can assume is
  // that it is greater than 0.
  uint32_t thickness = PPBScrollbarDev()->GetThickness();
  EXPECT(thickness > 0);

  return TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::GetValue(), PPB_Scrollbar_Dev::SetValue().
struct PP_Var TestValue() {
  // Test that initial value is 0.
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(), PP_TRUE);
  uint32_t value = PPBScrollbarDev()->GetValue(scrollbar);
  EXPECT(value == 0);

  PPBScrollbarDev()->SetValue(scrollbar, 10);
  value = PPBScrollbarDev()->GetValue(scrollbar);
  EXPECT(value == 10);

  PPBScrollbarDev()->SetValue(scrollbar, 0);
  value = PPBScrollbarDev()->GetValue(scrollbar);
  EXPECT(value == 0);

  PPBCore()->ReleaseResource(scrollbar);
  return TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::SetDocumentSize(). This only effects scrollbar
// appearance, so it should be verified visually.
struct PP_Var TestSetDocumentSize() {
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(), PP_TRUE);
  PPBScrollbarDev()->SetDocumentSize(scrollbar, 100);
  PPBCore()->ReleaseResource(scrollbar);

  return TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::ScrollBy().
struct PP_Var TestScrollBy() {
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(), PP_TRUE);
  PPBScrollbarDev()->ScrollBy(scrollbar, PP_SCROLLBY_PIXEL, 10);
  uint32_t value = PPBScrollbarDev()->GetValue(scrollbar);
  EXPECT(value == 10);
  PPBCore()->ReleaseResource(scrollbar);

  return TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::SetTickMarks(). This only effects scrollbar
// appearance, so it should be verified visually.
struct PP_Var TestSetTickMarks() {
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(), PP_TRUE);
  uint32_t thickness = PPBScrollbarDev()->GetThickness();
  const int32_t kCount = 2;
  PP_Rect tick_marks[kCount] = {
      { {0, 0}, {thickness, 1} },
      { {10, 10}, {thickness, 1} } };
  PPBScrollbarDev()->SetTickMarks(scrollbar, tick_marks, kCount);
  // Make sure we can handle an empty array.
  PPBScrollbarDev()->SetTickMarks(scrollbar, NULL, 0);
  PPBCore()->ReleaseResource(scrollbar);

  return TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterScriptableTest("testCreate", TestCreate);
  RegisterScriptableTest("testGetThickness", TestGetThickness);
  RegisterScriptableTest("testValue", TestValue);
  RegisterScriptableTest("testSetDocumentSize", TestSetDocumentSize);
  RegisterScriptableTest("testSetTickMarks", TestSetTickMarks);
  RegisterScriptableTest("testScrollBy", TestScrollBy);
}


void SetupPluginInterfaces() {
}
