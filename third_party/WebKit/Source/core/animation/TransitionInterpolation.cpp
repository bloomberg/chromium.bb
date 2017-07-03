// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/TransitionInterpolation.h"

#include "core/animation/CSSInterpolationTypesMap.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

void TransitionInterpolation::Interpolate(int iteration, double fraction) {
  if (cached_fraction_ != fraction || cached_iteration_ != iteration) {
    if (fraction != 0 && fraction != 1) {
      merge_.start_interpolable_value->Interpolate(
          *merge_.end_interpolable_value, fraction,
          *cached_interpolable_value_);
    }
    cached_iteration_ = iteration;
    cached_fraction_ = fraction;
  }
}

const InterpolableValue& TransitionInterpolation::CurrentInterpolableValue()
    const {
  if (cached_fraction_ == 0) {
    return *start_.interpolable_value;
  }
  if (cached_fraction_ == 1) {
    return *end_.interpolable_value;
  }
  return *cached_interpolable_value_;
}

NonInterpolableValue* TransitionInterpolation::CurrentNonInterpolableValue()
    const {
  if (cached_fraction_ == 0) {
    return start_.non_interpolable_value.Get();
  }
  if (cached_fraction_ == 1) {
    return end_.non_interpolable_value.Get();
  }
  return merge_.non_interpolable_value.Get();
}

void TransitionInterpolation::Apply(StyleResolverState& state) const {
  CSSInterpolationTypesMap map(state.GetDocument().GetPropertyRegistry());
  CSSInterpolationEnvironment environment(map, state, nullptr);
  type_.Apply(CurrentInterpolableValue(), CurrentNonInterpolableValue(),
              environment);
}

std::unique_ptr<TypedInterpolationValue>
TransitionInterpolation::GetInterpolatedValue() const {
  return TypedInterpolationValue::Create(
      type_, CurrentInterpolableValue().Clone(), CurrentNonInterpolableValue());
}

RefPtr<AnimatableValue>
TransitionInterpolation::GetInterpolatedCompositorValue() const {
  return AnimatableValue::Interpolate(compositor_start_.Get(),
                                      compositor_end_.Get(), cached_fraction_);
}

}  // namespace blink
