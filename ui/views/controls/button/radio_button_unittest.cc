// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/radio_button.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace {
// Group ID of the test radio buttons.
constexpr int kGroup = 1;
}  // namespace

namespace views {

class RadioButtonTest : public ViewsTestBase {
 public:
  RadioButtonTest() : button_container_(nullptr) {}

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a Widget so the radio buttons can find their group siblings.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(params);
    widget_->Show();

    button_container_ = new View();
    widget_->SetContentsView(button_container_);
  }

  void TearDown() override {
    button_container_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  View& button_container() { return *button_container_; }

 private:
  View* button_container_;
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(RadioButtonTest);
};

TEST_F(RadioButtonTest, Basics) {
  RadioButton* button1 = new RadioButton(base::ASCIIToUTF16("Blah"), kGroup);
  button_container().AddChildView(button1);
  RadioButton* button2 = new RadioButton(base::ASCIIToUTF16("Blah"), kGroup);
  button_container().AddChildView(button2);

  button1->SetChecked(true);
  EXPECT_TRUE(button1->checked());
  EXPECT_FALSE(button2->checked());

  button2->SetChecked(true);
  EXPECT_FALSE(button1->checked());
  EXPECT_TRUE(button2->checked());
}

TEST_F(RadioButtonTest, Focus) {
  RadioButton* button1 = new RadioButton(base::ASCIIToUTF16("Blah"), kGroup);
  button_container().AddChildView(button1);
  RadioButton* button2 = new RadioButton(base::ASCIIToUTF16("Blah"), kGroup);
  button_container().AddChildView(button2);

  // Tabbing through only focuses the checked button.
  button1->SetChecked(true);
  auto* focus_manager = button_container().GetFocusManager();
  ui::KeyEvent pressed_tab(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
  focus_manager->OnKeyEvent(pressed_tab);
  EXPECT_EQ(button1, focus_manager->GetFocusedView());
  focus_manager->OnKeyEvent(pressed_tab);
  EXPECT_EQ(button1, focus_manager->GetFocusedView());

  // The checked button can be moved using arrow keys.
  focus_manager->OnKeyEvent(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_DOWN, ui::EF_NONE));
  EXPECT_EQ(button2, focus_manager->GetFocusedView());
  EXPECT_FALSE(button1->checked());
  EXPECT_TRUE(button2->checked());

  focus_manager->OnKeyEvent(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UP, ui::EF_NONE));
  EXPECT_EQ(button1, focus_manager->GetFocusedView());
  EXPECT_TRUE(button1->checked());
  EXPECT_FALSE(button2->checked());
}

}  // namespace views
