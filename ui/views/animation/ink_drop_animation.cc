// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation.h"

namespace views {

const float InkDropAnimation::kHiddenOpacity = 0.f;
const float InkDropAnimation::kVisibleOpacity = 0.175f;

InkDropAnimation::InkDropAnimation() : observer_(nullptr) {}

InkDropAnimation::~InkDropAnimation() {}

void InkDropAnimation::NotifyAnimationStarted(InkDropState ink_drop_state) {
  observer_->InkDropAnimationStarted(ink_drop_state);
}

void InkDropAnimation::NotifyAnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationObserver::InkDropAnimationEndedReason reason) {
  observer_->InkDropAnimationEnded(ink_drop_state, reason);
  // |this| may be deleted!
}

}  // namespace views
