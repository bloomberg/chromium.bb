// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InvalidatableStyleInterpolation_h
#define InvalidatableStyleInterpolation_h

#include "core/animation/InterpolationType.h"
#include "core/animation/InterpolationValue.h"
#include "core/animation/PrimitiveInterpolation.h"
#include "core/animation/StyleInterpolation.h"

namespace blink {

// TODO(alancutter): This class will replace *StyleInterpolation, SVGInterpolation, Interpolation.
// For now it needs to distinguish itself during the refactor and temporarily has an ugly name.
// TODO(alancutter): Make this class generic for any animation environment so it can be reused for SVG animations.
class CORE_EXPORT InvalidatableStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtr<InvalidatableStyleInterpolation> create(
        const Vector<const InterpolationType*>& InterpolationTypes,
        const CSSPropertySpecificKeyframe& startKeyframe,
        const CSSPropertySpecificKeyframe& endKeyframe)
    {
        return adoptRef(new InvalidatableStyleInterpolation(InterpolationTypes, startKeyframe, endKeyframe));
    }

    virtual void interpolate(int iteration, double fraction);
    bool dependsOnUnderlyingValue() const;
    virtual void apply(StyleResolverState&) const { ASSERT_NOT_REACHED(); }
    static void applyStack(const ActiveInterpolations&, StyleResolverState&);

    virtual bool isInvalidatableStyleInterpolation() const { return true; }

private:
    InvalidatableStyleInterpolation(
        const Vector<const InterpolationType*>& InterpolationTypes,
        const CSSPropertySpecificKeyframe& startKeyframe,
        const CSSPropertySpecificKeyframe& endKeyframe);

    PassOwnPtr<InterpolationValue> maybeConvertUnderlyingValue(const StyleResolverState&) const;
    const InterpolationValue* ensureValidInterpolation(const StyleResolverState&, const InterpolationValue* underlyingValue) const;
    bool isCacheValid(const StyleResolverState&, const InterpolationValue* underlyingValue) const;
    bool isNeutralKeyframeActive() const;
    bool maybeCachePairwiseConversion(const StyleResolverState*, const InterpolationValue* underlyingValue) const;
    PassOwnPtr<InterpolationValue> convertSingleKeyframe(const CSSPropertySpecificKeyframe&, const StyleResolverState&, const InterpolationValue* underlyingValue) const;
    void setFlagIfInheritUsed(StyleResolverState&) const;
    double underlyingFraction() const;

    const Vector<const InterpolationType*>& m_interpolationTypes;
    const CSSPropertySpecificKeyframe* m_startKeyframe;
    const CSSPropertySpecificKeyframe* m_endKeyframe;
    double m_currentFraction;
    mutable OwnPtr<PrimitiveInterpolation> m_cachedConversion;
    mutable InterpolationType::ConversionCheckers m_conversionCheckers;
    mutable OwnPtr<InterpolationValue> m_cachedValue;
};

DEFINE_TYPE_CASTS(InvalidatableStyleInterpolation, Interpolation, value, value->isInvalidatableStyleInterpolation(), value.isInvalidatableStyleInterpolation());

} // namespace blink

#endif // InvalidatableStyleInterpolation_h
