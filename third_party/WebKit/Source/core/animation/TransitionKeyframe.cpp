// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/TransitionKeyframe.h"

#include "core/animation/CompositorAnimations.h"
#include "core/animation/InterpolationType.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include "core/animation/TransitionInterpolation.h"

namespace blink {

void TransitionKeyframe::setCompositorValue(
    RefPtr<AnimatableValue> compositorValue) {
  DCHECK_EQ(
      CompositorAnimations::isCompositableProperty(m_property.cssProperty()),
      static_cast<bool>(compositorValue.get()));
  m_compositorValue = std::move(compositorValue);
}

PropertyHandleSet TransitionKeyframe::properties() const {
  PropertyHandleSet result;
  result.insert(m_property);
  return result;
}

PassRefPtr<Keyframe::PropertySpecificKeyframe>
TransitionKeyframe::createPropertySpecificKeyframe(
    const PropertyHandle& property) const {
  DCHECK(property == m_property);
  return PropertySpecificKeyframe::create(offset(), &easing(), composite(),
                                          m_value->clone(), m_compositorValue);
}

PassRefPtr<Interpolation>
TransitionKeyframe::PropertySpecificKeyframe::createInterpolation(
    const PropertyHandle& property,
    const Keyframe::PropertySpecificKeyframe& otherSuperClass) const {
  const PropertySpecificKeyframe& other =
      toTransitionPropertySpecificKeyframe(otherSuperClass);
  DCHECK(m_value->type() == other.m_value->type());
  return TransitionInterpolation::create(
      property, m_value->type(), m_value->value().clone(),
      other.m_value->value().clone(), m_compositorValue,
      other.m_compositorValue);
}

}  // namespace blink
