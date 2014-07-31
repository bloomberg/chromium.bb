// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace test {

class NativeViewAccessibilityTest;

namespace {

class TestButton : public Button {
 public:
  TestButton() : Button(NULL) {}
};

}  // namespace

class NativeViewAccessibilityTest : public ViewsTestBase {
 public:
  NativeViewAccessibilityTest() {}
  virtual ~NativeViewAccessibilityTest() {}

  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();
    button_.reset(new TestButton());
    button_->SetSize(gfx::Size(20, 20));
    button_accessibility_ = NativeViewAccessibility::Create(button_.get());

    label_.reset(new Label);
    button_->AddChildView(label_.get());
    label_accessibility_ = NativeViewAccessibility::Create(label_.get());
  }

  virtual void TearDown() OVERRIDE {
    button_accessibility_->Destroy();
    button_accessibility_ = NULL;
    label_accessibility_->Destroy();
    label_accessibility_ = NULL;
    ViewsTestBase::TearDown();
  }

 protected:
  scoped_ptr<TestButton> button_;
  NativeViewAccessibility* button_accessibility_;
  scoped_ptr<Label> label_;
  NativeViewAccessibility* label_accessibility_;
};

TEST_F(NativeViewAccessibilityTest, RoleShouldMatch) {
  EXPECT_EQ(ui::AX_ROLE_BUTTON, button_accessibility_->GetData()->role);
  EXPECT_EQ(ui::AX_ROLE_STATIC_TEXT, label_accessibility_->GetData()->role);
}

TEST_F(NativeViewAccessibilityTest, BoundsShouldMatch) {
  gfx::Rect bounds = button_accessibility_->GetData()->location;
  bounds.Offset(button_accessibility_->GetGlobalCoordinateOffset());
  EXPECT_EQ(button_->GetBoundsInScreen(), bounds);
}

TEST_F(NativeViewAccessibilityTest, LabelIsChildOfButton) {
  EXPECT_EQ(1, button_accessibility_->GetChildCount());
  EXPECT_EQ(label_->GetNativeViewAccessible(),
            button_accessibility_->ChildAtIndex(0));
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            label_accessibility_->GetParent());
}

}  // namespace test
}  // namespace views
