// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/animatable/AnimatablePath.h"

#include "core/style/DataEquivalency.h"

namespace blink {

bool AnimatablePath::equalTo(const AnimatableValue* value) const {
  return dataEquivalent(m_path.get(), toAnimatablePath(value)->m_path.get());
}

}  // namespace blink
