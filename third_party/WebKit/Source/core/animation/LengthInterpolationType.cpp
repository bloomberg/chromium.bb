// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthInterpolationType.h"

#include "core/animation/LengthPropertyFunctions.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

// This class is implemented as a singleton whose instance represents the presence of percentages being used in a Length value
// while nullptr represents the absence of any percentages.
class LengthNonInterpolableValue : public NonInterpolableValue {
public:
    ~LengthNonInterpolableValue() override { ASSERT_NOT_REACHED(); }
    static PassRefPtrWillBeRawPtr<LengthNonInterpolableValue> create(bool hasPercentage)
    {
        DEFINE_STATIC_REF_WILL_BE_PERSISTENT(LengthNonInterpolableValue, singleton, adoptRefWillBeNoop(new LengthNonInterpolableValue()));
        ASSERT(singleton);
        return hasPercentage ? singleton : nullptr;
    }
    static PassRefPtrWillBeRawPtr<LengthNonInterpolableValue> merge(const NonInterpolableValue* a, const NonInterpolableValue* b)
    {
        return create(hasPercentage(a) || hasPercentage(b));
    }
    static bool hasPercentage(const NonInterpolableValue* nonInterpolableValue)
    {
        ASSERT(!nonInterpolableValue || nonInterpolableValue->type() == LengthNonInterpolableValue::staticType);
        return static_cast<bool>(nonInterpolableValue);
    }
    DEFINE_INLINE_VIRTUAL_TRACE() { NonInterpolableValue::trace(visitor); }
    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    LengthNonInterpolableValue() { }
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(LengthNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(LengthNonInterpolableValue);

LengthInterpolationType::LengthInterpolationType(CSSPropertyID property)
    : InterpolationType(property)
    , m_valueRange(LengthPropertyFunctions::valueRange(property))
{ }

static PassOwnPtr<InterpolableList> createNeutralInterpolableValue()
{
    const size_t length = CSSPrimitiveValue::LengthUnitTypeCount;
    OwnPtr<InterpolableList> values = InterpolableList::create(length);
    for (size_t i = 0; i < length; i++)
        values->set(i, InterpolableNumber::create(0));
    return values.release();
}

float LengthInterpolationType::effectiveZoom(const ComputedStyle& style) const
{
    return LengthPropertyFunctions::isZoomedLength(m_property) ? style.effectiveZoom() : 1;
}

PassOwnPtr<InterpolationValue> LengthInterpolationType::maybeConvertLength(const Length& length, float zoom) const
{
    if (!length.isSpecified())
        return nullptr;

    PixelsAndPercent pixelsAndPercent = length.pixelsAndPercent();
    OwnPtr<InterpolableList> values = createNeutralInterpolableValue();
    values->set(CSSPrimitiveValue::UnitTypePixels, InterpolableNumber::create(pixelsAndPercent.pixels / zoom));
    values->set(CSSPrimitiveValue::UnitTypePercentage, InterpolableNumber::create(pixelsAndPercent.percent));

    return InterpolationValue::create(*this, values.release(), LengthNonInterpolableValue::create(length.hasPercent()));
}

class ParentLengthChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtr<ParentLengthChecker> create(CSSPropertyID property, const Length& length)
    {
        return adoptPtr(new ParentLengthChecker(property, length));
    }

private:
    ParentLengthChecker(CSSPropertyID property, const Length& length)
        : m_property(property)
        , m_length(length)
    { }

    bool isValid(const StyleResolverState& state) const final
    {
        Length parentLength;
        if (!LengthPropertyFunctions::getLength(m_property, *state.parentStyle(), parentLength))
            return false;
        return parentLength == m_length;
    }

    const CSSPropertyID m_property;
    const Length m_length;
};

PassOwnPtr<InterpolationValue> LengthInterpolationType::maybeConvertNeutral() const
{
    return InterpolationValue::create(*this, createNeutralInterpolableValue());
}

PassOwnPtr<InterpolationValue> LengthInterpolationType::maybeConvertInitial() const
{
    Length initialLength;
    if (!LengthPropertyFunctions::getInitialLength(m_property, initialLength))
        return nullptr;
    return maybeConvertLength(initialLength, 1);
}

PassOwnPtr<InterpolationValue> LengthInterpolationType::maybeConvertInherit(const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!state || !state->parentStyle())
        return nullptr;
    Length inheritedLength;
    if (!LengthPropertyFunctions::getLength(m_property, *state->parentStyle(), inheritedLength))
        return nullptr;
    conversionCheckers.append(ParentLengthChecker::create(m_property, inheritedLength));
    return maybeConvertLength(inheritedLength, effectiveZoom(*state->parentStyle()));
}

