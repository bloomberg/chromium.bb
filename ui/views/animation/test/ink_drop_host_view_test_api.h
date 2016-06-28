// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_VIEW_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_VIEW_TEST_API_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"

namespace views {
namespace test {

// Test API to provide internal access to an InkDropHostView instance.
class InkDropHostViewTestApi {
 public:
  explicit InkDropHostViewTestApi(InkDropHostView* host_view);
  ~InkDropHostViewTestApi();

  void SetHasInkDrop(bool has_an_ink_drop);

  void SetInkDrop(std::unique_ptr<InkDrop> ink_drop);
  InkDrop* ink_drop() { return host_view_->ink_drop(); }

  bool HasGestureHandler() const;

  // Wrapper for InkDropHostView::GetInkDropCenterBasedOnLastEvent().
  gfx::Point GetInkDropCenterBasedOnLastEvent() const;

  // Wrapper for InkDropHostView::AnimateInkDrop().
  void AnimateInkDrop(InkDropState state, const ui::LocatedEvent* event);

 private:
  // The InkDropHostView to provide internal access to.
  InkDropHostView* host_view_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHostViewTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_VIEW_TEST_API_H_
