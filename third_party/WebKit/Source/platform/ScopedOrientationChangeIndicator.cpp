// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/ScopedOrientationChangeIndicator.h"

#include "platform/wtf/Assertions.h"

namespace blink {

ScopedOrientationChangeIndicator::State
    ScopedOrientationChangeIndicator::state_ =
        ScopedOrientationChangeIndicator::State::kNotProcessing;

ScopedOrientationChangeIndicator::ScopedOrientationChangeIndicator() {
  DCHECK(IsMainThread());

  previous_state_ = state_;
  state_ = State::kProcessing;
}

ScopedOrientationChangeIndicator::~ScopedOrientationChangeIndicator() {
  DCHECK(IsMainThread());
  state_ = previous_state_;
}

// static
bool ScopedOrientationChangeIndicator::ProcessingOrientationChange() {
  DCHECK(IsMainThread());
  return state_ == State::kProcessing;
}

}  // namespace blink