PassOwnPtr<InterpolationValue> LengthInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!value.isPrimitiveValue())
        return nullptr;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    CSSLengthArray valueArray;
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++)
        valueArray.append(0);
    bool hasPercentage = false;

    if (primitiveValue.isValueID()) {
        CSSValueID valueID = primitiveValue.getValueID();
        double pixels;
        if (!LengthPropertyFunctions::getPixelsForKeyword(m_property, valueID, pixels))
            return nullptr;
        valueArray[CSSPrimitiveValue::UnitTypePixels] = pixels;
    } else {
        if (!primitiveValue.isLength() && !primitiveValue.isPercentage() && !primitiveValue.isCalculatedPercentageWithLength())
            return nullptr;
        CSSLengthTypeArray hasType;
        hasType.ensureSize(CSSPrimitiveValue::LengthUnitTypeCount);
        primitiveValue.accumulateLengthArray(valueArray, hasType);
        hasPercentage = hasType.get(CSSPrimitiveValue::UnitTypePercentage);
    }

    OwnPtr<InterpolableList> values = InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++)
        values->set(i, InterpolableNumber::create(valueArray.at(i)));

    return InterpolationValue::create(*this, values.release(), LengthNonInterpolableValue::create(hasPercentage));
}

PassOwnPtr<InterpolationValue> LengthInterpolationType::maybeConvertUnderlyingValue(const StyleResolverState& state) const
{
    Length underlyingLength;
    if (!LengthPropertyFunctions::getLength(m_property, *state.style(), underlyingLength))
        return nullptr;
    return maybeConvertLength(underlyingLength, effectiveZoom(*state.style()));
}

PassOwnPtr<PairwisePrimitiveInterpolation> LengthInterpolationType::mergeSingleConversions(InterpolationValue& startValue, InterpolationValue& endValue) const
{
    return PairwisePrimitiveInterpolation::create(*this,
        startValue.mutableComponent().interpolableValue.release(),
        endValue.mutableComponent().interpolableValue.release(),
        LengthNonInterpolableValue::merge(startValue.nonInterpolableValue(), endValue.nonInterpolableValue()));
}

void LengthInterpolationType::composite(UnderlyingValue& underlyingValue, double underlyingFraction, const InterpolationValue& value) const
{
    InterpolationComponentValue& underlyingComponent = underlyingValue.mutableComponent();
    underlyingComponent.interpolableValue->scaleAndAdd(underlyingFraction, value.interpolableValue());
    underlyingComponent.nonInterpolableValue = LengthNonInterpolableValue::merge(underlyingValue->nonInterpolableValue(), value.nonInterpolableValue());
}

static bool isPixelsOrPercentOnly(const InterpolableList& values)
{
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        if (i == CSSPrimitiveValue::UnitTypePixels || i == CSSPrimitiveValue::UnitTypePercentage)
            continue;
        if (toInterpolableNumber(values.get(i))->value())
            return false;
    }
    return true;
}

// TODO(alancutter): Move this to Length.h.
static double clampToRange(double x, ValueRange range)
{
    return (range == ValueRangeNonNegative && x < 0) ? 0 : x;
}

