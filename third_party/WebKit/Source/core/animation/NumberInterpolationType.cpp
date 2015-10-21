// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/NumberInterpolationType.h"

#include "core/animation/NumberPropertyFunctions.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

class ParentNumberChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<ParentNumberChecker> create(const InterpolationType& type, CSSPropertyID property, double number)
    {
        return adoptPtr(new ParentNumberChecker(type, property, number));
    }

private:
    ParentNumberChecker(const InterpolationType& type, CSSPropertyID property, double number)
        : ConversionChecker(type)
        , m_property(property)
        , m_number(number)
    { }

    bool isValid(const StyleResolverState& state, const UnderlyingValue&) const final
    {
        double parentNumber;
        if (!NumberPropertyFunctions::getNumber(m_property, *state.parentStyle(), parentNumber))
            return false;
        return parentNumber == m_number;
    }

    DEFINE_INLINE_VIRTUAL_TRACE() { ConversionChecker::trace(visitor); }

    const CSSPropertyID m_property;
    const double m_number;
};

PassOwnPtr<InterpolationValue> NumberInterpolationType::createNumberValue(double number) const
{
    return InterpolationValue::create(*this, InterpolableNumber::create(number));
}

PassOwnPtr<InterpolationValue> NumberInterpolationType::maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const
{
    return createNumberValue(0);
}

PassOwnPtr<InterpolationValue> NumberInterpolationType::maybeConvertInitial() const
{
    double initialNumber;
    if (!NumberPropertyFunctions::getInitialNumber(m_property, initialNumber))
        return nullptr;
    return createNumberValue(initialNumber);
}

PassOwnPtr<InterpolationValue> NumberInterpolationType::maybeConvertInherit(const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!state || !state->parentStyle())
        return nullptr;
    double inheritedNumber;
    if (!NumberPropertyFunctions::getNumber(m_property, *state->parentStyle(), inheritedNumber))
        return nullptr;
    conversionCheckers.append(ParentNumberChecker::create(*this, m_property, inheritedNumber));
    return createNumberValue(inheritedNumber);
}

PassOwnPtr<InterpolationValue> NumberInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState*, ConversionCheckers&) const
{
    if (!value.isPrimitiveValue() || !toCSSPrimitiveValue(value).isNumber())
        return nullptr;
    return createNumberValue(toCSSPrimitiveValue(value).getDoubleValue());
}

PassOwnPtr<InterpolationValue> NumberInterpolationType::maybeConvertUnderlyingValue(const StyleResolverState& state) const
{
    double underlyingNumber;
    if (!NumberPropertyFunctions::getNumber(m_property, *state.style(), underlyingNumber))
        return nullptr;
    return createNumberValue(underlyingNumber);
}

void NumberInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue*, StyleResolverState& state) const
{
    double clampedNumber = NumberPropertyFunctions::clampNumber(m_property, toInterpolableNumber(interpolableValue).value());
    if (!NumberPropertyFunctions::setNumber(m_property, *state.style(), clampedNumber))
        StyleBuilder::applyProperty(m_property, state, CSSPrimitiveValue::create(clampedNumber, CSSPrimitiveValue::UnitType::Number).get());
}

} // namespace blink
