// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSLengthInterpolationType.h"

#include "core/animation/LengthPropertyFunctions.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// This class is implemented as a singleton whose instance represents the presence of percentages being used in a Length value
// while nullptr represents the absence of any percentages.
class CSSLengthNonInterpolableValue : public NonInterpolableValue {
public:
    ~CSSLengthNonInterpolableValue() final { NOTREACHED(); }
    static PassRefPtr<CSSLengthNonInterpolableValue> create(bool hasPercentage)
    {
        DEFINE_STATIC_REF(CSSLengthNonInterpolableValue, singleton, adoptRef(new CSSLengthNonInterpolableValue()));
        DCHECK(singleton);
        return hasPercentage ? singleton : nullptr;
    }
    static PassRefPtr<CSSLengthNonInterpolableValue> merge(const NonInterpolableValue* a, const NonInterpolableValue* b)
    {
        return create(hasPercentage(a) || hasPercentage(b));
    }
    static bool hasPercentage(const NonInterpolableValue*);

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSLengthNonInterpolableValue() { }
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSLengthNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSLengthNonInterpolableValue);

bool CSSLengthNonInterpolableValue::hasPercentage(const NonInterpolableValue* nonInterpolableValue)
{
    DCHECK(isCSSLengthNonInterpolableValue(nonInterpolableValue));
    return static_cast<bool>(nonInterpolableValue);
}

CSSLengthInterpolationType::CSSLengthInterpolationType(CSSPropertyID property)
    : CSSInterpolationType(property)
    , m_valueRange(LengthPropertyFunctions::getValueRange(property))
{ }

float CSSLengthInterpolationType::effectiveZoom(const ComputedStyle& style) const
{
    return LengthPropertyFunctions::isZoomedLength(cssProperty()) ? style.effectiveZoom() : 1;
}

std::unique_ptr<InterpolableValue> CSSLengthInterpolationType::createInterpolablePixels(double pixels)
{
    std::unique_ptr<InterpolableList> interpolableList = createNeutralInterpolableValue();
    interpolableList->set(CSSPrimitiveValue::UnitTypePixels, InterpolableNumber::create(pixels));
    return std::move(interpolableList);
}

InterpolationValue CSSLengthInterpolationType::createInterpolablePercent(double percent)
{
    std::unique_ptr<InterpolableList> interpolableList = createNeutralInterpolableValue();
    interpolableList->set(CSSPrimitiveValue::UnitTypePercentage, InterpolableNumber::create(percent));
    return InterpolationValue(std::move(interpolableList), CSSLengthNonInterpolableValue::create(true));
}

InterpolationValue CSSLengthInterpolationType::maybeConvertLength(const Length& length, float zoom)
{
    if (!length.isSpecified())
        return nullptr;

    PixelsAndPercent pixelsAndPercent = length.getPixelsAndPercent();
    std::unique_ptr<InterpolableList> values = createNeutralInterpolableValue();
    values->set(CSSPrimitiveValue::UnitTypePixels, InterpolableNumber::create(pixelsAndPercent.pixels / zoom));
    values->set(CSSPrimitiveValue::UnitTypePercentage, InterpolableNumber::create(pixelsAndPercent.percent));

    return InterpolationValue(std::move(values), CSSLengthNonInterpolableValue::create(length.isPercentOrCalc()));
}

std::unique_ptr<InterpolableList> CSSLengthInterpolationType::createNeutralInterpolableValue()
{
    const size_t length = CSSPrimitiveValue::LengthUnitTypeCount;
    std::unique_ptr<InterpolableList> values = InterpolableList::create(length);
    for (size_t i = 0; i < length; i++)
        values->set(i, InterpolableNumber::create(0));
    return values;
}

PairwiseInterpolationValue CSSLengthInterpolationType::staticMergeSingleConversions(InterpolationValue&& start, InterpolationValue&& end)
{
    return PairwiseInterpolationValue(
        std::move(start.interpolableValue),
        std::move(end.interpolableValue),
        CSSLengthNonInterpolableValue::merge(start.nonInterpolableValue.get(), end.nonInterpolableValue.get()));
}

bool CSSLengthInterpolationType::nonInterpolableValuesAreCompatible(const NonInterpolableValue* a, const NonInterpolableValue* b)
{
    DCHECK(isCSSLengthNonInterpolableValue(a));
    DCHECK(isCSSLengthNonInterpolableValue(b));
    return true;
}

void CSSLengthInterpolationType::composite(
    std::unique_ptr<InterpolableValue>& underlyingInterpolableValue,
    RefPtr<NonInterpolableValue>& underlyingNonInterpolableValue,
    double underlyingFraction,
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue)
{
    underlyingInterpolableValue->scaleAndAdd(underlyingFraction, interpolableValue);
    underlyingNonInterpolableValue = CSSLengthNonInterpolableValue::merge(underlyingNonInterpolableValue.get(), nonInterpolableValue);
}

void CSSLengthInterpolationType::subtractFromOneHundredPercent(InterpolationValue& result)
{
    InterpolableList& list = toInterpolableList(*result.interpolableValue);
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        double value = -toInterpolableNumber(*list.get(i)).value();
        if (i == CSSPrimitiveValue::UnitTypePercentage)
            value += 100;
        toInterpolableNumber(*list.getMutable(i)).set(value);
    }
    result.nonInterpolableValue = CSSLengthNonInterpolableValue::create(true);
}

