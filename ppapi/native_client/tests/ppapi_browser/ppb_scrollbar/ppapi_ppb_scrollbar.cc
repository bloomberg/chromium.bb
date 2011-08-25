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

const PP_Bool kVertical = PP_TRUE;

// Test PPB_Scrollbar_Dev::Create() and PPB_Scrollbar_Dev::IsScrollbar().
void TestCreate() {
  // Create a vertical scrollbar.
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(),
                                                    kVertical);
  EXPECT(scrollbar != kInvalidResource);
  EXPECT(PPBScrollbarDev()->IsScrollbar(scrollbar));
  PPBCore()->ReleaseResource(scrollbar);

  // Create a horizontal scrollbar.
  scrollbar = PPBScrollbarDev()->Create(pp_instance(),
                                        PP_FALSE);  // vertical
  EXPECT(scrollbar != kInvalidResource);
  EXPECT(PPBScrollbarDev()->IsScrollbar(scrollbar));
  PPBCore()->ReleaseResource(scrollbar);

  // Test that an invalid instance causes failure.
  scrollbar = PPBScrollbarDev()->Create(kInvalidInstance,
                                        PP_TRUE);  // vertical
  EXPECT(scrollbar == kInvalidResource);
  EXPECT(!PPBScrollbarDev()->IsScrollbar(scrollbar));

  TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::GetThickness().
void TestGetThickness() {
  // Thickness is a platform-defined constant; about all we can assume is
  // that it is greater than 0.
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(),
                                                    kVertical);
  uint32_t thickness = PPBScrollbarDev()->GetThickness(scrollbar);
  EXPECT(thickness > 0);

  PPBCore()->ReleaseResource(scrollbar);

  TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::GetValue(), PPB_Scrollbar_Dev::SetValue().
void TestValue() {
  // Test that initial value is 0.
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(),
                                                    kVertical);
  uint32_t value = PPBScrollbarDev()->GetValue(scrollbar);
  EXPECT(value == 0);

  // Set document size to expand value range.
  // TODO(bbudge) Refine tests once issues with scrollbar size/value are
  // worked out. http://code.google.com/p/chromium/issues/detail?id=89367
  PPBScrollbarDev()->SetDocumentSize(scrollbar, 100);
  PPBScrollbarDev()->SetValue(scrollbar, 10);
  value = PPBScrollbarDev()->GetValue(scrollbar);
  EXPECT(value == 10);

  PPBScrollbarDev()->SetValue(scrollbar, 0);
  value = PPBScrollbarDev()->GetValue(scrollbar);
  EXPECT(value == 0);

  PPBCore()->ReleaseResource(scrollbar);

  TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::SetDocumentSize(). This only effects scrollbar
// appearance, so it should be verified visually.
void TestSetDocumentSize() {
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(),
                                                    kVertical);
  PPBScrollbarDev()->SetDocumentSize(scrollbar, 100);
  PPBCore()->ReleaseResource(scrollbar);

  TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::ScrollBy().
void TestScrollBy() {
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(),
                                                    kVertical);
  // Set document size to expand value range.
  // TODO(bbudge) Refine tests once issues with scrollbar size/value are
  // worked out. http://code.google.com/p/chromium/issues/detail?id=89367
  PPBScrollbarDev()->SetDocumentSize(scrollbar, 100);
  PPBScrollbarDev()->ScrollBy(scrollbar, PP_SCROLLBY_PIXEL, 10);
  uint32_t value = PPBScrollbarDev()->GetValue(scrollbar);
  EXPECT(value == 10);
  PPBCore()->ReleaseResource(scrollbar);

  TEST_PASSED;
}

// Test PPB_Scrollbar_Dev::SetTickMarks(). This only effects scrollbar
// appearance, so it should be verified visually.
void TestSetTickMarks() {
  PP_Resource scrollbar = PPBScrollbarDev()->Create(pp_instance(),
                                                    kVertical);
  uint32_t thickness = PPBScrollbarDev()->GetThickness(scrollbar);
  const int32_t kCount = 2;
  PP_Rect tick_marks[kCount] = {
      { {0, 0}, {thickness, 1} },
      { {10, 10}, {thickness, 1} } };
  PPBScrollbarDev()->SetTickMarks(scrollbar, tick_marks, kCount);
  // Make sure we can handle an empty array.
  PPBScrollbarDev()->SetTickMarks(scrollbar, NULL, 0);
  PPBCore()->ReleaseResource(scrollbar);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestCreate", TestCreate);
  RegisterTest("TestGetThickness", TestGetThickness);
  RegisterTest("TestValue", TestValue);
  RegisterTest("TestSetDocumentSize", TestSetDocumentSize);
  RegisterTest("TestSetTickMarks", TestSetTickMarks);
  RegisterTest("TestScrollBy", TestScrollBy);
}

void SetupPluginInterfaces() {
}

