// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthInterpolationType.h"

#include "core/animation/LengthPropertyFunctions.h"
#include "core/animation/LengthStyleInterpolation.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

LengthInterpolationType::LengthInterpolationType(CSSPropertyID property)
    : InterpolationType(property)
    , m_valueRange(LengthPropertyFunctions::valueRange(property))
{ }

static PassOwnPtrWillBeRawPtr<InterpolableList> createNeutralValue()
{
    OwnPtrWillBeRawPtr<InterpolableList> listOfValuesAndTypes = InterpolableList::create(2);
    const size_t length = CSSPrimitiveValue::LengthUnitTypeCount;
    OwnPtrWillBeRawPtr<InterpolableList> listOfValues = InterpolableList::create(length);
    // TODO(alancutter): Use a NonInterpolableValue to represent the list of types.
    OwnPtrWillBeRawPtr<InterpolableList> listOfTypes = InterpolableList::create(length);
    for (size_t i = 0; i < length; i++) {
        listOfValues->set(i, InterpolableNumber::create(0));
        listOfTypes->set(i, InterpolableNumber::create(0));
    }
    listOfValuesAndTypes->set(0, listOfValues.release());
    listOfValuesAndTypes->set(1, listOfTypes.release());
    return listOfValuesAndTypes.release();
}

PassOwnPtrWillBeRawPtr<InterpolationValue> LengthInterpolationType::maybeConvertLength(const Length& length, float zoom) const
{
    if (!length.isSpecified())
        return nullptr;

    PixelsAndPercent pixelsAndPercent = length.pixelsAndPercent();
    OwnPtrWillBeRawPtr<InterpolableList> valuesAndTypes = createNeutralValue();

    InterpolableList& values = toInterpolableList(*valuesAndTypes->get(0));
    values.set(CSSPrimitiveValue::UnitTypePixels, InterpolableNumber::create(pixelsAndPercent.pixels / zoom));
    values.set(CSSPrimitiveValue::UnitTypePercentage, InterpolableNumber::create(pixelsAndPercent.percent));

    InterpolableList& types = toInterpolableList(*valuesAndTypes->get(1));
    types.set(CSSPrimitiveValue::UnitTypePixels, InterpolableNumber::create(1));
    types.set(CSSPrimitiveValue::UnitTypePercentage, InterpolableNumber::create(1));

    return InterpolationValue::create(*this, valuesAndTypes.release());
}

class ParentLengthChecker : public InterpolationType::ConversionChecker {
public:
    static PassOwnPtrWillBeRawPtr<ParentLengthChecker> create(CSSPropertyID property, const Length& length)
    {
        return adoptPtrWillBeNoop(new ParentLengthChecker(property, length));
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
            return true;
        return parentLength == m_length;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ConversionChecker::trace(visitor);
    }

    const CSSPropertyID m_property;
    const Length m_length;
};

PassOwnPtrWillBeRawPtr<InterpolationValue> LengthInterpolationType::maybeConvertNeutral() const
{
    return InterpolationValue::create(*this, createNeutralValue());
}

PassOwnPtrWillBeRawPtr<InterpolationValue> LengthInterpolationType::maybeConvertInitial() const
{
    Length initialLength;
    if (!LengthPropertyFunctions::getInitialLength(m_property, initialLength))
        return nullptr;
    return maybeConvertLength(initialLength, 1);
}

PassOwnPtrWillBeRawPtr<InterpolationValue> LengthInterpolationType::maybeConvertInherit(const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!state || !state->parentStyle())
        return nullptr;
    Length inheritedLength;
    if (!LengthPropertyFunctions::getLength(m_property, *state->parentStyle(), inheritedLength))
        return nullptr;
    conversionCheckers.append(ParentLengthChecker::create(m_property, inheritedLength));
    return maybeConvertLength(inheritedLength, state->parentStyle()->effectiveZoom());
}

PassOwnPtrWillBeRawPtr<InterpolationValue> LengthInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState* state, ConversionCheckers& conversionCheckers) const
{
    if (!value.isPrimitiveValue())
        return nullptr;

    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);

    OwnPtrWillBeRawPtr<InterpolableList> listOfValuesAndTypes = InterpolableList::create(2);
    OwnPtrWillBeRawPtr<InterpolableList> listOfValues = InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);
    OwnPtrWillBeRawPtr<InterpolableList> listOfTypes = InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);

    CSSLengthArray arrayOfValues;
    CSSLengthTypeArray arrayOfTypes;
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++)
        arrayOfValues.append(0);
    arrayOfTypes.ensureSize(CSSPrimitiveValue::LengthUnitTypeCount);

    if (primitiveValue.isValueID()) {
        CSSValueID valueID = primitiveValue.getValueID();
        double pixels;
        if (!LengthPropertyFunctions::getPixelsForKeyword(m_property, valueID, pixels))
            return nullptr;
        arrayOfTypes.set(CSSPrimitiveValue::UnitTypePixels);
        arrayOfValues[CSSPrimitiveValue::UnitTypePixels] = pixels;
    } else {
        if (!primitiveValue.isLength() && !primitiveValue.isPercentage() && !primitiveValue.isCalculatedPercentageWithLength())
            return nullptr;
        primitiveValue.accumulateLengthArray(arrayOfValues, arrayOfTypes);
    }

    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
        listOfValues->set(i, InterpolableNumber::create(arrayOfValues.at(i)));
        listOfTypes->set(i, InterpolableNumber::create(arrayOfTypes.get(i)));
    }

    listOfValuesAndTypes->set(0, listOfValues.release());
    listOfValuesAndTypes->set(1, listOfTypes.release());

    return InterpolationValue::create(*this, listOfValuesAndTypes.release());
}

PassOwnPtrWillBeRawPtr<InterpolationValue> LengthInterpolationType::maybeConvertUnderlyingValue(const StyleResolverState& state) const
{
    Length underlyingLength;
    if (!LengthPropertyFunctions::getLength(m_property, *state.style(), underlyingLength))
        return nullptr;
    return maybeConvertLength(underlyingLength, state.style()->effectiveZoom());
}

void LengthInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue*, StyleResolverState& state) const
{
    // TODO(alancutter): Make all length interpolation functions operate on ValueRanges instead of InterpolationRanges.
    InterpolationRange range = m_valueRange == ValueRangeNonNegative ? RangeNonNegative : RangeAll;
    // TODO(alancutter): Set arbitrary property Lengths on ComputedStyle without using cross compilation unit member function getters (Windows runtime doesn't like it).
    ASSERT(m_property == CSSPropertyLeft);
    LengthStyleInterpolation::applyInterpolableValue(m_property, interpolableValue, range, state, &ComputedStyle::setLeft);
}

} // namespace blink
