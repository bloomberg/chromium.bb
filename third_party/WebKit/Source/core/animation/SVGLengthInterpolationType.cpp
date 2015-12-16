// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SVGLengthInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSHelper.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthContext.h"

namespace blink {

namespace {

enum LengthInterpolatedUnit {
    LengthInterpolatedNumber,
    LengthInterpolatedPercentage,
    LengthInterpolatedEMS,
    LengthInterpolatedEXS,
    LengthInterpolatedREMS,
    LengthInterpolatedCHS,
};

static const CSSPrimitiveValue::UnitType unitTypes[] = {
    CSSPrimitiveValue::UnitType::UserUnits,
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
    case CSSPrimitiveValue::UnitType::UserUnits:
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
    case CSSPrimitiveValue::UnitType::Rems:
        return LengthInterpolatedREMS;
    case CSSPrimitiveValue::UnitType::Chs:
        return LengthInterpolatedCHS;
    }
}

} // namespace

PassOwnPtr<InterpolationValue> SVGLengthInterpolationType::maybeConvertNeutral(const UnderlyingValue&, ConversionCheckers&) const
{
    OwnPtr<InterpolableList> listOfValues = InterpolableList::create(numLengthInterpolatedUnits);
    for (size_t i = 0; i < numLengthInterpolatedUnits; ++i)
        listOfValues->set(i, InterpolableNumber::create(0));

    return InterpolationValue::create(*this, listOfValues.release());
}

PassOwnPtr<InterpolationValue> SVGLengthInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedLength)
        return nullptr;

    const SVGLength& length = toSVGLength(svgValue);
    double value = length.valueInSpecifiedUnits();
    LengthInterpolatedUnit unitType = convertToInterpolatedUnit(length.typeWithCalcResolved(), value);

    double values[numLengthInterpolatedUnits] = { };
    values[unitType] = value;

    OwnPtr<InterpolableList> listOfValues = InterpolableList::create(numLengthInterpolatedUnits);
    for (size_t i = 0; i < numLengthInterpolatedUnits; ++i)
        listOfValues->set(i, InterpolableNumber::create(values[i]));

    return InterpolationValue::create(*this, listOfValues.release());
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGLengthInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    ASSERT_NOT_REACHED();
    // This function is no longer called, because apply has been overridden.
    return nullptr;
}

PassRefPtrWillBeRawPtr<SVGPropertyBase> SVGLengthInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*, const SVGLengthContext& lengthContext) const
{
    const InterpolableList& listOfValues = toInterpolableList(interpolableValue);

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
        for (size_t i = 0; i < numLengthInterpolatedUnits; i++) {
            double entry = toInterpolableNumber(listOfValues.get(i))->value();
            if (entry)
                value += lengthContext.convertValueToUserUnits(entry, m_unitMode, unitTypes[i]);
        }
    }

    if (m_negativeValuesForbidden && value < 0)
        value = 0;

    RefPtrWillBeRawPtr<SVGLength> result = SVGLength::create(m_unitMode); // defaults to the length 0
    result->newValueSpecifiedUnits(unitType, value);
    return result.release();
}

void SVGLengthInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    SVGElement& element = environment.svgElement();
    SVGLengthContext lengthContext(&element);
    element.setWebAnimatedAttribute(attribute(), appliedSVGValue(interpolableValue, nonInterpolableValue, lengthContext));
}

} // namespace blink
