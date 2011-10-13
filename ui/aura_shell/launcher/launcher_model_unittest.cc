// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_model.h"

#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura_shell/launcher/launcher_model_observer.h"

namespace aura_shell {

namespace {

// LauncherModelObserver implementation that tracks what message are invoked.
class TestLauncherModelObserver : public LauncherModelObserver {
 public:
  TestLauncherModelObserver()
      : added_count_(0),
        removed_count_(0),
        images_changed_count_(0) {
  }

  // Returns a string description of the changes that have occurred since this
  // was last invoked. Resets state to initial state.
  std::string StateStringAndClear() {
    std::string result(
        base::StringPrintf("added=%d removed=%d images_changed=%d",
        added_count_, removed_count_, images_changed_count_));
    added_count_ = removed_count_ = images_changed_count_ = 0;
    return result;
  }

  // LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int index) OVERRIDE {
    added_count_++;
  }
  virtual void LauncherItemRemoved(int index) OVERRIDE {
    removed_count_++;
  }
  virtual void LauncherItemImagesChanged(int index) OVERRIDE {
    images_changed_count_++;
  }

 private:
  int added_count_;
  int removed_count_;
  int images_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherModelObserver);
};

}  // namespace

TEST(TestLauncher, BasicAssertions) {
  TestLauncherModelObserver observer;
  LauncherModel model;
  // Add an item.
  model.AddObserver(&observer);
  LauncherItem item;
  model.Add(0, item);
  EXPECT_EQ(1, model.item_count());
  EXPECT_EQ("added=1 removed=0 images_changed=0",
            observer.StateStringAndClear());

  // Change a tabbed image.
  model.SetTabbedImages(0, LauncherTabbedImages());
  EXPECT_EQ("added=0 removed=0 images_changed=1",
            observer.StateStringAndClear());

  // Remove the item.
  model.RemoveItemAt(0);
  EXPECT_EQ(0, model.item_count());
  EXPECT_EQ("added=0 removed=1 images_changed=0",
            observer.StateStringAndClear());

  // Add an app item.
  item.type = TYPE_APP;
  model.Add(0, item);
  observer.StateStringAndClear();

  // Change an app image.
  model.SetAppImage(0, SkBitmap());
  EXPECT_EQ("added=0 removed=0 images_changed=1",
            observer.StateStringAndClear());
}

}  // namespace aura_shell
