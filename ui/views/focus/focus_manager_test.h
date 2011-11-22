// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_FOCUS_MANAGER_TEST_H_
#define UI_VIEWS_FOCUS_FOCUS_MANAGER_TEST_H_

#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

class FocusChangeListener;

class FocusManagerTest : public ViewsTestBase,
                         public WidgetDelegate {
 public:
  FocusManagerTest();
  virtual ~FocusManagerTest();

  // Convenience to obtain the focus manager for the test's hosting widget.
  FocusManager* GetFocusManager();

  // Overridden from ViewsTestBase:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Overridden from WidgetDelegate:
  virtual View* GetContentsView() OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;

 protected:
  // Called after the Widget is initialized and the content view is added.
  // Override to add controls to the layout.
  virtual void InitContentView();

  void AddFocusChangeListener(FocusChangeListener* listener);

#if defined(OS_WIN) && !defined(USE_AURA)
  // Mocks activating/deactivating the window.
  void SimulateActivateWindow();
  void SimulateDeactivateWindow();

  void PostKeyDown(ui::KeyboardCode key_code);
  void PostKeyUp(ui::KeyboardCode key_code);
#endif

 private:
  View* contents_view_;
  FocusChangeListener* focus_change_listener_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerTest);
};

typedef std::pair<View*, View*> ViewPair;

// Use to record focus change notifications.
class TestFocusChangeListener : public FocusChangeListener {
 public:
  TestFocusChangeListener();
  virtual ~TestFocusChangeListener();

  const std::vector<ViewPair>& focus_changes() const { return focus_changes_; }
  void ClearFocusChanges();

  // Overridden from FocusChangeListener:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

 private:
  // A vector of which views lost/gained focus.
  std::vector<ViewPair> focus_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusChangeListener);
};

}  // namespace views

#endif  // UI_VIEWS_FOCUS_FOCUS_MANAGER_TEST_H_
