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

// TODO(alancutter): This class will replace *StyleInterpolation and
// Interpolation. For now it needs to distinguish itself during the refactor and
// temporarily has an ugly name.
class CORE_EXPORT InvalidatableInterpolation : public Interpolation {
 public:
  static PassRefPtr<InvalidatableInterpolation> create(
      const PropertyHandle& property,
      PassRefPtr<PropertySpecificKeyframe> startKeyframe,
      PassRefPtr<PropertySpecificKeyframe> endKeyframe) {
    return adoptRef(new InvalidatableInterpolation(
        property, std::move(startKeyframe), std::move(endKeyframe)));
  }

  const PropertyHandle& getProperty() const final { return m_property; }
  virtual void interpolate(int iteration, double fraction);
  bool dependsOnUnderlyingValue() const final;
  static void applyStack(const ActiveInterpolations&,
                         InterpolationEnvironment&);

  virtual bool isInvalidatableInterpolation() const { return true; }

 private:
  InvalidatableInterpolation(const PropertyHandle& property,
                             PassRefPtr<PropertySpecificKeyframe> startKeyframe,
                             PassRefPtr<PropertySpecificKeyframe> endKeyframe)
      : Interpolation(),
        m_property(property),
        m_interpolationTypes(nullptr),
        m_interpolationTypesVersion(0),
        m_startKeyframe(startKeyframe),
        m_endKeyframe(endKeyframe),
        m_currentFraction(std::numeric_limits<double>::quiet_NaN()),
        m_isConversionCached(false) {}

  using ConversionCheckers = InterpolationType::ConversionCheckers;

  std::unique_ptr<TypedInterpolationValue> maybeConvertUnderlyingValue(
      const InterpolationEnvironment&) const;
  const TypedInterpolationValue* ensureValidConversion(
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  void ensureValidInterpolationTypes(const InterpolationEnvironment&) const;
  void clearConversionCache() const;
  bool isConversionCacheValid(const InterpolationEnvironment&,
                              const UnderlyingValueOwner&) const;
  bool isNeutralKeyframeActive() const;
  std::unique_ptr<PairwisePrimitiveInterpolation> maybeConvertPairwise(
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  std::unique_ptr<TypedInterpolationValue> convertSingleKeyframe(
      const PropertySpecificKeyframe&,
      const InterpolationEnvironment&,
      const UnderlyingValueOwner&) const;
  void addConversionCheckers(const InterpolationType&,
                             ConversionCheckers&) const;
  void setFlagIfInheritUsed(InterpolationEnvironment&) const;
  double underlyingFraction() const;

  const PropertyHandle m_property;
  mutable const InterpolationTypes* m_interpolationTypes;
  mutable size_t m_interpolationTypesVersion;
  RefPtr<PropertySpecificKeyframe> m_startKeyframe;
  RefPtr<PropertySpecificKeyframe> m_endKeyframe;
  double m_currentFraction;
  mutable bool m_isConversionCached;
  mutable std::unique_ptr<PrimitiveInterpolation> m_cachedPairConversion;
  mutable ConversionCheckers m_conversionCheckers;
  mutable std::unique_ptr<TypedInterpolationValue> m_cachedValue;
};

DEFINE_TYPE_CASTS(InvalidatableInterpolation,
                  Interpolation,
                  value,
                  value->isInvalidatableInterpolation(),
                  value.isInvalidatableInterpolation());

}  // namespace blink

#endif  // InvalidatableInterpolation_h
