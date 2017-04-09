// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/animatable/AnimatableDoubleAndBool.h"


namespace blink {

bool AnimatableDoubleAndBool::EqualTo(const AnimatableValue* value) const {
  const AnimatableDoubleAndBool* other = ToAnimatableDoubleAndBool(value);
  return number_ == other->number_ && flag_ == other->flag_;
}

}  // namespace blink
