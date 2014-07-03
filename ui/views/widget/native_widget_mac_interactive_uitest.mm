// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "ui/views/test/widget_test.h"

namespace views {
namespace test {

// Tests for NativeWidgetMac that rely on global window manager state, and can
// not be parallelized.
class NativeWidgetMacInteractiveUITest
    : public WidgetTest,
      public ::testing::WithParamInterface<bool> {
 public:
  NativeWidgetMacInteractiveUITest() {}

  Widget* MakeWidget() {
    return GetParam() ? CreateTopLevelFramelessPlatformWidget()
                      : CreateTopLevelPlatformWidget();
  }

  // Overridden form testing::Test:
  virtual void SetUp() {
    // Unbundled applications (those without Info.plist) default to
    // NSApplicationActivationPolicyProhibited, which prohibits the application
    // obtaining key status.
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    WidgetTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacInteractiveUITest);
};

// Test that showing a window causes it to attain global keyWindow status.
TEST_P(NativeWidgetMacInteractiveUITest, ShowAttainsKeyStatus) {
  Widget* widget = MakeWidget();

  EXPECT_FALSE(widget->IsActive());
  widget->Show();
  EXPECT_TRUE(widget->IsActive());

  RunPendingMessages();
  EXPECT_TRUE([widget->GetNativeWindow() isKeyWindow]);
  widget->CloseNow();
}

// Test that ShowInactive does not take keyWindow status from an active window.
TEST_P(NativeWidgetMacInteractiveUITest, ShowInactiveIgnoresKeyStatus) {
  Widget* widget = MakeWidget();

  // In an application with only a single window, that window is always "active"
  // for the application unless that window is not visible. However, it will not
  // be key.
  EXPECT_FALSE(widget->IsActive());
  widget->ShowInactive();
  EXPECT_TRUE(widget->IsActive());
  RunPendingMessages();
  EXPECT_FALSE([widget->GetNativeWindow() isKeyWindow]);

  // Creating a second widget should now keep that widget active.
  Widget* widget2 = MakeWidget();
  widget2->Show();
  widget->ShowInactive();

  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget2->IsActive());
  RunPendingMessages();
  EXPECT_FALSE([widget->GetNativeWindow() isKeyWindow]);
  EXPECT_TRUE([widget2->GetNativeWindow() isKeyWindow]);

  // And finally activating the inactive widget should activate it and make it
  // key.
  widget->Activate();
  EXPECT_TRUE(widget->IsActive());
  RunPendingMessages();
  EXPECT_TRUE([widget->GetNativeWindow() isKeyWindow]);

  widget2->CloseNow();
  widget->CloseNow();
}

INSTANTIATE_TEST_CASE_P(NativeWidgetMacInteractiveUITestInstance,
                        NativeWidgetMacInteractiveUITest,
                        ::testing::Bool());

}  // namespace test
}  // namespace views
