// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_model.h"

#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura_shell/launcher/launcher_model_observer.h"
#include "views/view.h"

using views::View;

namespace aura_shell {

namespace {

// LauncherModelObserver implementation that tracks what message are invoked.
class TestLauncherModelObserver : public LauncherModelObserver {
 public:
  TestLauncherModelObserver()
      : added_count_(0),
        removed_count_(0),
        selection_changed_count_(0) {
  }

  // Returns a string description of the changes that have occurred since this
  // was last invoked. Resets state to initial state.
  std::string StateStringAndClear() {
    std::string result(base::StringPrintf("added=%d removed=%d s_changed=%d",
        added_count_, removed_count_, selection_changed_count_));
    added_count_ = removed_count_ = selection_changed_count_ = 0;
    return result;
  }

  // LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int index) OVERRIDE {
    added_count_++;
  }
  virtual void LauncherItemRemoved(int index) OVERRIDE {
    removed_count_++;
  }
  virtual void LauncherSelectionChanged() OVERRIDE {
    selection_changed_count_++;
  }

  private:
  int added_count_;
  int removed_count_;
  int selection_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherModelObserver);
};

}  // namespace

TEST(TestLauncher, BasicAssertions) {
  TestLauncherModelObserver observer;
  LauncherModel model;
  // Add an item.
  model.AddObserver(&observer);
  EXPECT_EQ(-1, model.selected_index());
  View* view = new View;
  model.AddItem(view, 0, true);
  EXPECT_EQ(1, model.item_count());
  EXPECT_EQ(view, model.view_at(0));
  EXPECT_EQ(true, model.is_draggable(0));
  EXPECT_EQ(-1, model.selected_index());
  EXPECT_EQ("added=1 removed=0 s_changed=0", observer.StateStringAndClear());

  EXPECT_EQ(0, model.IndexOfItemByView(view));
  EXPECT_EQ(-1, model.IndexOfItemByView(NULL));

  // Change the selection.
  model.SetSelectedIndex(0);
  EXPECT_EQ("added=0 removed=0 s_changed=1", observer.StateStringAndClear());

  // Remove the item.
  model.RemoveItemAt(0);
  delete view;  // We now own the view.
  view = NULL;
  EXPECT_EQ(0, model.item_count());
  EXPECT_EQ(0, model.selected_index());
  EXPECT_EQ("added=0 removed=1 s_changed=0", observer.StateStringAndClear());
}

}  // namespace aura_shell
