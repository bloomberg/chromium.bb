// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthStyleInterpolation.h"

#include "core/animation/LengthPropertyFunctions.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/CalculationValue.h"

namespace blink {

bool LengthStyleInterpolation::canCreateFrom(const CSSValue& value, CSSPropertyID property)
{
    if (!value.isPrimitiveValue())
        return false;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    return primitiveValue.isLength() || primitiveValue.isPercentage() || primitiveValue.isCalculatedPercentageWithLength();
}

PassOwnPtr<InterpolableValue> LengthStyleInterpolation::toInterpolableValue(const CSSValue& value, CSSPropertyID id)
{
    ASSERT(canCreateFrom(value, id));
    OwnPtr<InterpolableList> listOfValuesAndTypes = InterpolableList::create(2);
    OwnPtr<InterpolableList> listOfValues = InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);
    OwnPtr<InterpolableList> listOfTypes = InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);

    const CSSPrimitiveValue& primitive = toCSSPrimitiveValue(value);

    CSSLengthArray arrayOfValues;
    CSSPrimitiveValue::CSSLengthTypeArray arrayOfTypes;
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++)
        arrayOfValues.append(0);
    arrayOfTypes.ensureSize(CSSPrimitiveValue::LengthUnitTypeCount);
    primitive.accumulateLengthArray(arrayOfValues, arrayOfTypes);

    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        listOfValues->set(i, InterpolableNumber::create(arrayOfValues.at(i)));
        listOfTypes->set(i, InterpolableNumber::create(arrayOfTypes.get(i)));
    }

    listOfValuesAndTypes->set(0, listOfValues.release());
    listOfValuesAndTypes->set(1, listOfTypes.release());

    return listOfValuesAndTypes.release();
}

namespace {

bool isPixelsOrPercentOnly(const InterpolableValue& value)
{
    const InterpolableList& types = *toInterpolableList(toInterpolableList(value).get(1));
    bool result = false;
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        bool typeIsPresent = toInterpolableNumber(types.get(i))->value();
        if (i == CSSPrimitiveValue::UnitTypePixels)
            result |= typeIsPresent;
        else if (i == CSSPrimitiveValue::UnitTypePercentage)
            result |= typeIsPresent;
        else if (typeIsPresent)
            return false;
    }
    return result;
}

static CSSPrimitiveValue::UnitType toUnitType(int lengthUnitType)
{
    return static_cast<CSSPrimitiveValue::UnitType>(CSSPrimitiveValue::lengthUnitTypeToUnitType(static_cast<CSSPrimitiveValue::LengthUnitType>(lengthUnitType)));
}

static PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> constructCalcExpression(const InterpolableList* list)
{
    const InterpolableList* listOfValues = toInterpolableList(list->get(0));
    const InterpolableList* listOfTypes = toInterpolableList(list->get(1));
    RefPtrWillBeRawPtr<CSSCalcExpressionNode> expression = nullptr;
    for (size_t position = 0; position < CSSPrimitiveValue::LengthUnitTypeCount; position++) {
        const InterpolableNumber *subValueType = toInterpolableNumber(listOfTypes->get(position));
        if (!subValueType->value())
            continue;
        double value = toInterpolableNumber(listOfValues->get(position))->value();
        RefPtrWillBeRawPtr<CSSCalcExpressionNode> currentTerm = CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(value, toUnitType(position)));
        if (expression)
            expression = CSSCalcValue::createExpressionNode(expression.release(), currentTerm.release(), CalcAdd);
        else
            expression = currentTerm.release();
    }
    return expression.release();
}

static double clampToRange(double x, ValueRange range)
{
    return (range == ValueRangeNonNegative && x < 0) ? 0 : x;
}

