// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation.h"

namespace views {

const float InkDropAnimation::kHiddenOpacity = 0.f;
const float InkDropAnimation::kVisibleOpacity = 0.11f;

InkDropAnimation::InkDropAnimation() {}

InkDropAnimation::~InkDropAnimation() {}

void InkDropAnimation::AddObserver(InkDropAnimationObserver* observer) {
  observers_.AddObserver(observer);
}

void InkDropAnimation::RemoveObserver(InkDropAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InkDropAnimation::NotifyAnimationStarted(InkDropState ink_drop_state) {
  FOR_EACH_OBSERVER(InkDropAnimationObserver, observers_,
                    InkDropAnimationStarted(ink_drop_state));
}

void InkDropAnimation::NotifyAnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationObserver::InkDropAnimationEndedReason reason) {
  FOR_EACH_OBSERVER(InkDropAnimationObserver, observers_,
                    InkDropAnimationEnded(ink_drop_state, reason));
}

}  // namespace views
