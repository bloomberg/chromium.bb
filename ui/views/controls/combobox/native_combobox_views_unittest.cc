// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/native_combobox_views.h"
#include "ui/views/ime/mock_input_method.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

namespace {

// A wrapper of Combobox to intercept the result of OnKeyPressed() and
// OnKeyReleased() methods.
class TestCombobox : public views::Combobox {
 public:
  TestCombobox(ui::ComboboxModel* model)
      : Combobox(model),
        key_handled_(false),
        key_received_(false) {
  }

  virtual bool OnKeyPressed(const views::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Combobox::OnKeyPressed(e);
    return key_handled_;
  }

  virtual bool OnKeyReleased(const views::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Combobox::OnKeyReleased(e);
    return key_handled_;
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() {
    key_received_ = key_handled_ = false;
  }

 private:
  bool key_handled_;
  bool key_received_;

  DISALLOW_COPY_AND_ASSIGN(TestCombobox);
};

// A concrete class is needed to test the combobox
class TestComboboxModel : public ui::ComboboxModel {
 public:
  TestComboboxModel() {}
  virtual ~TestComboboxModel() {}
  virtual int GetItemCount() {
    return 4;
  }
  virtual string16 GetItemAt(int index) {
    EXPECT_GE(index, 0);
    EXPECT_LT(index, GetItemCount());
    return string16();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(TestComboboxModel);
};

}  // namespace

namespace views {

class NativeComboboxViewsTest : public ViewsTestBase {
 public:
  NativeComboboxViewsTest()
      : widget_(NULL),
        combobox_(NULL),
        combobox_view_(NULL),
        model_(NULL),
        input_method_(NULL) {
  }

  // ::testing::Test:
  virtual void SetUp() {
    ViewsTestBase::SetUp();
    Widget::SetPureViews(true);
  }

  virtual void TearDown() {
    Widget::SetPureViews(false);
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  void InitCombobox() {
    model_.reset(new TestComboboxModel());

    ASSERT_FALSE(combobox_);
    combobox_ = new TestCombobox(model_.get());
    combobox_->set_id(1);

    widget_ = new Widget;
    Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(combobox_);

    combobox_view_ = static_cast<NativeComboboxViews*>(
        combobox_->GetNativeWrapperForTesting());
    ASSERT_TRUE(combobox_view_);

    input_method_ = new MockInputMethod();
    widget_->ReplaceInputMethod(input_method_);

    // Assumes the Widget is always focused.
    input_method_->OnFocus();

    combobox_->RequestFocus();
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code) {
    KeyEvent event(ui::ET_KEY_PRESSED, key_code, 0);
    input_method_->DispatchKeyEvent(event);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  // We need widget to populate wrapper class.
  Widget* widget_;

  // combobox_ will be allocated InitCombobox() and then owned by widget_.
  TestCombobox* combobox_;

  // combobox_view_ is the pointer to the pure Views interface of combobox_.
  NativeComboboxViews* combobox_view_;

  // Combobox does not take ownership of model_, which needs to be scoped.
  scoped_ptr<ui::ComboboxModel> model_;

  // For testing input method related behaviors.
  MockInputMethod* input_method_;
};

TEST_F(NativeComboboxViewsTest, KeyTest) {
  InitCombobox();
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(combobox_->selected_item() + 1, model_->GetItemCount());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(combobox_->selected_item(), 0);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(combobox_->selected_item(), 2);
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_EQ(combobox_->selected_item(), 2);
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_EQ(combobox_->selected_item(), 2);
}

}  // namespace views
