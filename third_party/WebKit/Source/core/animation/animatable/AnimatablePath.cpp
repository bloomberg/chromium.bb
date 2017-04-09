// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/animatable/AnimatablePath.h"

#include "core/style/DataEquivalency.h"

namespace blink {

bool AnimatablePath::EqualTo(const AnimatableValue* value) const {
  return DataEquivalent(path_.Get(), ToAnimatablePath(value)->path_.Get());
}

}  // namespace blink
