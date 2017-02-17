// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/TransitionInterpolation.h"

#include "core/animation/CSSInterpolationTypesMap.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

void TransitionInterpolation::interpolate(int iteration, double fraction) {
  if (m_cachedFraction != fraction || m_cachedIteration != iteration) {
    if (fraction != 0 && fraction != 1) {
      m_merge.startInterpolableValue->interpolate(
          *m_merge.endInterpolableValue, fraction, *m_cachedInterpolableValue);
    }
    m_cachedIteration = iteration;
    m_cachedFraction = fraction;
  }
}

const InterpolableValue& TransitionInterpolation::currentInterpolableValue()
    const {
  if (m_cachedFraction == 0) {
    return *m_start.interpolableValue;
  }
  if (m_cachedFraction == 1) {
    return *m_end.interpolableValue;
  }
  return *m_cachedInterpolableValue;
}

NonInterpolableValue* TransitionInterpolation::currentNonInterpolableValue()
    const {
  if (m_cachedFraction == 0) {
    return m_start.nonInterpolableValue.get();
  }
  if (m_cachedFraction == 1) {
    return m_end.nonInterpolableValue.get();
  }
  return m_merge.nonInterpolableValue.get();
}

void TransitionInterpolation::apply(StyleResolverState& state) const {
  CSSInterpolationTypesMap map(state.document().propertyRegistry());
  InterpolationEnvironment environment(map, state);
  m_type.apply(currentInterpolableValue(), currentNonInterpolableValue(),
               environment);
}

std::unique_ptr<TypedInterpolationValue>
TransitionInterpolation::getInterpolatedValue() const {
  return TypedInterpolationValue::create(m_type,
                                         currentInterpolableValue().clone(),
                                         currentNonInterpolableValue());
}

RefPtr<AnimatableValue>
TransitionInterpolation::getInterpolatedCompositorValue() const {
  return AnimatableValue::interpolate(m_compositorStart.get(),
                                      m_compositorEnd.get(), m_cachedFraction);
}

}  // namespace blink
