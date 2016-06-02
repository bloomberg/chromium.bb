// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/test_ink_drop_delegate.h"

namespace views {
namespace test {

class TestInkDrop : public InkDrop {
 public:
  TestInkDrop() : InkDrop() {}
  ~TestInkDrop() override {}

  InkDropState GetTargetInkDropState() const override {
    NOTREACHED();
    return InkDropState::HIDDEN;
  }

  bool IsVisible() const override {
    NOTREACHED();
    return false;
  }
  void AnimateToState(InkDropState ink_drop_state) override { NOTREACHED(); }
  void SnapToActivated() override { NOTREACHED(); }
  void SetHovered(bool is_hovered) override { NOTREACHED(); }
  void SetFocused(bool is_focused) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInkDrop);
};

TestInkDropDelegate::TestInkDropDelegate()
    : state_(InkDropState::HIDDEN), is_hovered_(false) {}

TestInkDropDelegate::~TestInkDropDelegate() {}

void TestInkDropDelegate::OnAction(InkDropState state) {
  state_ = state;
}

void TestInkDropDelegate::SnapToActivated() {
  state_ = InkDropState::ACTIVATED;
}

void TestInkDropDelegate::SetHovered(bool is_hovered) {
  is_hovered_ = is_hovered;
}

InkDropState TestInkDropDelegate::GetTargetInkDropState() const {
  return state_;
}

InkDrop* TestInkDropDelegate::GetInkDrop() {
  if (!ink_drop_.get())
    ink_drop_.reset(new TestInkDrop);
  return ink_drop_.get();
}

}  // namespace test
}  // namespace views
