// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/Keyframe.h"

#include "core/animation/InvalidatableInterpolation.h"

namespace blink {

PassRefPtr<Interpolation>
Keyframe::PropertySpecificKeyframe::createInterpolation(
    const PropertyHandle& propertyHandle,
    const Keyframe::PropertySpecificKeyframe& end) const {
  // const_cast to take refs.
  return InvalidatableInterpolation::create(
      propertyHandle, const_cast<PropertySpecificKeyframe*>(this),
      const_cast<PropertySpecificKeyframe*>(&end));
}

}  // namespace blink
