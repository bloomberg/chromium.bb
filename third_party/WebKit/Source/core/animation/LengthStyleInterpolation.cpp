// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthStyleInterpolation.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

bool LengthStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    if (value.isPrimitiveValue()) {
        const CSSPrimitiveValue& primitiveValue = blink::toCSSPrimitiveValue(value);
        if (primitiveValue.cssCalcValue())
            return true;

        CSSPrimitiveValue::LengthUnitType type;
        // Only returns true if the type is a primitive length unit.
        return CSSPrimitiveValue::unitTypeToLengthUnitType(primitiveValue.primitiveType(), type);
    }
    return value.isCalcValue();
}

PassOwnPtrWillBeRawPtr<InterpolableValue> LengthStyleInterpolation::toInterpolableValue(const CSSValue& value)
{
    ASSERT(canCreateFrom(value));
    OwnPtrWillBeRawPtr<InterpolableList> listOfValuesAndTypes = InterpolableList::create(2);
    OwnPtrWillBeRawPtr<InterpolableList> listOfValues = InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);
    OwnPtrWillBeRawPtr<InterpolableList> listOfTypes = InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);

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

static CSSPrimitiveValue::UnitType toUnitType(int lengthUnitType)
{
    return static_cast<CSSPrimitiveValue::UnitType>(CSSPrimitiveValue::lengthUnitTypeToUnitType(static_cast<CSSPrimitiveValue::LengthUnitType>(lengthUnitType)));
}

static PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> constructCalcExpression(PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> previous, const InterpolableList* list, size_t position)
{
    const InterpolableList* listOfValues = toInterpolableList(list->get(0));
    const InterpolableList* listOfTypes = toInterpolableList(list->get(1));
    while (position != CSSPrimitiveValue::LengthUnitTypeCount) {
        const InterpolableNumber *subValueType = toInterpolableNumber(listOfTypes->get(position));
        if (subValueType->value()) {
            RefPtrWillBeRawPtr<CSSCalcExpressionNode> next;
            double value = toInterpolableNumber(listOfValues->get(position))->value();
            if (previous)
                next = CSSCalcValue::createExpressionNode(previous, CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(value, toUnitType(position))), CalcAdd);
            else
                next = CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(value, toUnitType(position)));
            return constructCalcExpression(next, list, position + 1);
        }
        position++;
    }
    return previous;
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
        return CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);
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
        return CSSPrimitiveValue::create(CSSCalcValue::create(constructCalcExpression(nullptr, listOfValuesAndTypes, 0), valueRange));
    }
}

void LengthStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, fromInterpolableValue(*m_cachedValue.get(), m_range).get());
}

void LengthStyleInterpolation::trace(Visitor* visitor)
{
    StyleInterpolation::trace(visitor);
}

}
