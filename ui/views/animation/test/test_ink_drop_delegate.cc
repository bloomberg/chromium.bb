// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_delegate.h"

namespace views {
namespace test {

TestInkDropDelegate::TestInkDropDelegate() {}

TestInkDropDelegate::~TestInkDropDelegate() {}

void TestInkDropDelegate::OnAction(InkDropState state) {
  ink_drop_.AnimateToState(state);
}

void TestInkDropDelegate::SnapToActivated() {
  ink_drop_.SnapToActivated();
}

void TestInkDropDelegate::SetHovered(bool is_hovered) {
  ink_drop_.SetHovered(is_hovered);
}

InkDropState TestInkDropDelegate::GetTargetInkDropState() const {
  return ink_drop_.GetTargetInkDropState();
}

InkDrop* TestInkDropDelegate::GetInkDrop() {
  return &ink_drop_;
}

}  // namespace test
}  // namespace views
