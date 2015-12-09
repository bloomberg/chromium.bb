// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_delegate.h"

#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/test/views_test_base.h"

namespace views {

namespace {

// An InkDropDelegate that keeps track of order of deletions.
class TestInkDropDelegate : public InkDropDelegate {
 public:
  TestInkDropDelegate(InkDropHost* ink_drop_host,
                      bool* button_deleted,
                      bool* delegate_deleted)
      : ink_drop_host_(ink_drop_host),
        button_deleted_(button_deleted),
        delegate_deleted_(delegate_deleted) {
    ink_drop_host_->AddInkDropLayer(nullptr);
  }
  ~TestInkDropDelegate() override {
    EXPECT_FALSE(*button_deleted_);
    ink_drop_host_->RemoveInkDropLayer(nullptr);
    *delegate_deleted_ = true;
  }

  // InkDropDelegate:
  void SetInkDropSize(int large_size,
                      int large_corner_radius,
                      int small_size,
                      int small_corner_radius) override {}
  void OnLayout() override {}
  void OnAction(InkDropState state) override {}

 private:
  InkDropHost* ink_drop_host_;
  bool* button_deleted_;
  bool* delegate_deleted_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropDelegate);
};

// A test Button class that owns a TestInkDropDelegate.
class TestButton : public views::CustomButton, public views::InkDropHost {
 public:
  TestButton(bool* layer_added,
             bool* layer_removed,
             bool* button_deleted,
             bool* delegate_deleted)
      : CustomButton(nullptr),
        layer_added_(layer_added),
        layer_removed_(layer_removed),
        button_deleted_(button_deleted),
        ink_drop_delegate_(
            new TestInkDropDelegate(this, button_deleted, delegate_deleted)) {
    set_ink_drop_delegate(ink_drop_delegate_.get());
    EXPECT_TRUE(*layer_added_);
  }
  ~TestButton() override {
    ink_drop_delegate_.reset();
    set_ink_drop_delegate(nullptr);
    EXPECT_TRUE(*layer_removed_);
    *button_deleted_ = true;
  }

  // views::InkDropHost:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override {
    *layer_added_ = true;
  }
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override {
    *layer_removed_ = true;
  }
  gfx::Point CalculateInkDropCenter() const override { return gfx::Point(); }

 private:
  bool* layer_added_;
  bool* layer_removed_;
  bool* button_deleted_;
  scoped_ptr<views::InkDropDelegate> ink_drop_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestButton);
};

}  // namespace

// Test that an InkDropDelegate gets safely deleted when a CustomButton is
// destroyed.
TEST(InkDropDelegateTest, CanBeDeleted) {
  bool layer_added = false;
  bool layer_removed = false;
  bool button_deleted = false;
  bool delegate_deleted = false;

  TestButton* button = new TestButton(&layer_added, &layer_removed,
                                      &button_deleted, &delegate_deleted);
  EXPECT_TRUE(layer_added);
  EXPECT_FALSE(layer_removed);
  EXPECT_FALSE(button_deleted);
  EXPECT_FALSE(delegate_deleted);

  delete button;
  EXPECT_TRUE(layer_added);
  EXPECT_TRUE(layer_removed);
  EXPECT_TRUE(button_deleted);
  EXPECT_TRUE(delegate_deleted);
}

}  // namespace views
