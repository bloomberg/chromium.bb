// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_manager_test.h"

#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// FocusManagerTest, public:

FocusManagerTest::FocusManagerTest()
    : contents_view_(new View),
      focus_change_listener_(NULL) {
}

FocusManagerTest::~FocusManagerTest() {
}

FocusManager* FocusManagerTest::GetFocusManager() {
  return GetWidget()->GetFocusManager();
}

////////////////////////////////////////////////////////////////////////////////
// FocusManagerTest, ViewTestBase overrides:

void FocusManagerTest::SetUp() {
  ViewsTestBase::SetUp();
  Widget* widget =
      Widget::CreateWindowWithBounds(this, gfx::Rect(0, 0, 1024, 768));
  InitContentView();
  widget->Show();
}

void FocusManagerTest::TearDown() {
  if (focus_change_listener_)
    GetFocusManager()->RemoveFocusChangeListener(focus_change_listener_);
  GetWidget()->Close();

  // Flush the message loop to make application verifiers happy.
  RunPendingMessages();
  ViewsTestBase::TearDown();
}

////////////////////////////////////////////////////////////////////////////////
// FocusManagerTest, WidgetDelegate implementation:

View* FocusManagerTest::GetContentsView() {
  return contents_view_;
}

Widget* FocusManagerTest::GetWidget() {
  return contents_view_->GetWidget();
}

const Widget* FocusManagerTest::GetWidget() const {
  return contents_view_->GetWidget();
}

////////////////////////////////////////////////////////////////////////////////
// FocusManagerTest, protected:

void FocusManagerTest::InitContentView() {
}

void FocusManagerTest::AddFocusChangeListener(FocusChangeListener* listener) {
  ASSERT_FALSE(focus_change_listener_);
  focus_change_listener_ = listener;
  GetFocusManager()->AddFocusChangeListener(listener);
}

#if defined(OS_WIN) && !defined(USE_AURA)
void FocusManagerTest::SimulateActivateWindow() {
  SendMessage(GetWidget()->GetNativeWindow(), WM_ACTIVATE, WA_ACTIVE, NULL);
}

void FocusManagerTest::SimulateDeactivateWindow() {
  SendMessage(GetWidget()->GetNativeWindow(), WM_ACTIVATE, WA_INACTIVE, NULL);
}

void FocusManagerTest::PostKeyDown(ui::KeyboardCode key_code) {
  PostMessage(GetWidget()->GetNativeView(), WM_KEYDOWN, key_code, 0);
}

void FocusManagerTest::PostKeyUp(ui::KeyboardCode key_code) {
  PostMessage(GetWidget()->GetNativeView(), WM_KEYUP, key_code, 0);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// TestFocusChangeListener

TestFocusChangeListener::TestFocusChangeListener() {
}

TestFocusChangeListener::~TestFocusChangeListener() {
}

void TestFocusChangeListener::OnWillChangeFocus(View* focused_before,
                                                View* focused_now) {
  focus_changes_.push_back(ViewPair(focused_before, focused_now));
}
void TestFocusChangeListener::OnDidChangeFocus(View* focused_before,
                                               View* focused_now) {
}

void TestFocusChangeListener::ClearFocusChanges() {
  focus_changes_.clear();
}

}  // namespace views
