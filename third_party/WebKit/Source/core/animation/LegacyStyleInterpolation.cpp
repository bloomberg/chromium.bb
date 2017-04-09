// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LegacyStyleInterpolation.h"

#include "core/css/resolver/AnimatedStyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"

#include <memory>

namespace blink {

namespace {

bool TypesMatch(const InterpolableValue* start, const InterpolableValue* end) {
  if (start == end)
    return true;
  if (start->IsNumber())
    return end->IsNumber();
  if (start->IsBool())
    return end->IsBool();
  if (start->IsAnimatableValue())
    return end->IsAnimatableValue();
  if (!(start->IsList() && end->IsList()))
    return false;
  const InterpolableList* start_list = ToInterpolableList(start);
  const InterpolableList* end_list = ToInterpolableList(end);
  if (start_list->length() != end_list->length())
    return false;
  for (size_t i = 0; i < start_list->length(); ++i) {
    if (!TypesMatch(start_list->Get(i), end_list->Get(i)))
      return false;
  }
  return true;
}

}  // namespace

LegacyStyleInterpolation::LegacyStyleInterpolation(
    std::unique_ptr<InterpolableValue> start,
    std::unique_ptr<InterpolableValue> end,
    CSSPropertyID id)
    : Interpolation(),
      start_(std::move(start)),
      end_(std::move(end)),
      property_(id),
      cached_fraction_(0),
      cached_iteration_(0),
      cached_value_(start_ ? start_->Clone() : nullptr) {
  RELEASE_ASSERT(TypesMatch(start_.get(), end_.get()));
}

void LegacyStyleInterpolation::Apply(StyleResolverState& state) const {
  AnimatedStyleBuilder::ApplyProperty(Id(), *state.Style(),
                                      CurrentValue().Get());
}

void LegacyStyleInterpolation::Interpolate(int iteration, double fraction) {
  if (cached_fraction_ != fraction || cached_iteration_ != iteration) {
    start_->Interpolate(*end_, fraction, *cached_value_);
    cached_iteration_ = iteration;
    cached_fraction_ = fraction;
  }
}

}  // namespace blink
