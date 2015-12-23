// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"

namespace views {
namespace test {

// Tests for NativeWidgetMac that rely on global window manager state, and can
// not be parallelized.
class NativeWidgetMacInteractiveUITest
    : public WidgetTest,
      public ::testing::WithParamInterface<bool> {
 public:
  class Observer;

  NativeWidgetMacInteractiveUITest()
      : activationCount_(0), deactivationCount_(0) {}

  Widget* MakeWidget() {
    return GetParam() ? CreateTopLevelFramelessPlatformWidget()
                      : CreateTopLevelPlatformWidget();
  }

 protected:
  scoped_ptr<Observer> observer_;
  int activationCount_;
  int deactivationCount_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacInteractiveUITest);
};

class NativeWidgetMacInteractiveUITest::Observer : public TestWidgetObserver {
 public:
  Observer(NativeWidgetMacInteractiveUITest* parent, Widget* widget)
      : TestWidgetObserver(widget), parent_(parent) {}

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active)
      parent_->activationCount_++;
    else
      parent_->deactivationCount_++;
  }

 private:
  NativeWidgetMacInteractiveUITest* parent_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

// Test that showing a window causes it to attain global keyWindow status.
TEST_P(NativeWidgetMacInteractiveUITest, ShowAttainsKeyStatus) {
  Widget* widget = MakeWidget();
  observer_.reset(new Observer(this, widget));

  EXPECT_FALSE(widget->IsActive());
  EXPECT_EQ(0, activationCount_);
  widget->Show();
  EXPECT_TRUE(widget->IsActive());
  RunPendingMessages();
  EXPECT_TRUE([widget->GetNativeWindow() isKeyWindow]);
  EXPECT_EQ(1, activationCount_);
  EXPECT_EQ(0, deactivationCount_);

  // Now check that losing and gaining key status due events outside of Widget
  // works correctly.
  Widget* widget2 = MakeWidget();  // Note: not observed.
  EXPECT_EQ(0, deactivationCount_);
  widget2->Show();
  EXPECT_EQ(1, deactivationCount_);

  RunPendingMessages();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_EQ(1, deactivationCount_);
  EXPECT_EQ(1, activationCount_);

  [widget->GetNativeWindow() makeKeyAndOrderFront:nil];
  RunPendingMessages();
  EXPECT_TRUE(widget->IsActive());
  EXPECT_EQ(1, deactivationCount_);
  EXPECT_EQ(2, activationCount_);

  widget2->CloseNow();
  widget->CloseNow();

  EXPECT_EQ(1, deactivationCount_);
  EXPECT_EQ(2, activationCount_);
}

// Test that ShowInactive does not take keyWindow status.
TEST_P(NativeWidgetMacInteractiveUITest, ShowInactiveIgnoresKeyStatus) {
  Widget* widget = MakeWidget();

  base::scoped_nsobject<WindowedNSNotificationObserver> waiter(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidBecomeKeyNotification
                       object:widget->GetNativeWindow()]);

  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([widget->GetNativeWindow() isVisible]);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE([widget->GetNativeWindow() isKeyWindow]);
  widget->ShowInactive();

  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([widget->GetNativeWindow() isVisible]);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE([widget->GetNativeWindow() isKeyWindow]);

  // If the window were to become active, this would activate it.
  RunPendingMessages();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE([widget->GetNativeWindow() isKeyWindow]);
  EXPECT_EQ(0, [waiter notificationCount]);

  // Activating the inactive widget should make it key, asynchronously.
  widget->Activate();
  [waiter wait];
  EXPECT_EQ(1, [waiter notificationCount]);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE([widget->GetNativeWindow() isKeyWindow]);

  widget->CloseNow();
}

INSTANTIATE_TEST_CASE_P(NativeWidgetMacInteractiveUITestInstance,
                        NativeWidgetMacInteractiveUITest,
                        ::testing::Bool());

}  // namespace test
}  // namespace views
