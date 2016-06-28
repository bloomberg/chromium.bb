// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_host_view_test_api.h"

namespace views {
namespace test {

InkDropHostViewTestApi::InkDropHostViewTestApi(InkDropHostView* host_view)
    : host_view_(host_view) {}

InkDropHostViewTestApi::~InkDropHostViewTestApi() {}

void InkDropHostViewTestApi::SetHasInkDrop(bool has_an_ink_drop) {
  host_view_->SetHasInkDrop(has_an_ink_drop);
}

void InkDropHostViewTestApi::SetInkDrop(std::unique_ptr<InkDrop> ink_drop) {
  host_view_->SetHasInkDrop(true);
  host_view_->ink_drop_ = std::move(ink_drop);
}

bool InkDropHostViewTestApi::HasGestureHandler() const {
  return host_view_->gesture_handler_ != nullptr;
}

gfx::Point InkDropHostViewTestApi::GetInkDropCenterBasedOnLastEvent() const {
  return host_view_->GetInkDropCenterBasedOnLastEvent();
}

void InkDropHostViewTestApi::AnimateInkDrop(InkDropState state,
                                            const ui::LocatedEvent* event) {
  host_view_->AnimateInkDrop(state, event);
}

}  // namespace test
}  // namespace views
