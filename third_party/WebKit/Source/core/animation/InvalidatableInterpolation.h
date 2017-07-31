// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InvalidatableInterpolation_h
#define InvalidatableInterpolation_h

#include "core/animation/Interpolation.h"
#include "core/animation/InterpolationType.h"
#include "core/animation/InterpolationTypesMap.h"
#include "core/animation/PrimitiveInterpolation.h"
#include "core/animation/TypedInterpolationValue.h"
#include <memory>

namespace blink {

// See the documentation of Interpolation for general information about this
// class hierarchy.
//
// The InvalidatableInterpolation subclass stores the start and end keyframes as
// PropertySpecificKeyframe objects.
//
// InvalidatableInterpolation uses conversion checkers and the interpolation
// environment to respond to changes to the underlying property value during
// interpolation.
//
// InvalidatableInterpolation is used to implement additive animations. During
// the effect application phase of animation computation, the current animated
// value of the property is applied to the element by calling the static
// ApplyStack function with an ordered list of InvalidatableInterpolation
// objects.
class CORE_EXPORT InvalidatableInterpolation : public Interpolation {
 public:
  static RefPtr<InvalidatableInterpolation> Create(
      const PropertyHandle& property,
      RefPtr<PropertySpecificKeyframe> start_keyframe,
      RefPtr<PropertySpecificKeyframe> end_keyframe) {
    return AdoptRef(new InvalidatableInterpolation(
        property, std::move(start_keyframe), std::move(end_keyframe)));
  }

  const PropertyHandle& GetProperty() const final { return property_; }
  virtual void Interpolate(int iteration, double fraction);
  bool DependsOnUnderlyingValue() const final;
  static void ApplyStack(const ActiveInterpolations&,
                         InterpolationEnvironment&);

  virtual bool IsInvalidatableInterpolation() const { return true; }

 private:
  InvalidatableInterpolation(const PropertyHandle& property,
                             RefPtr<PropertySpecificKeyframe> start_keyframe,
                             RefPtr<PropertySpecificKeyframe> end_keyframe)
      : Interpolation(),
        property_(property),
        interpolation_types_(nullptr),
        interpolation_types_version_(0),
        start_keyframe_(std::move(start_keyframe)),
        end_keyframe_(std::move(end_keyframe)),
        current_fraction_(std::numeric_limits<double>::quiet_NaN()),
        is_conversion_cached_(false) {}

  using ConversionCheckers = InterpolationType::ConversionCheckers;

  std::unique_ptr<TypedInterpolationValue> MaybeConvertUnderlyingValue(
      const InterpolationEnvironment&) const;
  const TypedInterpolationValue* EnsureValidConversion(
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  void EnsureValidInterpolationTypes(const InterpolationEnvironment&) const;
  void ClearConversionCache() const;
  bool IsConversionCacheValid(const InterpolationEnvironment&,
                              const UnderlyingValueOwner&) const;
  bool IsNeutralKeyframeActive() const;
  std::unique_ptr<PairwisePrimitiveInterpolation> MaybeConvertPairwise(
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  std::unique_ptr<TypedInterpolationValue> ConvertSingleKeyframe(
      const PropertySpecificKeyframe&,
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  void AddConversionCheckers(const InterpolationType&,
                             ConversionCheckers&) const;
  void SetFlagIfInheritUsed(InterpolationEnvironment&) const;
  double UnderlyingFraction() const;

  const PropertyHandle property_;
  mutable const InterpolationTypes* interpolation_types_;
  mutable size_t interpolation_types_version_;
  RefPtr<PropertySpecificKeyframe> start_keyframe_;
  RefPtr<PropertySpecificKeyframe> end_keyframe_;
  double current_fraction_;
  mutable bool is_conversion_cached_;
  mutable std::unique_ptr<PrimitiveInterpolation> cached_pair_conversion_;
  mutable ConversionCheckers conversion_checkers_;
  mutable std::unique_ptr<TypedInterpolationValue> cached_value_;
};

DEFINE_TYPE_CASTS(InvalidatableInterpolation,
                  Interpolation,
                  value,
                  value->IsInvalidatableInterpolation(),
                  value.IsInvalidatableInterpolation());

}  // namespace blink

#endif  // InvalidatableInterpolation_h
