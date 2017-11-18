// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/Keyframe.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/animation/EffectModel.h"
#include "core/animation/InvalidatableInterpolation.h"

namespace blink {

namespace {
StringView CompositeOperationToString(EffectModel::CompositeOperation op) {
  switch (op) {
    case EffectModel::kCompositeAdd:
      return "add";
    case EffectModel::kCompositeReplace:
      return "replace";
    default:
      NOTREACHED();
      return "";
  }
}
}  // namespace

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
  object_builder.Add("offset", offset_);
  object_builder.Add("easing", easing_->ToString());
  // TODO(crbug.com/785526): This should be absent if it matches the composite
  // operation of the keyframe effect (which is not yet implemented).
  object_builder.AddString("composite", CompositeOperationToString(composite_));
}

bool Keyframe::CompareOffsets(const scoped_refptr<Keyframe>& a,
                              const scoped_refptr<Keyframe>& b) {
  return a->Offset() < b->Offset();
}

}  // namespace blink
