// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/LengthSVGInterpolation.h"

#include "core/css/CSSHelper.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedLengthList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGLengthContext.h"
#include "wtf/StdLibExtras.h"

namespace blink {

PassRefPtrWillBeRawPtr<SVGLengthList> LengthSVGInterpolation::createList(const SVGAnimatedPropertyBase& attribute)
{
    ASSERT(attribute.type() == AnimatedLengthList);
    const SVGAnimatedLengthList& animatedLengthList = static_cast<const SVGAnimatedLengthList&>(attribute);
    return SVGLengthList::create(animatedLengthList.currentValue()->unitMode());
}

PassRefPtr<LengthSVGInterpolation> LengthSVGInterpolation::create(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
{
    NonInterpolableType modeData;
    OwnPtr<InterpolableValue> startValue = toInterpolableValue(toSVGLength(start), attribute.get(), &modeData);
    OwnPtr<InterpolableValue> endValue = toInterpolableValue(toSVGLength(end), attribute.get(), nullptr);
    return adoptRef(new LengthSVGInterpolation(startValue.release(), endValue.release(), attribute, modeData));
}

namespace {

void populateModeData(const SVGAnimatedPropertyBase* attribute, LengthSVGInterpolation::NonInterpolableType* ptrModeData)
{
    switch (attribute->type()) {
    case AnimatedLength: {
        const SVGAnimatedLength& animatedLength = static_cast<const SVGAnimatedLength&>(*attribute);
        ptrModeData->unitMode = animatedLength.currentValue()->unitMode();
        ptrModeData->negativeValuesMode = animatedLength.negativeValuesMode();
        break;
    }
    case AnimatedLengthList: {
        const SVGAnimatedLengthList& animatedLengthList = static_cast<const SVGAnimatedLengthList&>(*attribute);
        ptrModeData->unitMode = animatedLengthList.currentValue()->unitMode();
        ptrModeData->negativeValuesMode = AllowNegativeLengths;
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

enum LengthInterpolatedUnit {
    LengthInterpolatedNumber,
    LengthInterpolatedPercentage,
    LengthInterpolatedEMS,
    LengthInterpolatedEXS,
    LengthInterpolatedREMS,
    LengthInterpolatedCHS,
};

static const CSSPrimitiveValue::UnitType unitTypes[] = {
    CSSPrimitiveValue::UnitType::Number,
    CSSPrimitiveValue::UnitType::Percentage,
    CSSPrimitiveValue::UnitType::Ems,
    CSSPrimitiveValue::UnitType::Exs,
    CSSPrimitiveValue::UnitType::Rems,
    CSSPrimitiveValue::UnitType::Chs
};

const size_t numLengthInterpolatedUnits = WTF_ARRAY_LENGTH(unitTypes);

LengthInterpolatedUnit convertToInterpolatedUnit(CSSPrimitiveValue::UnitType unitType, double& value)
{
    switch (unitType) {
    case CSSPrimitiveValue::UnitType::Unknown:
    default:
        ASSERT_NOT_REACHED();
    case CSSPrimitiveValue::UnitType::Pixels:
    case CSSPrimitiveValue::UnitType::Number:
        return LengthInterpolatedNumber;
    case CSSPrimitiveValue::UnitType::Percentage:
        return LengthInterpolatedPercentage;
    case CSSPrimitiveValue::UnitType::Ems:
        return LengthInterpolatedEMS;
    case CSSPrimitiveValue::UnitType::Exs:
        return LengthInterpolatedEXS;
    case CSSPrimitiveValue::UnitType::Centimeters:
        value *= cssPixelsPerCentimeter;
        return LengthInterpolatedNumber;
    case CSSPrimitiveValue::UnitType::Millimeters:
        value *= cssPixelsPerMillimeter;
        return LengthInterpolatedNumber;
    case CSSPrimitiveValue::UnitType::Inches:
        value *= cssPixelsPerInch;
        return LengthInterpolatedNumber;
    case CSSPrimitiveValue::UnitType::Points:
        value *= cssPixelsPerPoint;
        return LengthInterpolatedNumber;
    case CSSPrimitiveValue::UnitType::Picas:
        value *= cssPixelsPerPica;
        return LengthInterpolatedNumber;
    case CSSPrimitiveValue::UnitType::UserUnits:
        return LengthInterpolatedNumber;
    case CSSPrimitiveValue::UnitType::Rems:
        return LengthInterpolatedREMS;
    case CSSPrimitiveValue::UnitType::Chs:
        return LengthInterpolatedCHS;
    }
}

} // namespace

PassOwnPtr<InterpolableValue> LengthSVGInterpolation::toInterpolableValue(SVGLength* length, const SVGAnimatedPropertyBase* attribute, NonInterpolableType* ptrModeData)
{
    if (ptrModeData)
        populateModeData(attribute, ptrModeData);

    double value = length->valueInSpecifiedUnits();
    LengthInterpolatedUnit unitType = convertToInterpolatedUnit(length->typeWithCalcResolved(), value);

    double values[numLengthInterpolatedUnits] = { };
    values[unitType] = value;

    OwnPtr<InterpolableList> listOfValues = InterpolableList::create(numLengthInterpolatedUnits);
    for (size_t i = 0; i < numLengthInterpolatedUnits; ++i)
        listOfValues->set(i, InterpolableNumber::create(values[i]));
    return listOfValues.release();
}

PassRefPtrWillBeRawPtr<SVGLength> LengthSVGInterpolation::fromInterpolableValue(const InterpolableValue& interpolableValue, const NonInterpolableType& modeData, const SVGElement* element)
{
    const InterpolableList& listOfValues = toInterpolableList(interpolableValue);
    ASSERT(element);

    double value = 0;
    CSSPrimitiveValue::UnitType unitType = CSSPrimitiveValue::UnitType::UserUnits;
    unsigned unitTypeCount = 0;
    // We optimise for the common case where only one unit type is involved.
    for (size_t i = 0; i < numLengthInterpolatedUnits; i++) {
        double entry = toInterpolableNumber(listOfValues.get(i))->value();
        if (!entry)
            continue;
        unitTypeCount++;
        if (unitTypeCount > 1)
            break;

        value = entry;
        unitType = unitTypes[i];
    }

    if (unitTypeCount > 1) {
        value = 0;
        unitType = CSSPrimitiveValue::UnitType::UserUnits;

        // SVGLength does not support calc expressions, so we convert to canonical units.
        SVGLengthContext lengthContext(element);
        for (size_t i = 0; i < numLengthInterpolatedUnits; i++) {
            double entry = toInterpolableNumber(listOfValues.get(i))->value();
            if (entry)
                value += lengthContext.convertValueToUserUnits(entry, modeData.unitMode, unitTypes[i]);
        }
    }

    if (modeData.negativeValuesMode == ForbidNegativeLengths && value < 0)
        value = 0;

    RefPtrWillBeRawPtr<SVGLength> result = SVGLength::create(modeData.unitMode); // defaults to the length 0
    result->newValueSpecifiedUnits(unitType, value);
    return result.release();
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> LengthSVGInterpolation::interpolatedValue(SVGElement& targetElement) const
{
    return fromInterpolableValue(*m_cachedValue, m_modeData, &targetElement);
}

}
