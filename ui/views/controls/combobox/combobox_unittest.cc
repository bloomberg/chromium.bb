// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <set>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/combobox_model.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/ime/mock_input_method.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

// A wrapper of Combobox to intercept the result of OnKeyPressed() and
// OnKeyReleased() methods.
class TestCombobox : public views::Combobox {
 public:
  explicit TestCombobox(ui::ComboboxModel* model)
      : Combobox(model),
        key_handled_(false),
        key_received_(false) {
  }

  virtual bool OnKeyPressed(const ui::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Combobox::OnKeyPressed(e);
    return key_handled_;
  }

  virtual bool OnKeyReleased(const ui::KeyEvent& e) OVERRIDE {
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

// A concrete class is needed to test the combobox.
class TestComboboxModel : public ui::ComboboxModel {
 public:
  TestComboboxModel() {}
  virtual ~TestComboboxModel() {}

  // ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE {
    return 10;
  }
  virtual string16 GetItemAt(int index) OVERRIDE {
    return string16();
  }
  virtual bool IsItemSeparatorAt(int index) OVERRIDE {
    return separators_.find(index) != separators_.end();
  }

  void SetSeparators(const std::set<int>& separators) {
    separators_ = separators;
  }

 private:
  std::set<int> separators_;

  DISALLOW_COPY_AND_ASSIGN(TestComboboxModel);
};

}  // namespace

namespace views {

class ComboboxTest : public ViewsTestBase {
 public:
  ComboboxTest() : widget_(NULL), combobox_(NULL), input_method_(NULL) {}

  virtual void TearDown() OVERRIDE {
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
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(combobox_);

    input_method_ = new MockInputMethod();
    widget_->ReplaceInputMethod(input_method_);

    // Assumes the Widget is always focused.
    input_method_->OnFocus();

    combobox_->RequestFocus();
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, 0, false);
    input_method_->DispatchKeyEvent(event);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  // We need widget to populate wrapper class.
  Widget* widget_;

  // |combobox_| will be allocated InitCombobox() and then owned by |widget_|.
  TestCombobox* combobox_;

  // Combobox does not take ownership of the model, hence it needs to be scoped.
  scoped_ptr<TestComboboxModel> model_;

  // For testing input method related behaviors.
  MockInputMethod* input_method_;
};

TEST_F(ComboboxTest, KeyTest) {
  InitCombobox();
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(combobox_->selected_index() + 1, model_->GetItemCount());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(combobox_->selected_index(), 0);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(combobox_->selected_index(), 1);
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(combobox_->selected_index(), 0);
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(combobox_->selected_index(), model_->GetItemCount() - 1);
}

// Check that if a combobox is disabled before it has a native wrapper, then the
// native wrapper inherits the disabled state when it gets created.
TEST_F(ComboboxTest, DisabilityTest) {
  model_.reset(new TestComboboxModel());

  ASSERT_FALSE(combobox_);
  combobox_ = new TestCombobox(model_.get());
  combobox_->SetEnabled(false);

  widget_ = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget_->Init(params);
  View* container = new View();
  widget_->SetContentsView(container);
  container->AddChildView(combobox_);
  EXPECT_FALSE(combobox_->enabled());
}

// Verifies that we don't select a separator line in combobox when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipSeparatorSimple) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(2);
  model_->SetSeparators(separators);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
}

// Verifies that we never select the separator that is in the beginning of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorBeginning) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(0);
  model_->SetSeparators(separators);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(2, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
}

// Verifies that we never select the separator that is in the end of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorEnd) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(model_->GetItemCount() - 1);
  model_->SetSeparators(separators);
  combobox_->SetSelectedIndex(8);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(8, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(7, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(8, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the beginning of the combobox list when
// navigating through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtBeginning) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(2);
  model_->SetSeparators(separators);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(9, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(3, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the middle of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleAdjacentSeparatorsAtMiddle) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(4);
  separators.insert(5);
  separators.insert(6);
  model_->SetSeparators(separators);
  combobox_->SetSelectedIndex(3);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(7, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the end of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtEnd) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(7);
  separators.insert(8);
  separators.insert(9);
  model_->SetSeparators(separators);
  combobox_->SetSelectedIndex(6);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(6, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(5, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(6, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(6, combobox_->selected_index());
}

}  // namespace views
