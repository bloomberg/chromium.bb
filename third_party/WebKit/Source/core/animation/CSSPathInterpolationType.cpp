// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSPathInterpolationType.h"

#include "core/animation/PathInterpolationFunctions.h"
#include "core/css/CSSPathValue.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

void CSSPathInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    ASSERT(cssProperty() == CSSPropertyD);
    environment.state().style()->setD(StylePath::create(PathInterpolationFunctions::appliedValue(interpolableValue, nonInterpolableValue)));
}

void CSSPathInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value) const
{
    PathInterpolationFunctions::composite(underlyingValueOwner, underlyingFraction, *this, value);
}

InterpolationValue CSSPathInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    return PathInterpolationFunctions::maybeConvertNeutral(*this, underlying, conversionCheckers);
}

InterpolationValue CSSPathInterpolationType::maybeConvertInitial() const
{
    return PathInterpolationFunctions::convertValue(CSSPathValue::emptyPathValue()->byteStream());
}

class ParentPathChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<ParentPathChecker> create(const InterpolationType& type, PassRefPtr<StylePath> stylePath)
    {
        return adoptPtr(new ParentPathChecker(type, stylePath));
    }

private:
    ParentPathChecker(const InterpolationType& type, PassRefPtr<StylePath> stylePath)
        : ConversionChecker(type)
        , m_stylePath(stylePath)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        return environment.state().parentStyle()->svgStyle().d() == m_stylePath.get();
    }

    const RefPtr<StylePath> m_stylePath;
};

InterpolationValue CSSPathInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    ASSERT(cssProperty() == CSSPropertyD);
    if (!state.parentStyle())
        return nullptr;

    conversionCheckers.append(ParentPathChecker::create(*this, state.parentStyle()->svgStyle().d()));
    return PathInterpolationFunctions::convertValue(state.parentStyle()->svgStyle().d()->byteStream());
}

InterpolationValue CSSPathInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    return PathInterpolationFunctions::convertValue(toCSSPathValue(value).byteStream());
}

InterpolationValue CSSPathInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    ASSERT(cssProperty() == CSSPropertyD);
    return PathInterpolationFunctions::convertValue(environment.state().style()->svgStyle().d()->byteStream());
}

PairwiseInterpolationValue CSSPathInterpolationType::mergeSingleConversions(InterpolationValue& start, InterpolationValue& end) const
{
    return PathInterpolationFunctions::mergeSingleConversions(start, end);
}

} // namespace blink
