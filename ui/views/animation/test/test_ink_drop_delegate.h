// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_DELEGATE_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_DELEGATE_H_

#include "base/macros.h"
#include "ui/views/animation/ink_drop_delegate.h"

namespace views {
namespace test {

class TestInkDropDelegate : public InkDropDelegate {
 public:
  TestInkDropDelegate();
  ~TestInkDropDelegate() override;

  bool is_hovered() const { return is_hovered_; }

  // InkDropDelegate:
  void OnAction(InkDropState state) override;
  void SnapToActivated() override;
  void SetHovered(bool is_hovered) override;
  InkDropState GetTargetInkDropState() const override;

 private:
  InkDropState state_;

  bool is_hovered_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropDelegate);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_DELEGATE_H_
