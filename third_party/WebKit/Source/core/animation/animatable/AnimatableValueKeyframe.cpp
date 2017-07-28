// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/animatable/AnimatableValueKeyframe.h"

#include "core/animation/LegacyStyleInterpolation.h"

namespace blink {

AnimatableValueKeyframe::AnimatableValueKeyframe(
    const AnimatableValueKeyframe& copy_from)
    : Keyframe(copy_from.offset_, copy_from.composite_, copy_from.easing_) {
  for (PropertyValueMap::const_iterator iter =
           copy_from.property_values_.begin();
       iter != copy_from.property_values_.end(); ++iter)
    SetPropertyValue(iter->key, iter->value.Get());
}

PropertyHandleSet AnimatableValueKeyframe::Properties() const {
  // This is not used in time-critical code, so we probably don't need to
  // worry about caching this result.
  PropertyHandleSet properties;
  for (PropertyValueMap::const_iterator iter = property_values_.begin();
       iter != property_values_.end(); ++iter)
    properties.insert(PropertyHandle(*iter.Keys()));
  return properties;
}

RefPtr<Keyframe> AnimatableValueKeyframe::Clone() const {
  return AdoptRef(new AnimatableValueKeyframe(*this));
}

RefPtr<Keyframe::PropertySpecificKeyframe>
AnimatableValueKeyframe::CreatePropertySpecificKeyframe(
    const PropertyHandle& property) const {
  return PropertySpecificKeyframe::Create(
      Offset(), &Easing(), PropertyValue(property.CssProperty()), Composite());
}

RefPtr<Keyframe::PropertySpecificKeyframe>
AnimatableValueKeyframe::PropertySpecificKeyframe::CloneWithOffset(
    double offset) const {
  return Create(offset, easing_, value_, composite_);
}

RefPtr<Interpolation>
AnimatableValueKeyframe::PropertySpecificKeyframe::CreateInterpolation(
    const PropertyHandle& property,
    const Keyframe::PropertySpecificKeyframe& end) const {
  return LegacyStyleInterpolation::Create(
      Value(), ToAnimatableValuePropertySpecificKeyframe(end).Value(),
      property.CssProperty());
}

}  // namespace blink
