// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InvalidatableInterpolation_h
#define InvalidatableInterpolation_h

#include "core/animation/InterpolationType.h"
#include "core/animation/PrimitiveInterpolation.h"
#include "core/animation/StyleInterpolation.h"
#include "core/animation/TypedInterpolationValue.h"

namespace blink {

using InterpolationTypes = Vector<OwnPtr<const InterpolationType>>;

// TODO(alancutter): This class will replace *StyleInterpolation, SVGInterpolation, Interpolation.
// For now it needs to distinguish itself during the refactor and temporarily has an ugly name.
class CORE_EXPORT InvalidatableInterpolation : public Interpolation {
public:
    static PassRefPtr<InvalidatableInterpolation> create(
        PropertyHandle property,
        const InterpolationTypes& interpolationTypes,
        const PropertySpecificKeyframe& startKeyframe,
        const PropertySpecificKeyframe& endKeyframe)
    {
        return adoptRef(new InvalidatableInterpolation(property, interpolationTypes, startKeyframe, endKeyframe));
    }

    PropertyHandle property() const final { return m_property; }
    virtual void interpolate(int iteration, double fraction);
    bool dependsOnUnderlyingValue() const;
    virtual void apply(InterpolationEnvironment&) const { ASSERT_NOT_REACHED(); }
    static void applyStack(const ActiveInterpolations&, InterpolationEnvironment&);

    virtual bool isInvalidatableInterpolation() const { return true; }

private:
    InvalidatableInterpolation(
        PropertyHandle property,
        const InterpolationTypes& interpolationTypes,
        const PropertySpecificKeyframe& startKeyframe,
        const PropertySpecificKeyframe& endKeyframe)
        : Interpolation(nullptr, nullptr)
        , m_property(property)
        , m_interpolationTypes(interpolationTypes)
        , m_startKeyframe(&startKeyframe)
        , m_endKeyframe(&endKeyframe)
        , m_currentFraction(std::numeric_limits<double>::quiet_NaN())
        , m_isCached(false)
    { }

    using ConversionCheckers = InterpolationType::ConversionCheckers;

    PassOwnPtr<TypedInterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const;
    const TypedInterpolationValue* ensureValidInterpolation(const InterpolationEnvironment&, const UnderlyingValueOwner&) const;
    void clearCache() const;
    bool isCacheValid(const InterpolationEnvironment&, const UnderlyingValueOwner&) const;
    bool isNeutralKeyframeActive() const;
    PassOwnPtr<PairwisePrimitiveInterpolation> maybeConvertPairwise(const InterpolationEnvironment&, const UnderlyingValueOwner&) const;
    PassOwnPtr<TypedInterpolationValue> convertSingleKeyframe(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const UnderlyingValueOwner&) const;
    void addConversionCheckers(const InterpolationType&, ConversionCheckers&) const;
    void setFlagIfInheritUsed(InterpolationEnvironment&) const;
    double underlyingFraction() const;

    const PropertyHandle m_property;
    const InterpolationTypes& m_interpolationTypes;
    const PropertySpecificKeyframe* m_startKeyframe;
    const PropertySpecificKeyframe* m_endKeyframe;
    double m_currentFraction;
    mutable bool m_isCached;
    mutable OwnPtr<PrimitiveInterpolation> m_cachedPairConversion;
    mutable ConversionCheckers m_conversionCheckers;
    mutable OwnPtr<TypedInterpolationValue> m_cachedValue;
};

DEFINE_TYPE_CASTS(InvalidatableInterpolation, Interpolation, value, value->isInvalidatableInterpolation(), value.isInvalidatableInterpolation());

} // namespace blink

#endif // InvalidatableInterpolation_h