InterpolationValue CSSLengthInterpolationType::maybeConvertCSSValue(const CSSValue& value)
{
    if (!value.isPrimitiveValue())
        return nullptr;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    if (!primitiveValue.isLength() && !primitiveValue.isPercentage() && !primitiveValue.isCalculatedPercentageWithLength())
        return nullptr;

    CSSLengthArray lengthArray;
    primitiveValue.accumulateLengthArray(lengthArray);

    std::unique_ptr<InterpolableList> values = InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++)
        values->set(i, InterpolableNumber::create(lengthArray.values[i]));

    bool hasPercentage = lengthArray.typeFlags.get(CSSPrimitiveValue::UnitTypePercentage);
    return InterpolationValue(std::move(values), CSSLengthNonInterpolableValue::create(hasPercentage));
}

class ParentLengthChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<ParentLengthChecker> create(CSSPropertyID property, const Length& length)
    {
        return wrapUnique(new ParentLengthChecker(property, length));
    }

private:
    ParentLengthChecker(CSSPropertyID property, const Length& length)
        : m_property(property)
        , m_length(length)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        Length parentLength;
        if (!LengthPropertyFunctions::getLength(m_property, *environment.state().parentStyle(), parentLength))
            return false;
        return parentLength == m_length;
    }

    const CSSPropertyID m_property;
    const Length m_length;
};

InterpolationValue CSSLengthInterpolationType::maybeConvertNeutral(const InterpolationValue&, ConversionCheckers&) const
{
    return InterpolationValue(createNeutralInterpolableValue());
}

InterpolationValue CSSLengthInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers& conversionCheckers) const
{
    Length initialLength;
    if (!LengthPropertyFunctions::getInitialLength(cssProperty(), initialLength))
        return nullptr;
    return maybeConvertLength(initialLength, 1);
}

InterpolationValue CSSLengthInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;
    Length inheritedLength;
    if (!LengthPropertyFunctions::getLength(cssProperty(), *state.parentStyle(), inheritedLength))
        return nullptr;
    conversionCheckers.append(ParentLengthChecker::create(cssProperty(), inheritedLength));
    return maybeConvertLength(inheritedLength, effectiveZoom(*state.parentStyle()));
}

InterpolationValue CSSLengthInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers& conversionCheckers) const
{
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).isValueID()) {
        CSSValueID valueID = toCSSPrimitiveValue(value).getValueID();
        double pixels;
        if (!LengthPropertyFunctions::getPixelsForKeyword(cssProperty(), valueID, pixels))
            return nullptr;
        return InterpolationValue(createInterpolablePixels(pixels));
    }

    return maybeConvertCSSValue(value);
}

InterpolationValue CSSLengthInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    Length underlyingLength;
    if (!LengthPropertyFunctions::getLength(cssProperty(), *environment.state().style(), underlyingLength))
        return nullptr;
    return maybeConvertLength(underlyingLength, effectiveZoom(*environment.state().style()));
}

void CSSLengthInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    InterpolationValue& underlying = underlyingValueOwner.mutableValue();
    composite(underlying.interpolableValue, underlying.nonInterpolableValue,
        underlyingFraction, *value.interpolableValue, value.nonInterpolableValue.get());
}

// TODO(alancutter): Move this to Length.h.
static double clampToRange(double x, ValueRange range)
{
    return (range == ValueRangeNonNegative && x < 0) ? 0 : x;
}

Length CSSLengthInterpolationType::createLength(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, const CSSToLengthConversionData& conversionData, ValueRange range)
{
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    bool hasPercentage = CSSLengthNonInterpolableValue::hasPercentage(nonInterpolableValue);
    double pixels = 0;
    double percentage = 0;
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        double value = toInterpolableNumber(*interpolableList.get(i)).value();
        if (i == CSSPrimitiveValue::UnitTypePercentage) {
            percentage = value;
        } else {
            CSSPrimitiveValue::UnitType type = CSSPrimitiveValue::lengthUnitTypeToUnitType(static_cast<CSSPrimitiveValue::LengthUnitType>(i));
            pixels += conversionData.zoomedComputedPixels(value, type);
        }
    }
    if (percentage != 0)
        hasPercentage = true;
    if (pixels != 0 && hasPercentage)
        return Length(CalculationValue::create(PixelsAndPercent(pixels, percentage), range));
    if (hasPercentage)
        return Length(clampToRange(percentage, range), Percent);
    return Length(CSSPrimitiveValue::clampToCSSLengthRange(clampToRange(pixels, range)), Fixed);
}

void CSSLengthInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    StyleResolverState& state = environment.state();
    ComputedStyle& style = *state.style();
    float zoom = effectiveZoom(style);
    Length length = createLength(interpolableValue, nonInterpolableValue, state.cssToLengthConversionData(), m_valueRange);
    if (LengthPropertyFunctions::setLength(cssProperty(), style, length)) {
#if DCHECK_IS_ON()
        // Assert that setting the length on ComputedStyle directly is identical to the StyleBuilder code path.
        // This check is useful for catching differences in clamping behaviour.
        Length before;
        Length after;
        DCHECK(LengthPropertyFunctions::getLength(cssProperty(), style, before));
        StyleBuilder::applyProperty(cssProperty(), state, *CSSPrimitiveValue::create(length, zoom));
        DCHECK(LengthPropertyFunctions::getLength(cssProperty(), style, after));
        DCHECK(before == after);
#endif
        return;
    }
    StyleBuilder::applyProperty(cssProperty(), state, *CSSPrimitiveValue::create(length, zoom));
}

} // namespace blink
