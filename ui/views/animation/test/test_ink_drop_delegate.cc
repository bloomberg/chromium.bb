// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_delegate.h"

namespace views {
namespace test {

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

}  // namespace test
}  // namespace views