static Length lengthFromInterpolableValue(const InterpolableValue& value, InterpolationRange interpolationRange, float zoom)
{
    const InterpolableList& values = *toInterpolableList(toInterpolableList(value).get(0));
    const InterpolableList& types = *toInterpolableList(toInterpolableList(value).get(1));
    bool hasPixels = toInterpolableNumber(types.get(CSSPrimitiveValue::UnitTypePixels))->value();
    bool hasPercent = toInterpolableNumber(types.get(CSSPrimitiveValue::UnitTypePercentage))->value();

    ValueRange range = (interpolationRange == RangeNonNegative) ? ValueRangeNonNegative : ValueRangeAll;
    PixelsAndPercent pixelsAndPercent(0, 0);
    if (hasPixels)
        pixelsAndPercent.pixels = toInterpolableNumber(values.get(CSSPrimitiveValue::UnitTypePixels))->value() * zoom;
    if (hasPercent)
        pixelsAndPercent.percent = toInterpolableNumber(values.get(CSSPrimitiveValue::UnitTypePercentage))->value();

    if (hasPixels && hasPercent)
        return Length(CalculationValue::create(pixelsAndPercent, range));
    if (hasPixels)
        return Length(CSSPrimitiveValue::clampToCSSLengthRange(clampToRange(pixelsAndPercent.pixels, range)), Fixed);
    if (hasPercent)
        return Length(clampToRange(pixelsAndPercent.percent, range), Percent);
    ASSERT_NOT_REACHED();
    return Length(0, Fixed);
}

}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> LengthStyleInterpolation::fromInterpolableValue(const InterpolableValue& value, InterpolationRange range)
{
    const InterpolableList* listOfValuesAndTypes = toInterpolableList(&value);
    const InterpolableList* listOfValues = toInterpolableList(listOfValuesAndTypes->get(0));
    const InterpolableList* listOfTypes = toInterpolableList(listOfValuesAndTypes->get(1));
    unsigned unitTypeCount = 0;
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        const InterpolableNumber* subType = toInterpolableNumber(listOfTypes->get(i));
        if (subType->value()) {
            unitTypeCount++;
        }
    }

    switch (unitTypeCount) {
    case 0:
        // TODO: this case should never be reached. This issue should be fixed once we have multiple interpolators.
        return CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    case 1:
        for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
            const InterpolableNumber *subValueType = toInterpolableNumber(listOfTypes->get(i));
            if (subValueType->value()) {
                double value = toInterpolableNumber(listOfValues->get(i))->value();
                if (range == RangeNonNegative && value < 0)
                    value = 0;
                return CSSPrimitiveValue::create(value, toUnitType(i));
            }
        }
        ASSERT_NOT_REACHED();
    default:
        ValueRange valueRange = (range == RangeNonNegative) ? ValueRangeNonNegative : ValueRangeAll;
        return CSSPrimitiveValue::create(CSSCalcValue::create(constructCalcExpression(listOfValuesAndTypes), valueRange));
    }
}

void LengthStyleInterpolation::applyInterpolableValue(CSSPropertyID property, const InterpolableValue& value, InterpolationRange range, StyleResolverState& state)
{
    if (isPixelsOrPercentOnly(value)) {
        Length length = lengthFromInterpolableValue(value, range, state.style()->effectiveZoom());
        if (LengthPropertyFunctions::setLength(property, *state.style(), length)) {
#if ENABLE(ASSERT)
            // Assert that setting the length on ComputedStyle directly is identical to the AnimatableValue code path.
            RefPtr<AnimatableValue> before = CSSAnimatableValueFactory::create(property, *state.style());
            StyleBuilder::applyProperty(property, state, fromInterpolableValue(value, range).get());
            RefPtr<AnimatableValue> after = CSSAnimatableValueFactory::create(property, *state.style());
            ASSERT(before->equals(*after));
#endif
            return;
        }
    }
    StyleBuilder::applyProperty(property, state, fromInterpolableValue(value, range).get());
}

void LengthStyleInterpolation::apply(StyleResolverState& state) const
{
    applyInterpolableValue(m_id, *m_cachedValue, m_range, state);
}

}
