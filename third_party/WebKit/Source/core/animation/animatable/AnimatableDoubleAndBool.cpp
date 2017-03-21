// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/animatable/AnimatableDoubleAndBool.h"


namespace blink {

bool AnimatableDoubleAndBool::equalTo(const AnimatableValue* value) const {
  const AnimatableDoubleAndBool* other = toAnimatableDoubleAndBool(value);
  return m_number == other->m_number && m_flag == other->m_flag;
}

}  // namespace blink