static Length createLength(const InterpolableList& values, bool hasPercentage, ValueRange range, double zoom)
{
    ASSERT(isPixelsOrPercentOnly(values));
    double pixels = toInterpolableNumber(values.get(CSSPrimitiveValue::UnitTypePixels))->value() * zoom;
    double percentage = toInterpolableNumber(values.get(CSSPrimitiveValue::UnitTypePercentage))->value();
    ASSERT(hasPercentage || percentage == 0);

    if (pixels && hasPercentage)
        return Length(CalculationValue::create(PixelsAndPercent(pixels, percentage), range));
    if (hasPercentage)
        return Length(clampToRange(percentage, range), Percent);
    return Length(CSSPrimitiveValue::clampToCSSLengthRange(clampToRange(pixels, range)), Fixed);
}

static CSSPrimitiveValue::UnitType toUnitType(int lengthUnitType)
{
    return static_cast<CSSPrimitiveValue::UnitType>(CSSPrimitiveValue::lengthUnitTypeToUnitType(static_cast<CSSPrimitiveValue::LengthUnitType>(lengthUnitType)));
}

static PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> createCalcExpression(const InterpolableList& values, bool hasPercentage)
{
    RefPtrWillBeRawPtr<CSSCalcExpressionNode> result = nullptr;
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        double value = toInterpolableNumber(values.get(i))->value();
        if (value || (i == CSSPrimitiveValue::UnitTypePercentage && hasPercentage)) {
            RefPtrWillBeRawPtr<CSSCalcExpressionNode> node = CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(value, toUnitType(i)));
            result = result ? CSSCalcValue::createExpressionNode(result.release(), node.release(), CalcAdd) : node.release();
        }
    }
    ASSERT(result);
    return result.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> createCSSValue(const InterpolableList& values, bool hasPercentage, ValueRange range)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> result;
    size_t firstUnitIndex = CSSPrimitiveValue::LengthUnitTypeCount;
    size_t unitTypeCount = 0;
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        if ((hasPercentage && i == CSSPrimitiveValue::UnitTypePercentage) || toInterpolableNumber(values.get(i))->value()) {
            unitTypeCount++;
            if (unitTypeCount == 1)
                firstUnitIndex = i;
        }
    }
    switch (unitTypeCount) {
    case 0:
        return CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    case 1: {
        double value = clampToRange(toInterpolableNumber(values.get(firstUnitIndex))->value(), range);
        return CSSPrimitiveValue::create(value, toUnitType(firstUnitIndex));
    }
    default:
        return CSSPrimitiveValue::create(CSSCalcValue::create(createCalcExpression(values, hasPercentage), range));
    }
}

void LengthInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, StyleResolverState& state) const
{
    const InterpolableList& values = toInterpolableList(interpolableValue);
    bool hasPercentage = LengthNonInterpolableValue::hasPercentage(nonInterpolableValue);
    if (isPixelsOrPercentOnly(values)) {
        Length length = createLength(values, hasPercentage, m_valueRange, effectiveZoom(*state.style()));
        if (LengthPropertyFunctions::setLength(m_property, *state.style(), length)) {
#if ENABLE(ASSERT)
            // Assert that setting the length on ComputedStyle directly is identical to the AnimatableValue code path.
            RefPtr<AnimatableValue> before = CSSAnimatableValueFactory::create(m_property, *state.style());
            StyleBuilder::applyProperty(m_property, state, createCSSValue(values, hasPercentage, m_valueRange).get());
            RefPtr<AnimatableValue> after = CSSAnimatableValueFactory::create(m_property, *state.style());
            ASSERT(before->equals(*after));
#endif
            return;
        }
    }
    StyleBuilder::applyProperty(m_property, state, createCSSValue(values, hasPercentage, m_valueRange).get());
}

} // namespace blink
