// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/Keyframe.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/animation/EffectModel.h"
#include "core/animation/InvalidatableInterpolation.h"

namespace blink {

scoped_refptr<Interpolation>
Keyframe::PropertySpecificKeyframe::CreateInterpolation(
    const PropertyHandle& property_handle,
    const Keyframe::PropertySpecificKeyframe& end) const {
  // const_cast to take refs.
  return InvalidatableInterpolation::Create(
      property_handle, const_cast<PropertySpecificKeyframe*>(this),
      const_cast<PropertySpecificKeyframe*>(&end));
}

void Keyframe::AddKeyframePropertiesToV8Object(
    V8ObjectBuilder& object_builder) const {
  if (offset_) {
    object_builder.Add("offset", offset_.value());
  } else {
    object_builder.AddNull("offset");
  }
  object_builder.Add("easing", easing_->ToString());
  if (composite_) {
    object_builder.AddString(
        "composite",
        EffectModel::CompositeOperationToString(composite_.value()));
  } else {
    object_builder.AddNull("composite");
  }
}

}  // namespace blink
