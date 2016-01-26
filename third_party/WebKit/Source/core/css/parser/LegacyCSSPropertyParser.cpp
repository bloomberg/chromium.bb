/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/parser/CSSPropertyParser.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSBorderImage.h"
#include "core/css/CSSContentDistributionValue.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSValuePool.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/frame/UseCounter.h"
#include "core/style/GridCoordinate.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void CSSPropertyParser::addProperty(CSSPropertyID propId, PassRefPtrWillBeRawPtr<CSSValue> value, bool important, bool implicit)
{
    ASSERT(!isPropertyAlias(propId));

    int shorthandIndex = 0;
    bool setFromShorthand = false;

    if (m_currentShorthand) {
        Vector<StylePropertyShorthand, 4> shorthands;
        getMatchingShorthandsForLonghand(propId, &shorthands);
        setFromShorthand = true;
        if (shorthands.size() > 1)
            shorthandIndex = indexOfShorthandForLonghand(m_currentShorthand, shorthands);
    }

    m_parsedProperties.append(CSSProperty(propId, value, important, setFromShorthand, shorthandIndex, m_implicitShorthand || implicit));
}

bool CSSPropertyParser::validCalculationUnit(CSSParserValue* value, Units unitflags, ReleaseParsedCalcValueCondition releaseCalc)
{
    bool mustBeNonNegative = unitflags & (FNonNeg | FPositiveInteger);

    if (!parseCalculation(value, mustBeNonNegative ? ValueRangeNonNegative : ValueRangeAll))
        return false;

    bool b = false;
    switch (m_parsedCalculation->category()) {
    case CalcLength:
        b = (unitflags & FLength);
        break;
    case CalcNumber:
        b = (unitflags & FNumber);
        if (!b && (unitflags & (FInteger | FPositiveInteger)) && m_parsedCalculation->isInt())
            b = true;
        if (b && mustBeNonNegative && m_parsedCalculation->isNegative())
            b = false;
        // Always resolve calc() to a UnitType::Number in the CSSParserValue if there are no non-numbers specified in the unitflags.
        if (b && !(unitflags & ~(FInteger | FNumber | FPositiveInteger | FNonNeg))) {
            double number = m_parsedCalculation->doubleValue();
            if ((unitflags & FPositiveInteger) && number <= 0) {
                b = false;
            } else {
                delete value->calcFunction;
                value->setUnit(CSSPrimitiveValue::UnitType::Number);
                value->fValue = number;
                value->isInt = m_parsedCalculation->isInt();
            }
            m_parsedCalculation.release();
            return b;
        }
        break;
    case CalcPercent:
        b = (unitflags & FPercent);
        if (b && mustBeNonNegative && m_parsedCalculation->isNegative())
            b = false;
        break;
    case CalcPercentLength:
        b = (unitflags & FPercent) && (unitflags & FLength);
        break;
    case CalcPercentNumber:
        b = (unitflags & FPercent) && (unitflags & FNumber);
        break;
    case CalcAngle:
        b = (unitflags & FAngle);
        break;
    case CalcTime:
        b = (unitflags & FTime);
        break;
    case CalcFrequency:
        b = (unitflags & FFrequency);
        break;
    case CalcOther:
        break;
    }
    if (!b || releaseCalc == ReleaseParsedCalcValue)
        m_parsedCalculation.release();
    return b;
}

inline bool CSSPropertyParser::shouldAcceptUnitLessValues(CSSParserValue* value, Units unitflags, CSSParserMode cssParserMode)
{
    // Quirks mode for certain properties and presentation attributes accept unit-less values for certain units.
    return (unitflags & (FLength | FAngle))
        && (!value->fValue // 0 can always be unitless.
            || isUnitLessLengthParsingEnabledForMode(cssParserMode) // HTML and SVG attribute values can always be unitless.
            || (cssParserMode == HTMLQuirksMode && (unitflags & FUnitlessQuirk)));
}

inline bool isCalculation(CSSParserValue* value)
{
    return value->m_unit == CSSParserValue::CalcFunction;
}

bool CSSPropertyParser::validUnit(CSSParserValue* value, Units unitflags, CSSParserMode cssParserMode, ReleaseParsedCalcValueCondition releaseCalc)
{
    if (isCalculation(value))
        return validCalculationUnit(value, unitflags, releaseCalc);

    if (unitflags & FNonNeg && value->fValue < 0)
        return false;
    switch (value->unit()) {
    case CSSPrimitiveValue::UnitType::Number:
        if (unitflags & FNumber)
            return true;
        if (shouldAcceptUnitLessValues(value, unitflags, cssParserMode)) {
            value->setUnit((unitflags & FLength) ? CSSPrimitiveValue::UnitType::Pixels : CSSPrimitiveValue::UnitType::Degrees);
            return true;
        }
        if ((unitflags & FInteger) && value->isInt)
            return true;
        if ((unitflags & FPositiveInteger) && value->isInt && value->fValue > 0)
            return true;
        return false;
    case CSSPrimitiveValue::UnitType::Percentage:
        return unitflags & FPercent;
    case CSSPrimitiveValue::UnitType::QuirkyEms:
        if (cssParserMode != UASheetMode)
            return false;
    /* fallthrough intentional */
    case CSSPrimitiveValue::UnitType::Ems:
    case CSSPrimitiveValue::UnitType::Rems:
    case CSSPrimitiveValue::UnitType::Chs:
    case CSSPrimitiveValue::UnitType::Exs:
    case CSSPrimitiveValue::UnitType::Pixels:
    case CSSPrimitiveValue::UnitType::Centimeters:
    case CSSPrimitiveValue::UnitType::Millimeters:
    case CSSPrimitiveValue::UnitType::Inches:
    case CSSPrimitiveValue::UnitType::Points:
    case CSSPrimitiveValue::UnitType::Picas:
    case CSSPrimitiveValue::UnitType::UserUnits:
    case CSSPrimitiveValue::UnitType::ViewportWidth:
    case CSSPrimitiveValue::UnitType::ViewportHeight:
    case CSSPrimitiveValue::UnitType::ViewportMin:
    case CSSPrimitiveValue::UnitType::ViewportMax:
        return unitflags & FLength;
    case CSSPrimitiveValue::UnitType::Milliseconds:
    case CSSPrimitiveValue::UnitType::Seconds:
        return unitflags & FTime;
    case CSSPrimitiveValue::UnitType::Degrees:
    case CSSPrimitiveValue::UnitType::Radians:
    case CSSPrimitiveValue::UnitType::Gradians:
    case CSSPrimitiveValue::UnitType::Turns:
        return unitflags & FAngle;
    case CSSPrimitiveValue::UnitType::DotsPerPixel:
    case CSSPrimitiveValue::UnitType::DotsPerInch:
    case CSSPrimitiveValue::UnitType::DotsPerCentimeter:
        return unitflags & FResolution;
    default:
        return false;
    }
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::createPrimitiveNumericValue(CSSParserValue* value)
{
    if (m_parsedCalculation) {
        ASSERT(isCalculation(value));
        return CSSPrimitiveValue::create(m_parsedCalculation.release());
    }

    ASSERT((value->unit() >= CSSPrimitiveValue::UnitType::Number && value->unit() <= CSSPrimitiveValue::UnitType::Kilohertz)
        || (value->unit() >= CSSPrimitiveValue::UnitType::Turns && value->unit() <= CSSPrimitiveValue::UnitType::Chs)
        || (value->unit() >= CSSPrimitiveValue::UnitType::ViewportWidth && value->unit() <= CSSPrimitiveValue::UnitType::ViewportMax)
        || (value->unit() >= CSSPrimitiveValue::UnitType::DotsPerPixel && value->unit() <= CSSPrimitiveValue::UnitType::DotsPerCentimeter));
    return cssValuePool().createValue(value->fValue, value->unit());
}

inline PassRefPtrWillBeRawPtr<CSSCustomIdentValue> CSSPropertyParser::createPrimitiveCustomIdentValue(CSSParserValue* value)
{
    ASSERT(value->m_unit == CSSParserValue::String || value->m_unit == CSSParserValue::Identifier);
    return CSSCustomIdentValue::create(value->string);
}

static inline bool isComma(CSSParserValue* value)
{
    ASSERT(value);
    return value->m_unit == CSSParserValue::Operator && value->iValue == ',';
}

static bool consumeComma(CSSParserValueList* valueList)
{
    CSSParserValue* value = valueList->current();
    if (!value || !isComma(value))
        return false;
    valueList->next();
    return true;
}

static inline bool isForwardSlashOperator(CSSParserValue* value)
{
    ASSERT(value);
    return value->m_unit == CSSParserValue::Operator && value->iValue == '/';
}

inline PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseValidPrimitive(CSSValueID identifier, CSSParserValue* value)
{
    if (identifier)
        return cssValuePool().createIdentifierValue(identifier);
    if (value->unit() >= CSSPrimitiveValue::UnitType::Number && value->unit() <= CSSPrimitiveValue::UnitType::Kilohertz)
        return createPrimitiveNumericValue(value);
    if (value->unit() >= CSSPrimitiveValue::UnitType::Turns && value->unit() <= CSSPrimitiveValue::UnitType::Chs)
        return createPrimitiveNumericValue(value);
    if (value->unit() >= CSSPrimitiveValue::UnitType::ViewportWidth && value->unit() <= CSSPrimitiveValue::UnitType::ViewportMax)
        return createPrimitiveNumericValue(value);
    if (value->unit() >= CSSPrimitiveValue::UnitType::DotsPerPixel && value->unit() <= CSSPrimitiveValue::UnitType::DotsPerCentimeter)
        return createPrimitiveNumericValue(value);
    if (value->unit() == CSSPrimitiveValue::UnitType::QuirkyEms)
        return CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::UnitType::QuirkyEms);
    if (isCalculation(value))
        return CSSPrimitiveValue::create(m_parsedCalculation.release());

    return nullptr;
}

void CSSPropertyParser::addExpandedPropertyForValue(CSSPropertyID propId, PassRefPtrWillBeRawPtr<CSSValue> prpValue, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(propId);
    unsigned shorthandLength = shorthand.length();
    if (!shorthandLength) {
        addProperty(propId, prpValue, important);
        return;
    }

    RefPtrWillBeRawPtr<CSSValue> value = prpValue;
    ShorthandScope scope(this, propId);
    const CSSPropertyID* longhands = shorthand.properties();
    for (unsigned i = 0; i < shorthandLength; ++i)
        addProperty(longhands[i], value, important);
}

bool CSSPropertyParser::legacyParseAndApplyValue(CSSPropertyID propertyID, bool important)
{
    RefPtrWillBeRawPtr<CSSValue> result = legacyParseValue(propertyID);
    if (!result)
        return false;
    addProperty(propertyID, result.release(), important);
    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::legacyParseValue(CSSPropertyID unresolvedProperty)
{
    CSSPropertyID propId = resolveCSSPropertyID(unresolvedProperty);

    CSSParserValue* value = m_valueList->current();

    // Note: m_parsedCalculation is used to pass the calc value to validUnit and then cleared at the end of this function.
    // FIXME: This is to avoid having to pass parsedCalc to all validUnit callers.
    ASSERT(!m_parsedCalculation);

    CSSValueID id = value->id;

    bool validPrimitive = false;
    RefPtrWillBeRawPtr<CSSValue> parsedValue = nullptr;

    switch (propId) {
    case CSSPropertyBackgroundColor: // <color> | inherit
        parsedValue = parseColor(m_valueList->current(), inQuirksMode() && !inShorthand());
        if (parsedValue)
            m_valueList->next();
        break;

    case CSSPropertyImageOrientation:
        if (RuntimeEnabledFeatures::imageOrientationEnabled())
            validPrimitive = value->id == CSSValueFromImage || (value->unit() != CSSPrimitiveValue::UnitType::Number && validUnit(value, FAngle) && value->fValue == 0);
        break;

    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyMaskSourceType:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    {
        RefPtrWillBeRawPtr<CSSValue> dummyValue = nullptr;
        CSSPropertyID propId1, propId2;
        if (!parseFillProperty(unresolvedProperty, propId1, propId2, parsedValue, dummyValue))
            return nullptr;
        ASSERT(!dummyValue);
        break;
    }
    case CSSPropertyBottom:               // <length> | <percentage> | auto | inherit
    case CSSPropertyLeft:                 // <length> | <percentage> | auto | inherit
    case CSSPropertyRight:                // <length> | <percentage> | auto | inherit
    case CSSPropertyTop:                  // <length> | <percentage> | auto | inherit
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FLength | FPercent | FUnitlessQuirk);
        break;

    case CSSPropertyWebkitBorderImage:
        parsedValue = parseBorderImage(propId);
        break;

    case CSSPropertyBorderImageOutset:
    case CSSPropertyWebkitMaskBoxImageOutset: {
        RefPtrWillBeRawPtr<CSSQuadValue> result = nullptr;
        if (parseBorderImageOutset(result))
            return result.release();
        return nullptr;
    }
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyWebkitMaskBoxImageRepeat: {
        RefPtrWillBeRawPtr<CSSValue> result = nullptr;
        if (parseBorderImageRepeat(result))
            return result.release();
        return nullptr;
    }
    case CSSPropertyBorderImageSlice:
    case CSSPropertyWebkitMaskBoxImageSlice: {
        RefPtrWillBeRawPtr<CSSBorderImageSliceValue> result = nullptr;
        if (parseBorderImageSlice(propId, result))
            return result.release();
        return nullptr;
    }
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitMaskBoxImageWidth: {
        RefPtrWillBeRawPtr<CSSQuadValue> result = nullptr;
        if (parseBorderImageWidth(result))
            return result.release();
        return nullptr;
    }
    case CSSPropertyWebkitBoxReflect:
        if (id == CSSValueNone)
            validPrimitive = true;
        else
            parsedValue = parseReflect();
        break;
    case CSSPropertyFontSizeAdjust: // none | <number>
        ASSERT(RuntimeEnabledFeatures::cssFontSizeAdjustEnabled());
        validPrimitive = (id == CSSValueNone) ? true : validUnit(value, FNumber | FNonNeg);
        break;
    case CSSPropertyJustifyContent:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseContentDistributionOverflowPosition();
        break;
    case CSSPropertyJustifySelf:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseItemPositionOverflowPosition();
        break;
    case CSSPropertyJustifyItems:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

        if ((parsedValue = parseLegacyPosition()))
            break;

        m_valueList->setCurrentIndex(0);
        parsedValue = parseItemPositionOverflowPosition();
        break;
    case CSSPropertyGridAutoFlow:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseGridAutoFlow(*m_valueList);
        break;
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseGridTrackSize(*m_valueList);
        break;

    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseGridTrackList();
        break;

    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseGridPosition();
        break;

    case CSSPropertyGridTemplateAreas:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseGridTemplateAreas();
        break;

    case CSSPropertyWebkitClipPath:
        if (id == CSSValueNone) {
            validPrimitive = true;
        } else if (value->m_unit == CSSParserValue::Function) {
            parsedValue = parseBasicShape();
        } else if (value->m_unit == CSSParserValue::URI) {
            // TODO(timloh): This will allow trailing junk
            return CSSURIValue::create(value->string);
        }
        break;
    case CSSPropertyShapeOutside:
        if (id == CSSValueNone)
            validPrimitive = true;
        else
            parsedValue = parseShapeProperty(propId);
        break;

    case CSSPropertyAlignContent:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseContentDistributionOverflowPosition();
        break;

    case CSSPropertyAlignSelf:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseItemPositionOverflowPosition();
        break;

    case CSSPropertyAlignItems:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseItemPositionOverflowPosition();
        break;

    // Everything else is handled in CSSPropertyParser.cpp
    default:
        return nullptr;
    }

    if (validPrimitive) {
        parsedValue = parseValidPrimitive(id, value);
        m_valueList->next();
    }
    ASSERT(!m_parsedCalculation);
    if (parsedValue) {
        if (!m_valueList->current() || inShorthand())
            return parsedValue.release();
    }
    return nullptr;
}

bool CSSPropertyParser::legacyParseShorthand(CSSPropertyID propertyID, bool important)
{
    switch (propertyID) {
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskRepeat: {
        RefPtrWillBeRawPtr<CSSValue> val1 = nullptr;
        RefPtrWillBeRawPtr<CSSValue> val2 = nullptr;
        CSSPropertyID propId1, propId2;
        if (parseFillProperty(propertyID, propId1, propId2, val1, val2)) {
            ShorthandScope scope(this, propertyID);
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            m_implicitShorthand = false;
            return true;
        }
        m_implicitShorthand = false;
        return true;
    }

    case CSSPropertyBorderImage:
    case CSSPropertyWebkitMaskBoxImage:
        return parseBorderImageShorthand(propertyID, important);

    case CSSPropertyGridGap:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridGapShorthand(important);

    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridItemPositionShorthand(propertyID, important);

    case CSSPropertyGridArea:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridAreaShorthand(important);

    case CSSPropertyGridTemplate:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridTemplateShorthand(important);

    case CSSPropertyGrid:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridShorthand(important);

    case CSSPropertyBackground: {
        // Position must come before color in this array because a plain old "0" is a legal color
        // in quirks mode but it's usually the X coordinate of a position.
        const CSSPropertyID properties[] = { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat,
                                   CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition, CSSPropertyBackgroundOrigin,
                                   CSSPropertyBackgroundClip, CSSPropertyBackgroundColor, CSSPropertyBackgroundSize };
        return parseFillShorthand(propertyID, properties, WTF_ARRAY_LENGTH(properties), important);
    }
    case CSSPropertyWebkitMask: {
        const CSSPropertyID properties[] = { CSSPropertyWebkitMaskImage, CSSPropertyWebkitMaskRepeat,
            CSSPropertyWebkitMaskPosition, CSSPropertyWebkitMaskOrigin, CSSPropertyWebkitMaskClip, CSSPropertyWebkitMaskSize };
        return parseFillShorthand(propertyID, properties, WTF_ARRAY_LENGTH(properties), important);
    }

    // The remaining shorthands are handled in CSSPropertyParser.cpp
    default:
        return false;
    }
}

void CSSPropertyParser::addFillValue(RefPtrWillBeRawPtr<CSSValue>& lval, PassRefPtrWillBeRawPtr<CSSValue> rval)
{
    if (lval) {
        if (lval->isBaseValueList())
            toCSSValueList(lval.get())->append(rval);
        else {
            PassRefPtrWillBeRawPtr<CSSValue> oldlVal(lval.release());
            PassRefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(oldlVal);
            list->append(rval);
            lval = list;
        }
    }
    else
        lval = rval;
}

static bool parseBackgroundClip(CSSParserValue* parserValue, RefPtrWillBeRawPtr<CSSValue>& cssValue)
{
    if (parserValue->id == CSSValueBorderBox || parserValue->id == CSSValuePaddingBox
        || parserValue->id == CSSValueContentBox || parserValue->id == CSSValueWebkitText) {
        cssValue = cssValuePool().createIdentifierValue(parserValue->id);
        return true;
    }
    return false;
}

const int cMaxFillProperties = 9;

bool CSSPropertyParser::parseFillShorthand(CSSPropertyID propId, const CSSPropertyID* properties, int numProperties, bool important)
{
    ASSERT(numProperties <= cMaxFillProperties);
    if (numProperties > cMaxFillProperties)
        return false;

    ShorthandScope scope(this, propId);

    bool parsedProperty[cMaxFillProperties] = { false };
    RefPtrWillBeRawPtr<CSSValue> values[cMaxFillProperties];
#if ENABLE(OILPAN)
    // Zero initialize the array of raw pointers.
    memset(&values, 0, sizeof(values));
#endif
    RefPtrWillBeRawPtr<CSSValue> clipValue = nullptr;
    RefPtrWillBeRawPtr<CSSValue> positionYValue = nullptr;
    RefPtrWillBeRawPtr<CSSValue> repeatYValue = nullptr;
    bool foundClip = false;
    int i;
    bool foundPositionCSSProperty = false;

    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (isComma(val)) {
            // We hit the end.  Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (properties[i] == CSSPropertyBackgroundColor && parsedProperty[i])
                    // Color is not allowed except as the last item in a list for backgrounds.
                    // Reject the entire property.
                    return false;

                if (!parsedProperty[i] && properties[i] != CSSPropertyBackgroundColor) {
                    addFillValue(values[i], cssValuePool().createImplicitInitialValue());
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, cssValuePool().createImplicitInitialValue());
                    if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                        addFillValue(repeatYValue, cssValuePool().createImplicitInitialValue());
                    if ((properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) && !parsedProperty[i]) {
                        // If background-origin wasn't present, then reset background-clip also.
                        addFillValue(clipValue, cssValuePool().createImplicitInitialValue());
                    }
                }
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
        }

        bool sizeCSSPropertyExpected = false;
        if (isForwardSlashOperator(val) && foundPositionCSSProperty) {
            sizeCSSPropertyExpected = true;
            m_valueList->next();
        }

        foundPositionCSSProperty = false;
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {

            if (sizeCSSPropertyExpected && (properties[i] != CSSPropertyBackgroundSize && properties[i] != CSSPropertyWebkitMaskSize))
                continue;
            if (!sizeCSSPropertyExpected && (properties[i] == CSSPropertyBackgroundSize || properties[i] == CSSPropertyWebkitMaskSize))
                continue;

            if (!parsedProperty[i]) {
                RefPtrWillBeRawPtr<CSSValue> val1 = nullptr;
                RefPtrWillBeRawPtr<CSSValue> val2 = nullptr;
                CSSPropertyID propId1, propId2;
                CSSParserValue* parserValue = m_valueList->current();
                // parseFillProperty() may modify m_implicitShorthand, so we MUST reset it
                // before EACH return below.
                if (parserValue && parseFillProperty(properties[i], propId1, propId2, val1, val2)) {
                    parsedProperty[i] = found = true;
                    addFillValue(values[i], val1.release());
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, val2.release());
                    if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                        addFillValue(repeatYValue, val2.release());
                    if (properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) {
                        // Reparse the value as a clip, and see if we succeed.
                        if (parseBackgroundClip(parserValue, val1))
                            addFillValue(clipValue, val1.release()); // The property parsed successfully.
                        else
                            addFillValue(clipValue, cssValuePool().createImplicitInitialValue()); // Some value was used for origin that is not supported by clip. Just reset clip instead.
                    }
                    if (properties[i] == CSSPropertyBackgroundClip || properties[i] == CSSPropertyWebkitMaskClip) {
                        // Update clipValue
                        addFillValue(clipValue, val1.release());
                        foundClip = true;
                    }
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        foundPositionCSSProperty = true;
                }
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found) {
            m_implicitShorthand = false;
            return false;
        }
    }

    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++) {
        // Fill in any remaining properties with the initial value.
        if (!parsedProperty[i]) {
            addFillValue(values[i], cssValuePool().createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                addFillValue(positionYValue, cssValuePool().createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                addFillValue(repeatYValue, cssValuePool().createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) {
                // If background-origin wasn't present, then reset background-clip also.
                addFillValue(clipValue, cssValuePool().createImplicitInitialValue());
            }
        }
        if (properties[i] == CSSPropertyBackgroundPosition) {
            addProperty(CSSPropertyBackgroundPositionX, values[i].release(), important);
            // it's OK to call positionYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyBackgroundPositionY, positionYValue.release(), important);
        } else if (properties[i] == CSSPropertyWebkitMaskPosition) {
            addProperty(CSSPropertyWebkitMaskPositionX, values[i].release(), important);
            // it's OK to call positionYValue.release() since we only see CSSPropertyWebkitMaskPosition once
            addProperty(CSSPropertyWebkitMaskPositionY, positionYValue.release(), important);
        } else if (properties[i] == CSSPropertyBackgroundRepeat) {
            addProperty(CSSPropertyBackgroundRepeatX, values[i].release(), important);
            // it's OK to call repeatYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyBackgroundRepeatY, repeatYValue.release(), important);
        } else if (properties[i] == CSSPropertyWebkitMaskRepeat) {
            addProperty(CSSPropertyWebkitMaskRepeatX, values[i].release(), important);
            // it's OK to call repeatYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyWebkitMaskRepeatY, repeatYValue.release(), important);
        } else if ((properties[i] == CSSPropertyBackgroundClip || properties[i] == CSSPropertyWebkitMaskClip) && !foundClip)
            // Value is already set while updating origin
            continue;
        else if (properties[i] == CSSPropertyBackgroundSize && !parsedProperty[i] && m_context.useLegacyBackgroundSizeShorthandBehavior())
            continue;
        else
            addProperty(properties[i], values[i].release(), important);

        // Add in clip values when we hit the corresponding origin property.
        if (properties[i] == CSSPropertyBackgroundOrigin && !foundClip)
            addProperty(CSSPropertyBackgroundClip, clipValue.release(), important);
        else if (properties[i] == CSSPropertyWebkitMaskOrigin && !foundClip)
            addProperty(CSSPropertyWebkitMaskClip, clipValue.release(), important);
    }

    m_implicitShorthand = false;
    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseColor(const CSSParserValue* value, bool acceptQuirkyColors)
{
    CSSValueID id = value->id;
    if (isColorKeyword(id)) {
        if (!isValueAllowedInMode(id, m_context.mode()))
            return nullptr;
        if (id == CSSValueWebkitText && m_context.useCounter())
            m_context.useCounter()->count(UseCounter::WebkitTextInColorProperty);
        return cssValuePool().createIdentifierValue(id);
    }
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(value, c, acceptQuirkyColors))
        return nullptr;
    return cssValuePool().createColorValue(c);
}

bool CSSPropertyParser::parseFillImage(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value)
{
    if (valueList->current()->id == CSSValueNone) {
        value = cssValuePool().createIdentifierValue(CSSValueNone);
        return true;
    }
    if (valueList->current()->m_unit == CSSParserValue::URI) {
        value = createCSSImageValueWithReferrer(valueList->current()->string, m_context);
        return true;
    }

    if (valueList->current()->m_unit == CSSParserValue::Function) {
        if (CSSPropertyParser::isGeneratedImage(valueList->current()->function->id))
            return parseGeneratedImage(valueList, value);

        if (valueList->current()->function->id == CSSValueWebkitImageSet) {
            value = parseImageSet(m_valueList);
            if (value)
                return true;
        }
    }

    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseFillPositionX(CSSParserValueList* valueList)
{
    int id = valueList->current()->id;
    if (id == CSSValueLeft || id == CSSValueRight || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueRight)
            percent = 100;
        else if (id == CSSValueCenter)
            percent = 50;
        return cssValuePool().createValue(percent, CSSPrimitiveValue::UnitType::Percentage);
    }
    if (validUnit(valueList->current(), FPercent | FLength))
        return createPrimitiveNumericValue(valueList->current());
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseFillPositionY(CSSParserValueList* valueList)
{
    int id = valueList->current()->id;
    if (id == CSSValueTop || id == CSSValueBottom || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueBottom)
            percent = 100;
        else if (id == CSSValueCenter)
            percent = 50;
        return cssValuePool().createValue(percent, CSSPrimitiveValue::UnitType::Percentage);
    }
    if (validUnit(valueList->current(), FPercent | FLength))
        return createPrimitiveNumericValue(valueList->current());
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseFillPositionComponent(CSSParserValueList* valueList, unsigned& cumulativeFlags, FillPositionFlag& individualFlag, FillPositionParsingMode parsingMode, Units unitless)
{
    CSSValueID id = valueList->current()->id;
    if (id == CSSValueLeft || id == CSSValueTop || id == CSSValueRight || id == CSSValueBottom || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueLeft || id == CSSValueRight) {
            if (cumulativeFlags & XFillPosition)
                return nullptr;
            cumulativeFlags |= XFillPosition;
            individualFlag = XFillPosition;
            if (id == CSSValueRight)
                percent = 100;
        }
        else if (id == CSSValueTop || id == CSSValueBottom) {
            if (cumulativeFlags & YFillPosition)
                return nullptr;
            cumulativeFlags |= YFillPosition;
            individualFlag = YFillPosition;
            if (id == CSSValueBottom)
                percent = 100;
        } else if (id == CSSValueCenter) {
            // Center is ambiguous, so we're not sure which position we've found yet, an x or a y.
            percent = 50;
            cumulativeFlags |= AmbiguousFillPosition;
            individualFlag = AmbiguousFillPosition;
        }

        if (parsingMode == ResolveValuesAsKeyword)
            return cssValuePool().createIdentifierValue(id);

        return cssValuePool().createValue(percent, CSSPrimitiveValue::UnitType::Percentage);
    }
    if (validUnit(valueList->current(), FPercent | FLength | unitless)) {
        if (!cumulativeFlags) {
            cumulativeFlags |= XFillPosition;
            individualFlag = XFillPosition;
        } else if (cumulativeFlags & (XFillPosition | AmbiguousFillPosition)) {
            cumulativeFlags |= YFillPosition;
            individualFlag = YFillPosition;
        } else {
            if (m_parsedCalculation)
                m_parsedCalculation.release();
            return nullptr;
        }
        return createPrimitiveNumericValue(valueList->current());
    }
    return nullptr;
}

static bool isValueConflictingWithCurrentEdge(int value1, int value2)
{
    if ((value1 == CSSValueLeft || value1 == CSSValueRight) && (value2 == CSSValueLeft || value2 == CSSValueRight))
        return true;

    if ((value1 == CSSValueTop || value1 == CSSValueBottom) && (value2 == CSSValueTop || value2 == CSSValueBottom))
        return true;

    return false;
}

static bool isFillPositionKeyword(CSSValueID value)
{
    return value == CSSValueLeft || value == CSSValueTop || value == CSSValueBottom || value == CSSValueRight || value == CSSValueCenter;
}

void CSSPropertyParser::parse4ValuesFillPosition(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2)
{
    // [ left | right ] [ <percentage] | <length> ] && [ top | bottom ] [ <percentage> | <length> ]
    // In the case of 4 values <position> requires the second value to be a length or a percentage.
    if (isFillPositionKeyword(parsedValue2->getValueID()))
        return;

    unsigned cumulativeFlags = 0;
    FillPositionFlag value3Flag = InvalidFillPosition;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value3 = parseFillPositionComponent(valueList, cumulativeFlags, value3Flag, ResolveValuesAsKeyword);
    if (!value3)
        return;

    CSSValueID ident1 = parsedValue1->getValueID();
    CSSValueID ident3 = value3->getValueID();

    if (ident1 == CSSValueCenter)
        return;

    if (!isFillPositionKeyword(ident3) || ident3 == CSSValueCenter)
        return;

    // We need to check if the values are not conflicting, e.g. they are not on the same edge. It is
    // needed as the second call to parseFillPositionComponent was on purpose not checking it. In the
    // case of two values top 20px is invalid but in the case of 4 values it becomes valid.
    if (isValueConflictingWithCurrentEdge(ident1, ident3))
        return;

    valueList->next();

    cumulativeFlags = 0;
    FillPositionFlag value4Flag = InvalidFillPosition;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value4 = parseFillPositionComponent(valueList, cumulativeFlags, value4Flag, ResolveValuesAsKeyword);
    if (!value4)
        return;

    // 4th value must be a length or a percentage.
    if (isFillPositionKeyword(value4->getValueID()))
        return;

    value1 = CSSValuePair::create(parsedValue1, parsedValue2, CSSValuePair::DropIdenticalValues);
    value2 = CSSValuePair::create(value3, value4, CSSValuePair::DropIdenticalValues);

    if (ident1 == CSSValueTop || ident1 == CSSValueBottom)
        value1.swap(value2);

    valueList->next();
}
void CSSPropertyParser::parse3ValuesFillPosition(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2)
{
    unsigned cumulativeFlags = 0;
    FillPositionFlag value3Flag = InvalidFillPosition;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value3 = parseFillPositionComponent(valueList, cumulativeFlags, value3Flag, ResolveValuesAsKeyword);

    // value3 is not an expected value, we return.
    if (!value3)
        return;

    valueList->next();

    bool swapNeeded = false;
    CSSValueID ident1 = parsedValue1->getValueID();
    CSSValueID ident2 = parsedValue2->getValueID();
    CSSValueID ident3 = value3->getValueID();

    CSSValueID firstPositionKeyword;
    CSSValueID secondPositionKeyword;

    if (ident1 == CSSValueCenter) {
        // <position> requires the first 'center' to be followed by a keyword.
        if (!isFillPositionKeyword(ident2))
            return;

        // If 'center' is the first keyword then the last one needs to be a length.
        if (isFillPositionKeyword(ident3))
            return;

        firstPositionKeyword = CSSValueLeft;
        if (ident2 == CSSValueLeft || ident2 == CSSValueRight) {
            firstPositionKeyword = CSSValueTop;
            swapNeeded = true;
        }
        value1 = CSSValuePair::create(cssValuePool().createIdentifierValue(firstPositionKeyword), cssValuePool().createValue(50, CSSPrimitiveValue::UnitType::Percentage), CSSValuePair::DropIdenticalValues);
        value2 = CSSValuePair::create(parsedValue2, value3, CSSValuePair::DropIdenticalValues);
    } else if (ident3 == CSSValueCenter) {
        if (isFillPositionKeyword(ident2))
            return;

        secondPositionKeyword = CSSValueTop;
        if (ident1 == CSSValueTop || ident1 == CSSValueBottom) {
            secondPositionKeyword = CSSValueLeft;
            swapNeeded = true;
        }
        value1 = CSSValuePair::create(parsedValue1, parsedValue2, CSSValuePair::DropIdenticalValues);
        value2 = CSSValuePair::create(cssValuePool().createIdentifierValue(secondPositionKeyword), cssValuePool().createValue(50, CSSPrimitiveValue::UnitType::Percentage), CSSValuePair::DropIdenticalValues);
    } else {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPositionValue = nullptr;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPositionValue = nullptr;

        if (isFillPositionKeyword(ident2)) {
            // To match CSS grammar, we should only accept: [ center | left | right | bottom | top ] [ left | right | top | bottom ] [ <percentage> | <length> ].
            ASSERT(ident2 != CSSValueCenter);

            if (isFillPositionKeyword(ident3))
                return;

            secondPositionValue = value3;
            secondPositionKeyword = ident2;
            firstPositionValue = cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Percentage);
        } else {
            // Per CSS, we should only accept: [ right | left | top | bottom ] [ <percentage> | <length> ] [ center | left | right | bottom | top ].
            if (!isFillPositionKeyword(ident3))
                return;

            firstPositionValue = parsedValue2;
            secondPositionKeyword = ident3;
            secondPositionValue = cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Percentage);
        }

        if (isValueConflictingWithCurrentEdge(ident1, secondPositionKeyword))
            return;

        value1 = CSSValuePair::create(parsedValue1, firstPositionValue, CSSValuePair::DropIdenticalValues);
        value2 = CSSValuePair::create(cssValuePool().createIdentifierValue(secondPositionKeyword), secondPositionValue, CSSValuePair::DropIdenticalValues);
    }

    if (ident1 == CSSValueTop || ident1 == CSSValueBottom || swapNeeded)
        value1.swap(value2);

#if ENABLE(ASSERT)
    const CSSValuePair& first = toCSSValuePair(*value1);
    const CSSValuePair& second = toCSSValuePair(*value2);
    ident1 = toCSSPrimitiveValue(first.first()).getValueID();
    ident2 = toCSSPrimitiveValue(second.first()).getValueID();
    ASSERT(ident1 == CSSValueLeft || ident1 == CSSValueRight);
    ASSERT(ident2 == CSSValueBottom || ident2 == CSSValueTop);
#endif
}

inline bool CSSPropertyParser::isPotentialPositionValue(CSSParserValue* value)
{
    return isFillPositionKeyword(value->id) || validUnit(value, FPercent | FLength, ReleaseParsedCalcValue);
}

void CSSPropertyParser::parseFillPosition(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2, Units unitless)
{
    unsigned numberOfValues = 0;
    for (unsigned i = valueList->currentIndex(); i < valueList->size(); ++i, ++numberOfValues) {
        CSSParserValue* current = valueList->valueAt(i);
        if (!current || isComma(current) || isForwardSlashOperator(current) || !isPotentialPositionValue(current))
            break;
    }

    if (numberOfValues > 4)
        return;

    // If we are parsing two values, we can safely call the CSS 2.1 parsing function and return.
    if (numberOfValues <= 2) {
        parse2ValuesFillPosition(valueList, value1, value2, unitless);
        return;
    }

    ASSERT(numberOfValues > 2 && numberOfValues <= 4);

    CSSParserValue* value = valueList->current();

    // <position> requires the first value to be a background keyword.
    if (!isFillPositionKeyword(value->id))
        return;

    // Parse the first value. We're just making sure that it is one of the valid keywords or a percentage/length.
    unsigned cumulativeFlags = 0;
    FillPositionFlag value1Flag = InvalidFillPosition;
    FillPositionFlag value2Flag = InvalidFillPosition;
    value1 = parseFillPositionComponent(valueList, cumulativeFlags, value1Flag, ResolveValuesAsKeyword);
    if (!value1)
        return;

    valueList->next();

    // In case we are parsing more than two values, relax the check inside of parseFillPositionComponent. top 20px is
    // a valid start for <position>.
    cumulativeFlags = AmbiguousFillPosition;
    value2 = parseFillPositionComponent(valueList, cumulativeFlags, value2Flag, ResolveValuesAsKeyword);
    if (value2)
        valueList->next();
    else {
        value1.clear();
        return;
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1 = toCSSPrimitiveValue(value1.get());
    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2 = toCSSPrimitiveValue(value2.get());

    value1.clear();
    value2.clear();

    // Per CSS3 syntax, <position> can't have 'center' as its second keyword as we have more arguments to follow.
    if (parsedValue2->getValueID() == CSSValueCenter)
        return;

    if (numberOfValues == 3)
        parse3ValuesFillPosition(valueList, value1, value2, parsedValue1.release(), parsedValue2.release());
    else
        parse4ValuesFillPosition(valueList, value1, value2, parsedValue1.release(), parsedValue2.release());
}

void CSSPropertyParser::parse2ValuesFillPosition(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2, Units unitless)
{
    // Parse the first value.  We're just making sure that it is one of the valid keywords or a percentage/length.
    unsigned cumulativeFlags = 0;
    FillPositionFlag value1Flag = InvalidFillPosition;
    FillPositionFlag value2Flag = InvalidFillPosition;
    value1 = parseFillPositionComponent(valueList, cumulativeFlags, value1Flag, ResolveValuesAsPercent, unitless);
    if (!value1)
        return;

    // It only takes one value for background-position to be correctly parsed if it was specified in a shorthand (since we
    // can assume that any other values belong to the rest of the shorthand).  If we're not parsing a shorthand, though, the
    // value was explicitly specified for our property.
    CSSParserValue* value = valueList->next();

    // First check for the comma.  If so, we are finished parsing this value or value pair.
    if (value && isComma(value))
        value = 0;

    if (value) {
        value2 = parseFillPositionComponent(valueList, cumulativeFlags, value2Flag, ResolveValuesAsPercent, unitless);
        if (value2)
            valueList->next();
        else {
            if (!inShorthand()) {
                value1.clear();
                return;
            }
        }
    }

    if (!value2) {
        // Only one value was specified. If that value was not a keyword, then it sets the x position, and the y position
        // is simply 50%. This is our default.
        // For keywords, the keyword was either an x-keyword (left/right), a y-keyword (top/bottom), or an ambiguous keyword (center).
        // For left/right/center, the default of 50% in the y is still correct.
        value2 = cssValuePool().createValue(50, CSSPrimitiveValue::UnitType::Percentage);
    }

    if (value1Flag == YFillPosition || value2Flag == XFillPosition)
        value1.swap(value2);
}

void CSSPropertyParser::parseFillRepeat(RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2)
{
    CSSValueID id = m_valueList->current()->id;
    if (id == CSSValueRepeatX) {
        m_implicitShorthand = true;
        value1 = cssValuePool().createIdentifierValue(CSSValueRepeat);
        value2 = cssValuePool().createIdentifierValue(CSSValueNoRepeat);
        m_valueList->next();
        return;
    }
    if (id == CSSValueRepeatY) {
        m_implicitShorthand = true;
        value1 = cssValuePool().createIdentifierValue(CSSValueNoRepeat);
        value2 = cssValuePool().createIdentifierValue(CSSValueRepeat);
        m_valueList->next();
        return;
    }
    if (id == CSSValueRepeat || id == CSSValueNoRepeat || id == CSSValueRound || id == CSSValueSpace)
        value1 = cssValuePool().createIdentifierValue(id);
    else {
        value1 = nullptr;
        return;
    }

    CSSParserValue* value = m_valueList->next();

    // Parse the second value if one is available
    if (value && !isComma(value)) {
        id = value->id;
        if (id == CSSValueRepeat || id == CSSValueNoRepeat || id == CSSValueRound || id == CSSValueSpace) {
            value2 = cssValuePool().createIdentifierValue(id);
            m_valueList->next();
            return;
        }
    }

    // If only one value was specified, value2 is the same as value1.
    m_implicitShorthand = true;
    value2 = cssValuePool().createIdentifierValue(toCSSPrimitiveValue(value1.get())->getValueID());
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseFillSize(CSSPropertyID unresolvedProperty)
{
    CSSParserValue* value = m_valueList->current();
    m_valueList->next();

    if (value->id == CSSValueContain || value->id == CSSValueCover)
        return cssValuePool().createIdentifierValue(value->id);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1 = nullptr;

    if (value->id == CSSValueAuto)
        parsedValue1 = cssValuePool().createIdentifierValue(CSSValueAuto);
    else {
        if (!validUnit(value, FLength | FPercent))
            return nullptr;
        parsedValue1 = createPrimitiveNumericValue(value);
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2 = nullptr;
    value = m_valueList->current();
    if (value) {
        if (value->id == CSSValueAuto) {
            // `auto' is the default
            m_valueList->next();
        } else if (validUnit(value, FLength | FPercent)) {
            parsedValue2 = createPrimitiveNumericValue(value);
            m_valueList->next();
        }
    } else if (unresolvedProperty == CSSPropertyAliasWebkitBackgroundSize) {
        // For backwards compatibility we set the second value to the first if it is omitted.
        // We only need to do this for -webkit-background-size. It should be safe to let masks match
        // the real property.
        parsedValue2 = parsedValue1;
    }

    if (!parsedValue2)
        return parsedValue1;

    return CSSValuePair::create(parsedValue1.release(), parsedValue2.release(), CSSValuePair::KeepIdenticalValues);
}

bool CSSPropertyParser::parseFillProperty(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2,
    RefPtrWillBeRawPtr<CSSValue>& retValue1, RefPtrWillBeRawPtr<CSSValue>& retValue2)
{
    // We initially store the first value in value/value2, and only create
    // CSSValueLists if we have more values.
    RefPtrWillBeRawPtr<CSSValueList> values = nullptr;
    RefPtrWillBeRawPtr<CSSValueList> values2 = nullptr;
    RefPtrWillBeRawPtr<CSSValue> value = nullptr;
    RefPtrWillBeRawPtr<CSSValue> value2 = nullptr;

    retValue1 = retValue2 = nullptr;
    propId1 = resolveCSSPropertyID(propId);
    propId2 = propId1;
    if (propId == CSSPropertyBackgroundPosition) {
        propId1 = CSSPropertyBackgroundPositionX;
        propId2 = CSSPropertyBackgroundPositionY;
    } else if (propId == CSSPropertyWebkitMaskPosition) {
        propId1 = CSSPropertyWebkitMaskPositionX;
        propId2 = CSSPropertyWebkitMaskPositionY;
    } else if (propId == CSSPropertyBackgroundRepeat) {
        propId1 = CSSPropertyBackgroundRepeatX;
        propId2 = CSSPropertyBackgroundRepeatY;
    } else if (propId == CSSPropertyWebkitMaskRepeat) {
        propId1 = CSSPropertyWebkitMaskRepeatX;
        propId2 = CSSPropertyWebkitMaskRepeatY;
    }

    while (true) {
        RefPtrWillBeRawPtr<CSSValue> currValue = nullptr;
        RefPtrWillBeRawPtr<CSSValue> currValue2 = nullptr;

        Units unitless = FUnknown;
        CSSParserValue* val = m_valueList->current();
        ASSERT(val);

        switch (propId) {
        case CSSPropertyBackgroundColor:
            currValue = parseColor(val);
            if (currValue)
                m_valueList->next();
            break;
        case CSSPropertyBackgroundAttachment:
            if (val->id == CSSValueScroll || val->id == CSSValueFixed || val->id == CSSValueLocal) {
                currValue = cssValuePool().createIdentifierValue(val->id);
                m_valueList->next();
            }
            break;
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage:
            if (parseFillImage(m_valueList, currValue))
                m_valueList->next();
            break;
        case CSSPropertyWebkitBackgroundClip:
        case CSSPropertyWebkitBackgroundOrigin:
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskOrigin:
            // The first three values here are deprecated and do not apply to the version of the property that has
            // the -webkit- prefix removed.
            if (val->id == CSSValueBorder || val->id == CSSValuePadding || val->id == CSSValueContent
                || val->id == CSSValueBorderBox || val->id == CSSValuePaddingBox || val->id == CSSValueContentBox
                || ((propId == CSSPropertyWebkitBackgroundClip || propId == CSSPropertyWebkitMaskClip)
                    && (val->id == CSSValueText || val->id == CSSValueWebkitText))) {
                if (val->id == CSSValueWebkitText && m_context.useCounter())
                    m_context.useCounter()->count(UseCounter::WebkitTextInClipProperty);
                currValue = cssValuePool().createIdentifierValue(val->id);
                m_valueList->next();
            }
            break;
        case CSSPropertyBackgroundClip:
            if (parseBackgroundClip(val, currValue)) {
                if (val->id == CSSValueWebkitText && m_context.useCounter())
                    m_context.useCounter()->count(UseCounter::WebkitTextInClipProperty);
                m_valueList->next();
            }
            break;
        case CSSPropertyBackgroundOrigin:
            if (val->id == CSSValueBorderBox || val->id == CSSValuePaddingBox || val->id == CSSValueContentBox) {
                currValue = cssValuePool().createIdentifierValue(val->id);
                m_valueList->next();
            }
            break;
        case CSSPropertyBackgroundPosition:
            if (!inShorthand())
                unitless = FUnitlessQuirk;
            // fall-through
        case CSSPropertyWebkitMaskPosition:
            parseFillPosition(m_valueList, currValue, currValue2, unitless);
            // parseFillPosition advances the m_valueList pointer.
            break;
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX: {
            currValue = parseFillPositionX(m_valueList);
            if (currValue)
                m_valueList->next();
            break;
        }
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY: {
            currValue = parseFillPositionY(m_valueList);
            if (currValue)
                m_valueList->next();
            break;
        }
        case CSSPropertyWebkitBackgroundComposite:
        case CSSPropertyWebkitMaskComposite:
            if (val->id >= CSSValueClear && val->id <= CSSValuePlusLighter) {
                currValue = cssValuePool().createIdentifierValue(val->id);
                m_valueList->next();
            }
            break;
        case CSSPropertyBackgroundBlendMode:
            if (val->id == CSSValueNormal || val->id == CSSValueMultiply
                || val->id == CSSValueScreen || val->id == CSSValueOverlay || val->id == CSSValueDarken
                || val->id == CSSValueLighten ||  val->id == CSSValueColorDodge || val->id == CSSValueColorBurn
                || val->id == CSSValueHardLight || val->id == CSSValueSoftLight || val->id == CSSValueDifference
                || val->id == CSSValueExclusion || val->id == CSSValueHue || val->id == CSSValueSaturation
                || val->id == CSSValueColor || val->id == CSSValueLuminosity) {
                currValue = cssValuePool().createIdentifierValue(val->id);
                m_valueList->next();
            }
            break;
        case CSSPropertyBackgroundRepeat:
        case CSSPropertyWebkitMaskRepeat:
            parseFillRepeat(currValue, currValue2);
            // parseFillRepeat advances the m_valueList pointer
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyAliasWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize: {
            currValue = parseFillSize(propId);
            break;
        }
        case CSSPropertyMaskSourceType: {
            ASSERT(RuntimeEnabledFeatures::cssMaskSourceTypeEnabled());
            if (val->id == CSSValueAuto || val->id == CSSValueAlpha || val->id == CSSValueLuminance) {
                currValue = cssValuePool().createIdentifierValue(val->id);
                m_valueList->next();
            } else {
                currValue = nullptr;
            }
            break;
        }
        default:
            break;
        }
        if (!currValue)
            return false;

        if (value && !values) {
            values = CSSValueList::createCommaSeparated();
            values->append(value.release());
        }

        if (value2 && !values2) {
            values2 = CSSValueList::createCommaSeparated();
            values2->append(value2.release());
        }

        if (values)
            values->append(currValue.release());
        else
            value = currValue.release();
        if (currValue2) {
            if (values2)
                values2->append(currValue2.release());
            else
                value2 = currValue2.release();
        }

        // When parsing any fill shorthand property, we let it handle building up the lists for all
        // properties.
        if (inShorthand())
            break;

        if (!m_valueList->current())
            break;
        if (!consumeComma(m_valueList) || !m_valueList->current())
            return false;
    }

    if (values) {
        ASSERT(values->length());
        retValue1 = values.release();
        if (values2) {
            ASSERT(values2->length());
            retValue2 = values2.release();
        }
    } else {
        ASSERT(value);
        retValue1 = value.release();
        retValue2 = value2.release();
    }

    return true;
}

static inline bool isCSSWideKeyword(const CSSParserValue& value)
{
    return value.id == CSSValueInitial || value.id == CSSValueInherit || value.id == CSSValueUnset || value.id == CSSValueDefault;
}

static inline bool isValidCustomIdentForGridPositions(const CSSParserValue& value)
{
    // FIXME: we need a more general solution for <custom-ident> in all properties.
    return value.m_unit == CSSParserValue::Identifier && value.id != CSSValueSpan && value.id != CSSValueAuto && !isCSSWideKeyword(value);
}

// The function parses [ <integer> || <custom-ident> ] in <grid-line> (which can be stand alone or with 'span').
bool CSSPropertyParser::parseIntegerOrCustomIdentFromGridPosition(RefPtrWillBeRawPtr<CSSPrimitiveValue>& numericValue, RefPtrWillBeRawPtr<CSSCustomIdentValue>& gridLineName)
{
    CSSParserValue* value = m_valueList->current();
    if (validUnit(value, FInteger) && value->fValue) {
        numericValue = createPrimitiveNumericValue(value);
        value = m_valueList->next();
        if (value && isValidCustomIdentForGridPositions(*value)) {
            gridLineName = createPrimitiveCustomIdentValue(m_valueList->current());
            m_valueList->next();
        }
        return true;
    }

    if (isValidCustomIdentForGridPositions(*value)) {
        gridLineName = createPrimitiveCustomIdentValue(m_valueList->current());
        value = m_valueList->next();
        if (value && validUnit(value, FInteger) && value->fValue) {
            numericValue = createPrimitiveNumericValue(value);
            m_valueList->next();
        }
        return true;
    }

    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridPosition()
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueAuto) {
        m_valueList->next();
        return cssValuePool().createIdentifierValue(CSSValueAuto);
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> numericValue = nullptr;
    RefPtrWillBeRawPtr<CSSCustomIdentValue> gridLineName = nullptr;
    bool hasSeenSpanKeyword = false;

    if (parseIntegerOrCustomIdentFromGridPosition(numericValue, gridLineName)) {
        value = m_valueList->current();
        if (value && value->id == CSSValueSpan) {
            hasSeenSpanKeyword = true;
            m_valueList->next();
        }
    } else if (value->id == CSSValueSpan) {
        hasSeenSpanKeyword = true;
        if (CSSParserValue* nextValue = m_valueList->next()) {
            if (!isForwardSlashOperator(nextValue) && !parseIntegerOrCustomIdentFromGridPosition(numericValue, gridLineName))
                return nullptr;
        }
    }

    // Check that we have consumed all the value list. For shorthands, the parser will pass
    // the whole value list (including the opposite position).
    if (m_valueList->current() && !isForwardSlashOperator(m_valueList->current()))
        return nullptr;

    // If we didn't parse anything, this is not a valid grid position.
    if (!hasSeenSpanKeyword && !gridLineName && !numericValue)
        return nullptr;

    // Negative numbers are not allowed for span (but are for <integer>).
    if (hasSeenSpanKeyword && numericValue && numericValue->getIntValue() < 0)
        return nullptr;

    // For the <custom-ident> case.
    if (gridLineName && !numericValue && !hasSeenSpanKeyword)
        return CSSCustomIdentValue::create(gridLineName->value());

    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    if (hasSeenSpanKeyword)
        values->append(cssValuePool().createIdentifierValue(CSSValueSpan));
    if (numericValue)
        values->append(numericValue.release());
    if (gridLineName)
        values->append(gridLineName.release());
    ASSERT(values->length());
    return values.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> gridMissingGridPositionValue(CSSValue* value)
{
    if (value->isCustomIdentValue())
        return value;

    return cssValuePool().createIdentifierValue(CSSValueAuto);
}

bool CSSPropertyParser::parseGridItemPositionShorthand(CSSPropertyID shorthandId, bool important)
{
    ShorthandScope scope(this, shorthandId);
    const StylePropertyShorthand& shorthand = shorthandForProperty(shorthandId);
    ASSERT(shorthand.length() == 2);

    RefPtrWillBeRawPtr<CSSValue> startValue = parseGridPosition();
    if (!startValue)
        return false;

    RefPtrWillBeRawPtr<CSSValue> endValue = nullptr;
    if (m_valueList->current()) {
        if (!isForwardSlashOperator(m_valueList->current()))
            return false;

        if (!m_valueList->next())
            return false;

        endValue = parseGridPosition();
        if (!endValue || m_valueList->current())
            return false;
    } else {
        endValue = gridMissingGridPositionValue(startValue.get());
    }

    addProperty(shorthand.properties()[0], startValue, important);
    addProperty(shorthand.properties()[1], endValue, important);
    return true;
}

bool CSSPropertyParser::parseGridGapShorthand(bool important)
{
    ShorthandScope scope(this, CSSPropertyGridGap);
    ASSERT(shorthandForProperty(CSSPropertyGridGap).length() == 2);

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    if (!validUnit(value, FLength | FNonNeg))
        return false;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> columnGap = createPrimitiveNumericValue(value);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> rowGap = nullptr;

    value = m_valueList->next();
    if (value) {
        if (!validUnit(value, FLength | FNonNeg))
            return false;

        rowGap = createPrimitiveNumericValue(value);
        if (m_valueList->next())
            return false;
    } else {
        rowGap = columnGap;
    }

    addProperty(CSSPropertyGridColumnGap, columnGap, important);
    addProperty(CSSPropertyGridRowGap, rowGap, important);

    return true;
}

bool CSSPropertyParser::parseGridTemplateRowsAndAreas(PassRefPtrWillBeRawPtr<CSSValue> templateColumns, bool important)
{
    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;
    bool trailingIdentWasAdded = false;
    RefPtrWillBeRawPtr<CSSValueList> templateRows = CSSValueList::createSpaceSeparated();

    // At least template-areas strings must be defined.
    if (!m_valueList->current())
        return false;

    while (m_valueList->current()) {
        // Handle leading <custom-ident>*.
        if (!parseGridLineNames(*m_valueList, *templateRows, trailingIdentWasAdded ? toCSSGridLineNamesValue(templateRows->item(templateRows->length() - 1)) : nullptr))
            return false;

        // Handle a template-area's row.
        if (!parseGridTemplateAreasRow(gridAreaMap, rowCount, columnCount))
            return false;
        ++rowCount;

        // Handle template-rows's track-size.
        if (m_valueList->current() && m_valueList->current()->m_unit != CSSParserValue::String) {
            RefPtrWillBeRawPtr<CSSValue> value = parseGridTrackSize(*m_valueList);
            if (!value)
                return false;
            templateRows->append(value);
        } else {
            templateRows->append(cssValuePool().createIdentifierValue(CSSValueAuto));
        }

        // This will handle the trailing/leading <custom-ident>* in the grammar.
        if (!parseGridLineNames(*m_valueList, *templateRows))
            return false;
        trailingIdentWasAdded = templateRows->item(templateRows->length() - 1)->isGridLineNamesValue();
    }

    // [<track-list> /]?
    if (templateColumns)
        addProperty(CSSPropertyGridTemplateColumns, templateColumns, important);
    else
        addProperty(CSSPropertyGridTemplateColumns,  cssValuePool().createIdentifierValue(CSSValueNone), important);

    // [<line-names>? <string> [<track-size> <line-names>]? ]+
    RefPtrWillBeRawPtr<CSSValue> templateAreas = CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
    addProperty(CSSPropertyGridTemplateAreas, templateAreas.release(), important);
    addProperty(CSSPropertyGridTemplateRows, templateRows.release(), important);

    return true;
}


bool CSSPropertyParser::parseGridTemplateShorthand(bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    ShorthandScope scope(this, CSSPropertyGridTemplate);
    ASSERT(gridTemplateShorthand().length() == 3);

    // At least "none" must be defined.
    if (!m_valueList->current())
        return false;

    bool firstValueIsNone = m_valueList->current()->id == CSSValueNone;

    // 1- 'none' case.
    if (firstValueIsNone && !m_valueList->next()) {
        addProperty(CSSPropertyGridTemplateColumns, cssValuePool().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateRows, cssValuePool().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateAreas, cssValuePool().createIdentifierValue(CSSValueNone), important);
        return true;
    }

    unsigned index = 0;
    RefPtrWillBeRawPtr<CSSValue> columnsValue = nullptr;
    if (firstValueIsNone) {
        columnsValue = cssValuePool().createIdentifierValue(CSSValueNone);
    } else {
        columnsValue = parseGridTrackList();
    }

    // 2- <grid-template-columns> / <grid-template-columns> syntax.
    if (columnsValue) {
        if (!(m_valueList->current() && isForwardSlashOperator(m_valueList->current()) && m_valueList->next()))
            return false;
        index = m_valueList->currentIndex();
        if (RefPtrWillBeRawPtr<CSSValue> rowsValue = parseGridTrackList()) {
            if (m_valueList->current())
                return false;
            addProperty(CSSPropertyGridTemplateColumns, columnsValue, important);
            addProperty(CSSPropertyGridTemplateRows, rowsValue, important);
            addProperty(CSSPropertyGridTemplateAreas, cssValuePool().createIdentifierValue(CSSValueNone), important);
            return true;
        }
    }


    // 3- [<track-list> /]? [<line-names>? <string> [<track-size> <line-names>]? ]+ syntax.
    // The template-columns <track-list> can't be 'none'.
    if (firstValueIsNone)
        return false;
    // It requires to rewind parsing due to previous syntax failures.
    m_valueList->setCurrentIndex(index);
    return parseGridTemplateRowsAndAreas(columnsValue, important);
}

bool CSSPropertyParser::parseGridShorthand(bool important)
{
    ShorthandScope scope(this, CSSPropertyGrid);
    ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 8);

    // 1- <grid-template>
    if (parseGridTemplateShorthand(important)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addProperty(CSSPropertyGridAutoFlow, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridAutoColumns, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridAutoRows, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridColumnGap, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridRowGap, cssValuePool().createImplicitInitialValue(), important);
        return true;
    }

    // Need to rewind parsing to explore the alternative syntax of this shorthand.
    m_valueList->setCurrentIndex(0);

    // 2- <grid-auto-flow> [ <grid-auto-columns> [ / <grid-auto-rows> ]? ]
    if (!legacyParseAndApplyValue(CSSPropertyGridAutoFlow, important))
        return false;

    RefPtrWillBeRawPtr<CSSValue> autoColumnsValue = nullptr;
    RefPtrWillBeRawPtr<CSSValue> autoRowsValue = nullptr;

    if (m_valueList->current()) {
        autoColumnsValue = parseGridTrackSize(*m_valueList);
        if (!autoColumnsValue)
            return false;
        if (m_valueList->current()) {
            if (!isForwardSlashOperator(m_valueList->current()) || !m_valueList->next())
                return false;
            autoRowsValue = parseGridTrackSize(*m_valueList);
            if (!autoRowsValue)
                return false;
        }
        if (m_valueList->current())
            return false;
    } else {
        // Other omitted values are set to their initial values.
        autoColumnsValue = cssValuePool().createImplicitInitialValue();
        autoRowsValue = cssValuePool().createImplicitInitialValue();
    }

    // if <grid-auto-rows> value is omitted, it is set to the value specified for grid-auto-columns.
    if (!autoRowsValue)
        autoRowsValue = autoColumnsValue;

    addProperty(CSSPropertyGridAutoColumns, autoColumnsValue, important);
    addProperty(CSSPropertyGridAutoRows, autoRowsValue, important);

    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    addProperty(CSSPropertyGridTemplateColumns, cssValuePool().createImplicitInitialValue(), important);
    addProperty(CSSPropertyGridTemplateRows, cssValuePool().createImplicitInitialValue(), important);
    addProperty(CSSPropertyGridTemplateAreas, cssValuePool().createImplicitInitialValue(), important);
    addProperty(CSSPropertyGridColumnGap, cssValuePool().createImplicitInitialValue(), important);
    addProperty(CSSPropertyGridRowGap, cssValuePool().createImplicitInitialValue(), important);

    return true;
}

bool CSSPropertyParser::parseGridAreaShorthand(bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    ShorthandScope scope(this, CSSPropertyGridArea);
    const StylePropertyShorthand& shorthand = gridAreaShorthand();
    ASSERT_UNUSED(shorthand, shorthand.length() == 4);

    RefPtrWillBeRawPtr<CSSValue> rowStartValue = parseGridPosition();
    if (!rowStartValue)
        return false;

    RefPtrWillBeRawPtr<CSSValue> columnStartValue = nullptr;
    if (!parseSingleGridAreaLonghand(columnStartValue))
        return false;

    RefPtrWillBeRawPtr<CSSValue> rowEndValue = nullptr;
    if (!parseSingleGridAreaLonghand(rowEndValue))
        return false;

    RefPtrWillBeRawPtr<CSSValue> columnEndValue = nullptr;
    if (!parseSingleGridAreaLonghand(columnEndValue))
        return false;

    if (!columnStartValue)
        columnStartValue = gridMissingGridPositionValue(rowStartValue.get());

    if (!rowEndValue)
        rowEndValue = gridMissingGridPositionValue(rowStartValue.get());

    if (!columnEndValue)
        columnEndValue = gridMissingGridPositionValue(columnStartValue.get());

    addProperty(CSSPropertyGridRowStart, rowStartValue, important);
    addProperty(CSSPropertyGridColumnStart, columnStartValue, important);
    addProperty(CSSPropertyGridRowEnd, rowEndValue, important);
    addProperty(CSSPropertyGridColumnEnd, columnEndValue, important);
    return true;
}

bool CSSPropertyParser::parseSingleGridAreaLonghand(RefPtrWillBeRawPtr<CSSValue>& property)
{
    if (!m_valueList->current())
        return true;

    if (!isForwardSlashOperator(m_valueList->current()))
        return false;

    if (!m_valueList->next())
        return false;

    property = parseGridPosition();
    return true;
}

static inline bool isClosingBracket(const CSSParserValue& value)
{
    return value.m_unit == CSSParserValue::Operator && value.iValue == ']';
}

bool CSSPropertyParser::parseGridLineNames(CSSParserValueList& inputList, CSSValueList& valueList, CSSGridLineNamesValue* previousNamedAreaTrailingLineNames)
{
    if (!inputList.current() || inputList.current()->m_unit != CSSParserValue::Operator || inputList.current()->iValue != '[')
        return true;

    // Skip '['
    inputList.next();

    RefPtrWillBeRawPtr<CSSGridLineNamesValue> lineNames = previousNamedAreaTrailingLineNames;
    if (!lineNames)
        lineNames = CSSGridLineNamesValue::create();

    while (CSSParserValue* identValue = inputList.current()) {
        if (isClosingBracket(*identValue))
            break;

        if (!isValidCustomIdentForGridPositions(*identValue))
            return false;

        RefPtrWillBeRawPtr<CSSCustomIdentValue> lineName = createPrimitiveCustomIdentValue(identValue);
        lineNames->append(lineName.release());
        inputList.next();
    }

    if (!inputList.current() || !isClosingBracket(*inputList.current()))
        return false;

    if (!previousNamedAreaTrailingLineNames)
        valueList.append(lineNames.release());

    // Consume ']'
    inputList.next();
    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridTrackList()
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueNone) {
        m_valueList->next();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }

    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    // Handle leading  <custom-ident>*.
    if (!parseGridLineNames(*m_valueList, *values))
        return nullptr;

    bool seenTrackSizeOrRepeatFunction = false;
    bool seenAutoRepeat = false;
    while (CSSParserValue* currentValue = m_valueList->current()) {
        if (isForwardSlashOperator(currentValue))
            break;
        if (currentValue->m_unit == CSSParserValue::Function && currentValue->function->id == CSSValueRepeat) {
            bool isAutoRepeat;
            if (!parseGridTrackRepeatFunction(*values, isAutoRepeat))
                return nullptr;
            if (isAutoRepeat && seenAutoRepeat)
                return nullptr;
            seenTrackSizeOrRepeatFunction = true;
            seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
        } else {
            RefPtrWillBeRawPtr<CSSValue> value = parseGridTrackSize(*m_valueList, seenAutoRepeat ? FixedSizeOnly : AllowAll);
            if (!value)
                return nullptr;
            values->append(value);
            seenTrackSizeOrRepeatFunction = true;
        }
        // This will handle the trailing <custom-ident>* in the grammar.
        if (!parseGridLineNames(*m_valueList, *values))
            return nullptr;
    }

    // We should have found a <track-size> or else it is not a valid <track-list>
    if (!seenTrackSizeOrRepeatFunction)
        return nullptr;

    // <auto-repeat> requires definite minimum track sizes in order to compute the number of repetitions.
    // The above while loop detects those appearances after the <auto-repeat> but not the ones before.
    if (seenAutoRepeat) {
        for (auto value : *values) {
            if (value->isGridLineNamesValue())
                continue;
            ASSERT(value->isPrimitiveValue() || (value->isFunctionValue() && toCSSFunctionValue(*value).item(0)));
            const CSSPrimitiveValue& primitiveValue = value->isPrimitiveValue()
                ? toCSSPrimitiveValue(*value)
                : toCSSPrimitiveValue(*toCSSFunctionValue(*value).item(0));
            CSSValueID valueID = primitiveValue.getValueID();
            if (valueID == CSSValueMinContent || valueID == CSSValueMaxContent || valueID == CSSValueAuto || primitiveValue.isFlex())
                return nullptr;
        }
    }

    return values;
}

bool CSSPropertyParser::parseGridTrackRepeatFunction(CSSValueList& list, bool& isAutoRepeat)
{
    CSSParserValueList* arguments = m_valueList->current()->function->args.get();
    if (!arguments || arguments->size() < 3 || !isComma(arguments->valueAt(1)))
        return false;

    CSSParserValue* currentValue = arguments->valueAt(0);
    isAutoRepeat = currentValue->id == CSSValueAutoFill || currentValue->id == CSSValueAutoFit;
    if (!isAutoRepeat && !validUnit(currentValue, FPositiveInteger))
        return false;

    // The number of repetitions for <auto-repeat> is not important at parsing level
    // because it will be computed later, let's set it to 1.
    size_t repetitions = isAutoRepeat ? 1 : clampTo<size_t>(currentValue->fValue, 0, kGridMaxTracks);

    RefPtrWillBeRawPtr<CSSValueList> repeatedValues = CSSValueList::createSpaceSeparated();
    arguments->next(); // Skip the repetition count.
    arguments->next(); // Skip the comma.

    // Handle leading <custom-ident>*.
    if (!parseGridLineNames(*arguments, *repeatedValues))
        return false;

    size_t numberOfTracks = 0;
    TrackSizeRestriction restriction = isAutoRepeat ? FixedSizeOnly : AllowAll;
    while (arguments->current()) {
        if (isAutoRepeat && numberOfTracks)
            return false;

        RefPtrWillBeRawPtr<CSSValue> trackSize = parseGridTrackSize(*arguments, restriction);
        if (!trackSize)
            return false;

        repeatedValues->append(trackSize);
        ++numberOfTracks;

        // This takes care of any trailing <custom-ident>* in the grammar.
        if (!parseGridLineNames(*arguments, *repeatedValues))
            return false;
    }

    // We should have found at least one <track-size> or else it is not a valid <track-list>.
    if (!numberOfTracks)
        return false;

    // We clamp the number of repetitions to a multiple of the repeat() track list's size, while staying below the max
    // grid size.
    repetitions = std::min(repetitions, kGridMaxTracks / numberOfTracks);

    for (size_t i = 0; i < repetitions; ++i) {
        for (size_t j = 0; j < repeatedValues->length(); ++j)
            list.append(repeatedValues->item(j));
    }

    // parseGridTrackSize iterated over the repeat arguments, move to the next value.
    m_valueList->next();
    return true;
}


PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridTrackSize(CSSParserValueList& inputList, TrackSizeRestriction restriction)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSParserValue* currentValue = inputList.current();
    inputList.next();

    if (currentValue->id == CSSValueAuto)
        return restriction == AllowAll ? cssValuePool().createIdentifierValue(CSSValueAuto) : nullptr;

    if (currentValue->m_unit == CSSParserValue::Function && currentValue->function->id == CSSValueMinmax) {
        // The spec defines the following grammar: minmax( <track-breadth> , <track-breadth> )
        CSSParserValueList* arguments = currentValue->function->args.get();
        if (!arguments || arguments->size() != 3 || !isComma(arguments->valueAt(1)))
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> minTrackBreadth = parseGridBreadth(arguments->valueAt(0), restriction);
        if (!minTrackBreadth)
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> maxTrackBreadth = parseGridBreadth(arguments->valueAt(2));
        if (!maxTrackBreadth)
            return nullptr;

        RefPtrWillBeRawPtr<CSSFunctionValue> result = CSSFunctionValue::create(CSSValueMinmax);
        result->append(minTrackBreadth);
        result->append(maxTrackBreadth);
        return result.release();
    }

    return parseGridBreadth(currentValue, restriction);
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseGridBreadth(CSSParserValue* currentValue, TrackSizeRestriction restriction)
{
    if (currentValue->id == CSSValueMinContent || currentValue->id == CSSValueMaxContent || currentValue->id == CSSValueAuto)
        return restriction == AllowAll ? cssValuePool().createIdentifierValue(currentValue->id) : nullptr;

    if (currentValue->unit() == CSSPrimitiveValue::UnitType::Fraction) {
        if (restriction == FixedSizeOnly)
            return nullptr;

        double flexValue = currentValue->fValue;

        // Fractional unit is a non-negative dimension.
        if (flexValue < 0)
            return nullptr;

        return cssValuePool().createValue(flexValue, CSSPrimitiveValue::UnitType::Fraction);
    }

    if (!validUnit(currentValue, FNonNeg | FLength | FPercent))
        return nullptr;

    return createPrimitiveNumericValue(currentValue);
}

static Vector<String> parseGridTemplateAreasColumnNames(const String& gridRowNames)
{
    ASSERT(!gridRowNames.isEmpty());
    Vector<String> columnNames;
    // Using StringImpl to avoid checks and indirection in every call to String::operator[].
    StringImpl& text = *gridRowNames.impl();

    StringBuilder areaName;
    for (unsigned i = 0; i < text.length(); ++i) {
        if (text[i] == ' ') {
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
            continue;
        }
        if (text[i] == '.') {
            if (areaName == ".")
                continue;
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        } else {
            if (areaName == ".") {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        }

        areaName.append(text[i]);
    }

    if (!areaName.isEmpty())
        columnNames.append(areaName.toString());

    return columnNames;
}

bool CSSPropertyParser::parseGridTemplateAreasRow(NamedGridAreaMap& gridAreaMap, const size_t rowCount, size_t& columnCount)
{
    CSSParserValue* currentValue = m_valueList->current();
    if (!currentValue || currentValue->m_unit != CSSParserValue::String)
        return false;

    String gridRowNames = currentValue->string;
    if (gridRowNames.isEmpty() || gridRowNames.containsOnlyWhitespace())
        return false;

    Vector<String> columnNames = parseGridTemplateAreasColumnNames(gridRowNames);
    if (!columnCount) {
        columnCount = columnNames.size();
        ASSERT(columnCount);
    } else if (columnCount != columnNames.size()) {
        // The declaration is invalid is all the rows don't have the number of columns.
        return false;
    }

    for (size_t currentCol = 0; currentCol < columnCount; ++currentCol) {
        const String& gridAreaName = columnNames[currentCol];

        // Unamed areas are always valid (we consider them to be 1x1).
        if (gridAreaName == ".")
            continue;

        // We handle several grid areas with the same name at once to simplify the validation code.
        size_t lookAheadCol;
        for (lookAheadCol = currentCol + 1; lookAheadCol < columnCount; ++lookAheadCol) {
            if (columnNames[lookAheadCol] != gridAreaName)
                break;
        }

        NamedGridAreaMap::iterator gridAreaIt = gridAreaMap.find(gridAreaName);
        if (gridAreaIt == gridAreaMap.end()) {
            gridAreaMap.add(gridAreaName, GridCoordinate(GridSpan::translatedDefiniteGridSpan(rowCount, rowCount + 1), GridSpan::translatedDefiniteGridSpan(currentCol, lookAheadCol)));
        } else {
            GridCoordinate& gridCoordinate = gridAreaIt->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (rowCount != gridCoordinate.rows.resolvedFinalPosition())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentCol != gridCoordinate.columns.resolvedInitialPosition())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadCol != gridCoordinate.columns.resolvedFinalPosition())
                return false;

            gridCoordinate.rows = GridSpan::translatedDefiniteGridSpan(gridCoordinate.rows.resolvedInitialPosition(), gridCoordinate.rows.resolvedFinalPosition() + 1);
        }
        currentCol = lookAheadCol - 1;
    }

    m_valueList->next();
    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridTemplateAreas()
{
    if (m_valueList->current() && m_valueList->current()->id == CSSValueNone) {
        m_valueList->next();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }

    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;

    while (m_valueList->current()) {
        if (!parseGridTemplateAreasRow(gridAreaMap, rowCount, columnCount))
            return nullptr;
        ++rowCount;
    }

    if (!rowCount || !columnCount)
        return nullptr;

    return CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridAutoFlow(CSSParserValueList& list)
{
    // [ row | column ] || dense
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSParserValue* value = list.current();
    if (!value)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();

    // First parameter.
    CSSValueID firstId = value->id;
    if (firstId != CSSValueRow && firstId != CSSValueColumn && firstId != CSSValueDense)
        return nullptr;
    parsedValues->append(cssValuePool().createIdentifierValue(firstId));

    // Second parameter, if any.
    value = list.next();
    if (value) {
        switch (firstId) {
        case CSSValueRow:
        case CSSValueColumn:
            if (value->id != CSSValueDense)
                return parsedValues;
            break;
        case CSSValueDense:
            if (value->id != CSSValueRow && value->id != CSSValueColumn)
                return parsedValues;
            break;
        default:
            return parsedValues;
        }
        parsedValues->append(cssValuePool().createIdentifierValue(value->id));
        list.next();
    }

    return parsedValues;
}

void completeBorderRadii(RefPtrWillBeRawPtr<CSSPrimitiveValue> radii[4])
{
    if (radii[3])
        return;
    if (!radii[2]) {
        if (!radii[1])
            radii[1] = radii[0];
        radii[2] = radii[0];
    }
    radii[3] = radii[1];
}

PassRefPtrWillBeRawPtr<CSSBasicShapeInsetValue> CSSPropertyParser::parseBasicShapeInset(CSSParserValueList* args)
{
    ASSERT(args);

    RefPtrWillBeRawPtr<CSSBasicShapeInsetValue> shape = CSSBasicShapeInsetValue::create();

    CSSParserValue* argument = args->current();
    WillBeHeapVector<RefPtrWillBeMember<CSSPrimitiveValue>> widthArguments;
    bool hasRoundedInset = false;

    while (argument) {
        if (argument->m_unit == CSSParserValue::Identifier && argument->id == CSSValueRound) {
            if (!args->next())
                return nullptr;
            hasRoundedInset = true;
            break;
        }

        Units unitFlags = FLength | FPercent;
        if (!validUnit(argument, unitFlags) || widthArguments.size() > 4)
            return nullptr;

        widthArguments.append(createPrimitiveNumericValue(argument));
        argument = args->next();
    }

    switch (widthArguments.size()) {
    case 1: {
        shape->updateShapeSize1Value(widthArguments[0].get());
        break;
    }
    case 2: {
        shape->updateShapeSize2Values(widthArguments[0].get(), widthArguments[1].get());
        break;
        }
    case 3: {
        shape->updateShapeSize3Values(widthArguments[0].get(), widthArguments[1].get(), widthArguments[2].get());
        break;
    }
    case 4: {
        shape->updateShapeSize4Values(widthArguments[0].get(), widthArguments[1].get(), widthArguments[2].get(), widthArguments[3].get());
        break;
    }
    default:
        return nullptr;
    }

    if (hasRoundedInset) {
        // FIXME: Refactor completeBorderRadii and the array
        RefPtrWillBeRawPtr<CSSPrimitiveValue> radii[4];
        RefPtrWillBeRawPtr<CSSPrimitiveValue> radii2[4];
        if (!parseRadii(radii, radii2, args))
            return nullptr;
        shape->setTopLeftRadius(CSSValuePair::create(radii[0].release(), radii2[0].release(), CSSValuePair::DropIdenticalValues));
        shape->setTopRightRadius(CSSValuePair::create(radii[1].release(), radii2[1].release(), CSSValuePair::DropIdenticalValues));
        shape->setBottomRightRadius(CSSValuePair::create(radii[2].release(), radii2[2].release(), CSSValuePair::DropIdenticalValues));
        shape->setBottomLeftRadius(CSSValuePair::create(radii[3].release(), radii2[3].release(), CSSValuePair::DropIdenticalValues));
    }

    return shape.release();
}

static bool isContentDistributionKeyword(CSSValueID id)
{
    return id == CSSValueSpaceBetween || id == CSSValueSpaceAround
        || id == CSSValueSpaceEvenly || id == CSSValueStretch;
}

static bool isContentPositionKeyword(CSSValueID id)
{
    return id == CSSValueStart || id == CSSValueEnd || id == CSSValueCenter
        || id == CSSValueFlexStart || id == CSSValueFlexEnd
        || id == CSSValueLeft || id == CSSValueRight;
}

static bool isBaselinePositionKeyword(CSSValueID id)
{
    return id == CSSValueBaseline || id == CSSValueLastBaseline;
}

static bool isAlignmentOverflowKeyword(CSSValueID id)
{
    return id == CSSValueUnsafe || id == CSSValueSafe;
}

static bool isItemPositionKeyword(CSSValueID id)
{
    return id == CSSValueStart || id == CSSValueEnd || id == CSSValueCenter
        || id == CSSValueSelfStart || id == CSSValueSelfEnd || id == CSSValueFlexStart
        || id == CSSValueFlexEnd || id == CSSValueLeft || id == CSSValueRight;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseLegacyPosition()
{
    // [ legacy && [ left | right | center ]

    CSSParserValue* value = m_valueList->current();
    ASSERT(value);

    if (value->id == CSSValueLegacy) {
        value = m_valueList->next();
        if (!value)
            return nullptr;
        if (value->id != CSSValueCenter && value->id != CSSValueLeft && value->id != CSSValueRight)
            return nullptr;
    } else if (value->id == CSSValueCenter || value->id == CSSValueLeft || value->id == CSSValueRight) {
        if (!m_valueList->next() || m_valueList->current()->id != CSSValueLegacy)
            return nullptr;
    } else {
        return nullptr;
    }

    m_valueList->next();
    return CSSValuePair::create(cssValuePool().createIdentifierValue(CSSValueLegacy), cssValuePool().createIdentifierValue(value->id), CSSValuePair::DropIdenticalValues);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseContentDistributionOverflowPosition()
{
    // auto | <baseline-position> | <content-distribution> || [ <overflow-position>? && <content-position> ]
    // <baseline-position> = baseline | last-baseline;
    // <content-distribution> = space-between | space-around | space-evenly | stretch;
    // <content-position> = center | start | end | flex-start | flex-end | left | right;
    // <overflow-position> = true | safe

    // auto | <baseline-position>
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueAuto || isBaselinePositionKeyword(value->id)) {
        m_valueList->next();
        return CSSContentDistributionValue::create(CSSValueInvalid, value->id, CSSValueInvalid);
    }

    CSSValueID distribution = CSSValueInvalid;
    CSSValueID position = CSSValueInvalid;
    CSSValueID overflow = CSSValueInvalid;
    while (value) {
        if (isContentDistributionKeyword(value->id)) {
            if (distribution != CSSValueInvalid)
                return nullptr;
            distribution = value->id;
        } else if (isContentPositionKeyword(value->id)) {
            if (position != CSSValueInvalid)
                return nullptr;
            position = value->id;
        } else if (isAlignmentOverflowKeyword(value->id)) {
            if (overflow != CSSValueInvalid)
                return nullptr;
            overflow = value->id;
        } else {
            return nullptr;
        }
        value = m_valueList->next();
    }

    // The grammar states that we should have at least <content-distribution> or
    // <content-position> ( <content-distribution> || <content-position> ).
    if (position == CSSValueInvalid && distribution == CSSValueInvalid)
        return nullptr;

    // The grammar states that <overflow-position> must be associated to <content-position>.
    if (overflow != CSSValueInvalid && position == CSSValueInvalid)
        return nullptr;

    return CSSContentDistributionValue::create(distribution, position, overflow);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseItemPositionOverflowPosition()
{
    // auto | stretch | <baseline-position> | [<item-position> && <overflow-position>? ]
    // <baseline-position> = baseline | last-baseline;
    // <item-position> = center | start | end | self-start | self-end | flex-start | flex-end | left | right;
    // <overflow-position> = true | safe

    CSSParserValue* value = m_valueList->current();
    ASSERT(value);

    if (value->id == CSSValueAuto || value->id == CSSValueStretch || isBaselinePositionKeyword(value->id)) {
        m_valueList->next();
        return cssValuePool().createIdentifierValue(value->id);
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> position = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> overflowAlignmentKeyword = nullptr;
    if (isItemPositionKeyword(value->id)) {
        position = cssValuePool().createIdentifierValue(value->id);
        value = m_valueList->next();
        if (value) {
            if (isAlignmentOverflowKeyword(value->id))
                overflowAlignmentKeyword = cssValuePool().createIdentifierValue(value->id);
            else
                return nullptr;
        }
    } else if (isAlignmentOverflowKeyword(value->id)) {
        overflowAlignmentKeyword = cssValuePool().createIdentifierValue(value->id);
        value = m_valueList->next();
        if (value && isItemPositionKeyword(value->id))
            position = cssValuePool().createIdentifierValue(value->id);
        else
            return nullptr;
    } else {
        return nullptr;
    }

    m_valueList->next();

    ASSERT(position);
    if (overflowAlignmentKeyword)
        return CSSValuePair::create(position, overflowAlignmentKeyword, CSSValuePair::DropIdenticalValues);
    return position.release();
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseShapeRadius(CSSParserValue* value)
{
    if (value->id == CSSValueClosestSide || value->id == CSSValueFarthestSide)
        return cssValuePool().createIdentifierValue(value->id);

    if (!validUnit(value, FLength | FPercent | FNonNeg))
        return nullptr;

    return createPrimitiveNumericValue(value);
}

PassRefPtrWillBeRawPtr<CSSBasicShapeCircleValue> CSSPropertyParser::parseBasicShapeCircle(CSSParserValueList* args)
{
    ASSERT(args);

    // circle(radius)
    // circle(radius at <position>)
    // circle(at <position>)
    // where position defines centerX and centerY using a CSS <position> data type.
    RefPtrWillBeRawPtr<CSSBasicShapeCircleValue> shape = CSSBasicShapeCircleValue::create();

    for (CSSParserValue* argument = args->current(); argument; argument = args->next()) {
        // The call to parseFillPosition below should consume all of the
        // arguments except the first two. Thus, and index greater than one
        // indicates an invalid production.
        if (args->currentIndex() > 1)
            return nullptr;

        if (!args->currentIndex() && argument->id != CSSValueAt) {
            if (RefPtrWillBeRawPtr<CSSPrimitiveValue> radius = parseShapeRadius(argument)) {
                shape->setRadius(radius);
                continue;
            }

            return nullptr;
        }

        if (argument->id == CSSValueAt && args->next()) {
            RefPtrWillBeRawPtr<CSSValue> centerX = nullptr;
            RefPtrWillBeRawPtr<CSSValue> centerY = nullptr;
            parseFillPosition(args, centerX, centerY);
            if (centerX && centerY && !args->current()) {
                shape->setCenterX(centerX);
                shape->setCenterY(centerY);
            } else {
                return nullptr;
            }
        } else {
            return nullptr;
        }
    }

    return shape.release();
}

PassRefPtrWillBeRawPtr<CSSBasicShapeEllipseValue> CSSPropertyParser::parseBasicShapeEllipse(CSSParserValueList* args)
{
    ASSERT(args);

    // ellipse(radiusX)
    // ellipse(radiusX at <position>)
    // ellipse(radiusX radiusY)
    // ellipse(radiusX radiusY at <position>)
    // ellipse(at <position>)
    // where position defines centerX and centerY using a CSS <position> data type.
    RefPtrWillBeRawPtr<CSSBasicShapeEllipseValue> shape = CSSBasicShapeEllipseValue::create();

    for (CSSParserValue* argument = args->current(); argument; argument = args->next()) {
        // The call to parseFillPosition below should consume all of the
        // arguments except the first three. Thus, an index greater than two
        // indicates an invalid production.
        if (args->currentIndex() > 2)
            return nullptr;

        if (args->currentIndex() < 2 && argument->id != CSSValueAt) {
            if (RefPtrWillBeRawPtr<CSSPrimitiveValue> radius = parseShapeRadius(argument)) {
                if (!shape->radiusX())
                    shape->setRadiusX(radius);
                else
                    shape->setRadiusY(radius);
                continue;
            }

            return nullptr;
        }

        if (argument->id != CSSValueAt || !args->next()) // expecting ellipse(.. at <position>)
            return nullptr;
        RefPtrWillBeRawPtr<CSSValue> centerX = nullptr;
        RefPtrWillBeRawPtr<CSSValue> centerY = nullptr;
        parseFillPosition(args, centerX, centerY);
        if (!centerX || !centerY || args->current())
            return nullptr;

        shape->setCenterX(centerX);
        shape->setCenterY(centerY);
    }

    return shape.release();
}

PassRefPtrWillBeRawPtr<CSSBasicShapePolygonValue> CSSPropertyParser::parseBasicShapePolygon(CSSParserValueList* args)
{
    ASSERT(args);

    unsigned size = args->size();
    if (!size)
        return nullptr;

    RefPtrWillBeRawPtr<CSSBasicShapePolygonValue> shape = CSSBasicShapePolygonValue::create();

    CSSParserValue* argument = args->current();
    if (argument->id == CSSValueEvenodd || argument->id == CSSValueNonzero) {
        shape->setWindRule(argument->id == CSSValueEvenodd ? RULE_EVENODD : RULE_NONZERO);
        args->next();

        if (!consumeComma(args))
            return nullptr;

        size -= 2;
    }

    // <length> <length>, ... <length> <length> -> each pair has 3 elements except the last one
    if (!size || (size % 3) - 2)
        return nullptr;

    while (true) {
        CSSParserValue* argumentX = args->current();
        if (!argumentX || !validUnit(argumentX, FLength | FPercent))
            return nullptr;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> xLength = createPrimitiveNumericValue(argumentX);

        CSSParserValue* argumentY = args->next();
        if (!argumentY || !validUnit(argumentY, FLength | FPercent))
            return nullptr;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> yLength = createPrimitiveNumericValue(argumentY);

        shape->appendPoint(xLength.release(), yLength.release());

        if (!args->next())
            break;
        if (!consumeComma(args))
            return nullptr;
    }

    return shape.release();
}

static bool isBoxValue(CSSValueID valueId)
{
    switch (valueId) {
    case CSSValueContentBox:
    case CSSValuePaddingBox:
    case CSSValueBorderBox:
    case CSSValueMarginBox:
        return true;
    default:
        break;
    }

    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseShapeProperty(CSSPropertyID propId)
{
    RefPtrWillBeRawPtr<CSSValue> imageValue = nullptr;
    if (parseFillImage(m_valueList, imageValue)) {
        m_valueList->next();
        return imageValue.release();
    }

    return parseBasicShapeAndOrBox();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseBasicShapeAndOrBox()
{
    CSSParserValue* value = m_valueList->current();

    bool shapeFound = false;
    bool boxFound = false;
    CSSValueID valueId;

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (unsigned i = 0; i < 2; ++i) {
        if (!value)
            break;
        valueId = value->id;
        if (value->m_unit == CSSParserValue::Function && !shapeFound) {
            // parseBasicShape already asks for the next value list item.
            RefPtrWillBeRawPtr<CSSValue> shapeValue = parseBasicShape();
            if (!shapeValue)
                return nullptr;
            list->append(shapeValue.release());
            shapeFound = true;
        } else if (isBoxValue(valueId) && !boxFound) {
            list->append(parseValidPrimitive(valueId, value));
            boxFound = true;
            m_valueList->next();
        } else {
            return nullptr;
        }

        value = m_valueList->current();
    }

    if (m_valueList->current())
        return nullptr;
    return list.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseBasicShape()
{
    CSSParserValue* value = m_valueList->current();
    ASSERT(value->m_unit == CSSParserValue::Function);
    CSSParserValueList* args = value->function->args.get();

    if (!args)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValue> shape = nullptr;
    if (value->function->id == CSSValueCircle)
        shape = parseBasicShapeCircle(args);
    else if (value->function->id == CSSValueEllipse)
        shape = parseBasicShapeEllipse(args);
    else if (value->function->id == CSSValuePolygon)
        shape = parseBasicShapePolygon(args);
    else if (value->function->id == CSSValueInset)
        shape = parseBasicShapeInset(args);

    if (!shape)
        return nullptr;

    m_valueList->next();

    return shape.release();
}

inline int CSSPropertyParser::colorIntFromValue(CSSParserValue* v)
{
    bool isPercent;
    double value;

    if (m_parsedCalculation) {
        isPercent = m_parsedCalculation->category() == CalcPercent;
        value = m_parsedCalculation->doubleValue();
        m_parsedCalculation.release();
    } else {
        isPercent = v->unit() == CSSPrimitiveValue::UnitType::Percentage;
        value = v->fValue;
    }

    if (value <= 0.0)
        return 0;

    if (isPercent) {
        if (value >= 100.0)
            return 255;
        return static_cast<int>(value * 256.0 / 100.0);
    }

    if (value >= 255.0)
        return 255;

    return static_cast<int>(value);
}

bool CSSPropertyParser::parseColorParameters(const CSSParserValue* value, int* colorArray, bool parseAlpha)
{
    CSSParserValueList* args = value->function->args.get();
    CSSParserValue* v = args->current();
    Units unitType = FUnknown;
    // Get the first value and its type
    if (validUnit(v, FInteger))
        unitType = FInteger;
    else if (validUnit(v, FPercent))
        unitType = FPercent;
    else
        return false;

    colorArray[0] = colorIntFromValue(v);
    for (int i = 1; i < 3; i++) {
        args->next();
        if (!consumeComma(args))
            return false;
        v = args->current();
        if (!validUnit(v, unitType))
            return false;
        colorArray[i] = colorIntFromValue(v);
    }
    if (parseAlpha) {
        args->next();
        if (!consumeComma(args))
            return false;
        v = args->current();
        if (!validUnit(v, FNumber))
            return false;
        // Convert the floating pointer number of alpha to an integer in the range [0, 256),
        // with an equal distribution across all 256 values.
        colorArray[3] = static_cast<int>(std::max(0.0, std::min(1.0, v->fValue)) * nextafter(256.0, 0.0));
    }
    return true;
}

// The CSS3 specification defines the format of a HSL color as
// hsl(<number>, <percent>, <percent>)
// and with alpha, the format is
// hsla(<number>, <percent>, <percent>, <number>)
// The first value, HUE, is in an angle with a value between 0 and 360
bool CSSPropertyParser::parseHSLParameters(const CSSParserValue* value, double* colorArray, bool parseAlpha)
{
    CSSParserValueList* args = value->function->args.get();
    CSSParserValue* v = args->current();
    // Get the first value
    if (!validUnit(v, FNumber))
        return false;
    // normalize the Hue value and change it to be between 0 and 1.0
    colorArray[0] = (((static_cast<int>(v->fValue) % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; i++) {
        args->next();
        if (!consumeComma(args))
            return false;
        v = args->current();
        if (!validUnit(v, FPercent))
            return false;
        double percentValue = m_parsedCalculation ? m_parsedCalculation.release()->doubleValue() : v->fValue;
        colorArray[i] = std::max(0.0, std::min(100.0, percentValue)) / 100.0; // needs to be value between 0 and 1.0
    }
    if (parseAlpha) {
        args->next();
        if (!consumeComma(args))
            return false;
        v = args->current();
        if (!validUnit(v, FNumber))
            return false;
        colorArray[3] = std::max(0.0, std::min(1.0, v->fValue));
    }
    return true;
}

bool CSSPropertyParser::parseColorFromValue(const CSSParserValue* value, RGBA32& result, bool acceptQuirkyColors)
{
    if (acceptQuirkyColors && value->unit() == CSSPrimitiveValue::UnitType::Number
        && value->fValue >= 0. && value->fValue < 1000000. && value->isInt) {
        String str = String::format("%06d", static_cast<int>(value->fValue));
        return Color::parseHexColor(str, result);
    }
    if (acceptQuirkyColors && value->m_unit == CSSParserValue::DimensionList) {
        CSSParserValue* numberToken = value->valueList->valueAt(0);
        CSSParserValue* unitToken = value->valueList->valueAt(1);
        ASSERT(numberToken->unit() == CSSPrimitiveValue::UnitType::Number);
        ASSERT(unitToken->m_unit == CSSParserValue::Identifier);
        if (!numberToken->isInt || numberToken->fValue < 0)
            return false;
        String color = String::number(numberToken->fValue) + String(unitToken->string);
        if (color.length() > 6)
            return false;
        while (color.length() < 6)
            color = "0" + color;
        return Color::parseHexColor(color, result);
    }
    if (value->m_unit == CSSParserValue::Identifier) {
        Color color;
        if (!color.setNamedColor(value->string))
            return acceptQuirkyColors && Color::parseHexColor(value->string, result);
        result = color.rgb();
        return true;
    }
    if (value->m_unit == CSSParserValue::HexColor) {
        if (value->string.is8Bit())
            return Color::parseHexColor(value->string.characters8(), value->string.length(), result);
        return Color::parseHexColor(value->string.characters16(), value->string.length(), result);
    }

    if (value->m_unit == CSSParserValue::Function
        && value->function->args != 0
        && value->function->args->size() == 5 /* rgb + two commas */
        && value->function->id == CSSValueRgb) {
        int colorValues[3];
        if (!parseColorParameters(value, colorValues, false))
            return false;
        result = makeRGB(colorValues[0], colorValues[1], colorValues[2]);
    } else {
        if (value->m_unit == CSSParserValue::Function
            && value->function->args != 0
            && value->function->args->size() == 7 /* rgba + three commas */
            && value->function->id == CSSValueRgba) {
            int colorValues[4];
            if (!parseColorParameters(value, colorValues, true))
                return false;
            result = makeRGBA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else if (value->m_unit == CSSParserValue::Function
            && value->function->args != 0
            && value->function->args->size() == 5 /* hsl + two commas */
            && value->function->id == CSSValueHsl) {
            double colorValues[3];
            if (!parseHSLParameters(value, colorValues, false))
                return false;
            result = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], 1.0);
        } else if (value->m_unit == CSSParserValue::Function
            && value->function->args != 0
            && value->function->args->size() == 7 /* hsla + three commas */
            && value->function->id == CSSValueHsla) {
            double colorValues[4];
            if (!parseHSLParameters(value, colorValues, true))
                return false;
            result = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else {
            return false;
        }
    }

    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseReflect()
{
    // box-reflect: <direction> <offset> <mask>

    // Direction comes first.
    CSSParserValue* val = m_valueList->current();
    RefPtrWillBeRawPtr<CSSPrimitiveValue> direction = nullptr;
    switch (val->id) {
    case CSSValueAbove:
    case CSSValueBelow:
    case CSSValueLeft:
    case CSSValueRight:
        direction = cssValuePool().createIdentifierValue(val->id);
        break;
    default:
        return nullptr;
    }

    // The offset comes next.
    val = m_valueList->next();
    RefPtrWillBeRawPtr<CSSPrimitiveValue> offset = nullptr;
    if (!val)
        offset = cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Pixels);
    else {
        if (!validUnit(val, FLength | FPercent))
            return nullptr;
        offset = createPrimitiveNumericValue(val);
    }

    // Now for the mask.
    RefPtrWillBeRawPtr<CSSValue> mask = nullptr;
    val = m_valueList->next();
    if (val) {
        mask = parseBorderImage(CSSPropertyWebkitBoxReflect);
        if (!mask)
            return nullptr;
    }

    return CSSReflectValue::create(direction.release(), offset.release(), mask.release());
}

class BorderImageParseContext {
    STACK_ALLOCATED();
public:
    BorderImageParseContext()
    : m_canAdvance(false)
    , m_allowCommit(true)
    , m_allowImage(true)
    , m_allowImageSlice(true)
    , m_allowRepeat(true)
    , m_allowForwardSlashOperator(false)
    , m_allowWidth(false)
    , m_requireOutset(false)
    {}

    bool canAdvance() const { return m_canAdvance; }
    void setCanAdvance(bool canAdvance) { m_canAdvance = canAdvance; }

    bool allowCommit() const { return m_allowCommit; }
    bool allowImage() const { return m_allowImage; }
    bool allowImageSlice() const { return m_allowImageSlice; }
    bool allowRepeat() const { return m_allowRepeat; }
    bool allowForwardSlashOperator() const { return m_allowForwardSlashOperator; }

    bool allowWidth() const { return m_allowWidth; }
    bool requireOutset() const { return m_requireOutset; }

    void commitImage(PassRefPtrWillBeRawPtr<CSSValue> image)
    {
        m_image = image;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowImage = false;
        m_allowForwardSlashOperator = false;
        m_allowWidth = false;
        m_requireOutset = false;
        m_allowImageSlice = !m_imageSlice;
        m_allowRepeat = !m_repeat;
    }
    void commitImageSlice(PassRefPtrWillBeRawPtr<CSSBorderImageSliceValue> slice)
    {
        m_imageSlice = slice;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowForwardSlashOperator = true;
        m_allowImageSlice = false;
        m_allowWidth = false;
        m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitForwardSlashOperator()
    {
        m_canAdvance = true;
        m_allowCommit = false;
        m_allowImage = false;
        m_allowImageSlice = false;
        m_allowRepeat = false;
        if (!m_borderWidth && !m_allowWidth) {
            m_allowForwardSlashOperator = true;
            m_allowWidth = true;
            m_requireOutset = false;
        } else {
            m_allowForwardSlashOperator = false;
            m_requireOutset = true;
            m_allowWidth = false;
        }
    }
    void commitBorderWidth(PassRefPtrWillBeRawPtr<CSSQuadValue> width)
    {
        m_borderWidth = width;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowForwardSlashOperator = true;
        m_allowImageSlice = false;
        m_allowWidth = false;
        m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitBorderOutset(PassRefPtrWillBeRawPtr<CSSQuadValue> outset)
    {
        m_outset = outset;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowImageSlice = false;
        m_allowForwardSlashOperator = false;
        m_allowWidth = false;
        m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitRepeat(PassRefPtrWillBeRawPtr<CSSValue> repeat)
    {
        m_repeat = repeat;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowRepeat = false;
        m_allowForwardSlashOperator = false;
        m_allowWidth = false;
        m_requireOutset = false;
        m_allowImageSlice = !m_imageSlice;
        m_allowImage = !m_image;
    }

    PassRefPtrWillBeRawPtr<CSSValue> commitCSSValue()
    {
        return createBorderImageValue(m_image, m_imageSlice.get(), m_borderWidth.get(), m_outset.get(), m_repeat.get());
    }

    bool m_canAdvance;

    bool m_allowCommit;
    bool m_allowImage;
    bool m_allowImageSlice;
    bool m_allowRepeat;
    bool m_allowForwardSlashOperator;

    bool m_allowWidth;
    bool m_requireOutset;

    RefPtrWillBeMember<CSSValue> m_image;
    RefPtrWillBeMember<CSSBorderImageSliceValue> m_imageSlice;
    RefPtrWillBeMember<CSSQuadValue> m_borderWidth;
    RefPtrWillBeMember<CSSQuadValue> m_outset;

    RefPtrWillBeMember<CSSValue> m_repeat;
};

bool CSSPropertyParser::buildBorderImageParseContext(CSSPropertyID propId, BorderImageParseContext& context)
{
    CSSPropertyParser::ShorthandScope scope(this, propId);
    while (CSSParserValue* val = m_valueList->current()) {
        context.setCanAdvance(false);

        if (!context.canAdvance() && context.allowForwardSlashOperator() && isForwardSlashOperator(val))
            context.commitForwardSlashOperator();

        if (!context.canAdvance() && context.allowImage()) {
            if (val->m_unit == CSSParserValue::URI) {
                context.commitImage(createCSSImageValueWithReferrer(val->string, m_context));
            } else if (val->m_unit == CSSParserValue::Function) {
                if (CSSPropertyParser::isGeneratedImage(val->function->id)) {
                    RefPtrWillBeRawPtr<CSSValue> value = nullptr;
                    if (parseGeneratedImage(m_valueList, value))
                        context.commitImage(value.release());
                    else
                        return false;
                } else if (val->function->id == CSSValueWebkitImageSet) {
                    RefPtrWillBeRawPtr<CSSValue> value = parseImageSet(m_valueList);
                    if (value)
                        context.commitImage(value.release());
                    else
                        return false;
                }
            } else if (val->id == CSSValueNone)
                context.commitImage(cssValuePool().createIdentifierValue(CSSValueNone));
        }

        if (!context.canAdvance() && context.allowImageSlice()) {
            RefPtrWillBeRawPtr<CSSBorderImageSliceValue> imageSlice = nullptr;
            if (parseBorderImageSlice(propId, imageSlice))
                context.commitImageSlice(imageSlice.release());
        }

        if (!context.canAdvance() && context.allowRepeat()) {
            RefPtrWillBeRawPtr<CSSValue> repeat = nullptr;
            if (parseBorderImageRepeat(repeat))
                context.commitRepeat(repeat.release());
        }

        if (!context.canAdvance() && context.allowWidth()) {
            RefPtrWillBeRawPtr<CSSQuadValue> borderWidth = nullptr;
            if (parseBorderImageWidth(borderWidth))
                context.commitBorderWidth(borderWidth.release());
        }

        if (!context.canAdvance() && context.requireOutset()) {
            RefPtrWillBeRawPtr<CSSQuadValue> borderOutset = nullptr;
            if (parseBorderImageOutset(borderOutset))
                context.commitBorderOutset(borderOutset.release());
        }

        if (!context.canAdvance())
            return false;

        m_valueList->next();
    }

    return context.allowCommit();
}

void CSSPropertyParser::commitBorderImageProperty(CSSPropertyID propId, PassRefPtrWillBeRawPtr<CSSValue> value, bool important)
{
    if (value)
        addProperty(propId, value, important);
    else
        addProperty(propId, cssValuePool().createImplicitInitialValue(), important, true);
}

bool CSSPropertyParser::parseBorderImageShorthand(CSSPropertyID propId, bool important)
{
    BorderImageParseContext context;
    if (buildBorderImageParseContext(propId, context)) {
        switch (propId) {
        case CSSPropertyWebkitMaskBoxImage:
            commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageSource, context.m_image, important);
            commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageSlice, context.m_imageSlice.get(), important);
            commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageWidth, context.m_borderWidth.get(), important);
            commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageOutset, context.m_outset.get(), important);
            commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageRepeat, context.m_repeat.get(), important);
            return true;
        case CSSPropertyBorderImage:
            commitBorderImageProperty(CSSPropertyBorderImageSource, context.m_image, important);
            commitBorderImageProperty(CSSPropertyBorderImageSlice, context.m_imageSlice.get(), important);
            commitBorderImageProperty(CSSPropertyBorderImageWidth, context.m_borderWidth.get(), important);
            commitBorderImageProperty(CSSPropertyBorderImageOutset, context.m_outset.get(), important);
            commitBorderImageProperty(CSSPropertyBorderImageRepeat, context.m_repeat, important);
            return true;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseBorderImage(CSSPropertyID propId)
{
    BorderImageParseContext context;
    if (buildBorderImageParseContext(propId, context)) {
        return context.commitCSSValue();
    }
    return nullptr;
}

static bool isBorderImageRepeatKeyword(int id)
{
    return id == CSSValueStretch || id == CSSValueRepeat || id == CSSValueSpace || id == CSSValueRound;
}

bool CSSPropertyParser::parseBorderImageRepeat(RefPtrWillBeRawPtr<CSSValue>& result)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstValue = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondValue = nullptr;
    CSSParserValue* val = m_valueList->current();
    if (!val)
        return false;
    if (isBorderImageRepeatKeyword(val->id))
        firstValue = cssValuePool().createIdentifierValue(val->id);
    else
        return false;

    val = m_valueList->next();
    if (val) {
        if (isBorderImageRepeatKeyword(val->id))
            secondValue = cssValuePool().createIdentifierValue(val->id);
        else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            // We need to rewind the value list, so that when its advanced we'll
            // end up back at this value.
            m_valueList->previous();
            secondValue = firstValue;
        }
    } else
        secondValue = firstValue;

    result = CSSValuePair::create(firstValue, secondValue, CSSValuePair::DropIdenticalValues);
    return true;
}

class BorderImageSliceParseContext {
    STACK_ALLOCATED();
public:
    BorderImageSliceParseContext()
    : m_allowNumber(true)
    , m_allowFill(true)
    , m_allowFinalCommit(false)
    , m_fill(false)
    { }

    bool allowNumber() const { return m_allowNumber; }
    bool allowFill() const { return m_allowFill; }
    bool allowFinalCommit() const { return m_allowFinalCommit; }
    CSSPrimitiveValue* top() const { return m_top.get(); }

    void commitNumber(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val)
    {
        if (!m_top)
            m_top = val;
        else if (!m_right)
            m_right = val;
        else if (!m_bottom)
            m_bottom = val;
        else {
            ASSERT(!m_left);
            m_left = val;
        }

        m_allowNumber = !m_left;
        m_allowFinalCommit = true;
    }

    void commitFill() { m_fill = true; m_allowFill = false; m_allowNumber = !m_top; }

    PassRefPtrWillBeRawPtr<CSSBorderImageSliceValue> commitBorderImageSlice()
    {
        // We need to clone and repeat values for any omissions.
        ASSERT(m_top);
        if (!m_right) {
            m_right = m_top;
            m_bottom = m_top;
            m_left = m_top;
        }
        if (!m_bottom) {
            m_bottom = m_top;
            m_left = m_right;
        }
        if (!m_left)
            m_left = m_right;

        return CSSBorderImageSliceValue::create(CSSQuadValue::create(m_top.release(), m_right.release(), m_bottom.release(), m_left.release(), CSSQuadValue::SerializeAsQuad), m_fill);
    }

private:
    bool m_allowNumber;
    bool m_allowFill;
    bool m_allowFinalCommit;

    RefPtrWillBeMember<CSSPrimitiveValue> m_top;
    RefPtrWillBeMember<CSSPrimitiveValue> m_right;
    RefPtrWillBeMember<CSSPrimitiveValue> m_bottom;
    RefPtrWillBeMember<CSSPrimitiveValue> m_left;

    bool m_fill;
};

bool CSSPropertyParser::parseBorderImageSlice(CSSPropertyID propId, RefPtrWillBeRawPtr<CSSBorderImageSliceValue>& result)
{
    BorderImageSliceParseContext context;
    for (CSSParserValue* val = m_valueList->current(); val; val = m_valueList->next()) {
        // FIXME calc() http://webkit.org/b/16662 : calc is parsed but values are not created yet.
        if (context.allowNumber() && !isCalculation(val) && validUnit(val, FNumber | FNonNeg | FPercent)) {
            context.commitNumber(createPrimitiveNumericValue(val));
        } else if (context.allowFill() && val->id == CSSValueFill) {
            context.commitFill();
        } else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            if (context.allowFinalCommit()) {
                // We're going to successfully parse, but we don't want to consume this token.
                m_valueList->previous();
            }
            break;
        }
    }

    if (context.allowFinalCommit()) {
        // FIXME: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect have to do a fill by default.
        // FIXME: What do we do with -webkit-box-reflect and -webkit-mask-box-image? Probably just have to leave them filling...
        if (propId == CSSPropertyWebkitBorderImage || propId == CSSPropertyWebkitMaskBoxImage || propId == CSSPropertyWebkitBoxReflect)
            context.commitFill();

        // Need to fully commit as a single value.
        result = context.commitBorderImageSlice();
        return true;
    }

    return false;
}

class BorderImageQuadParseContext {
    STACK_ALLOCATED();
public:
    BorderImageQuadParseContext()
    : m_allowNumber(true)
    , m_allowFinalCommit(false)
    { }

    bool allowNumber() const { return m_allowNumber; }
    bool allowFinalCommit() const { return m_allowFinalCommit; }
    CSSPrimitiveValue* top() const { return m_top.get(); }

    void commitNumber(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val)
    {
        if (!m_top)
            m_top = val;
        else if (!m_right)
            m_right = val;
        else if (!m_bottom)
            m_bottom = val;
        else {
            ASSERT(!m_left);
            m_left = val;
        }

        m_allowNumber = !m_left;
        m_allowFinalCommit = true;
    }

    void setTop(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_top = val; }

    PassRefPtrWillBeRawPtr<CSSQuadValue> commitBorderImageQuad()
    {
        // We need to clone and repeat values for any omissions.
        ASSERT(m_top);
        if (!m_right) {
            m_right = m_top;
            m_bottom = m_top;
            m_left = m_top;
        }
        if (!m_bottom) {
            m_bottom = m_top;
            m_left = m_right;
        }
        if (!m_left)
            m_left = m_right;

        return CSSQuadValue::create(m_top.release(), m_right.release(), m_bottom.release(), m_left.release(), CSSQuadValue::SerializeAsQuad);
    }

private:
    bool m_allowNumber;
    bool m_allowFinalCommit;

    RefPtrWillBeMember<CSSPrimitiveValue> m_top;
    RefPtrWillBeMember<CSSPrimitiveValue> m_right;
    RefPtrWillBeMember<CSSPrimitiveValue> m_bottom;
    RefPtrWillBeMember<CSSPrimitiveValue> m_left;
};

bool CSSPropertyParser::parseBorderImageQuad(Units validUnits, RefPtrWillBeRawPtr<CSSQuadValue>& result)
{
    BorderImageQuadParseContext context;
    for (CSSParserValue* val = m_valueList->current(); val; val = m_valueList->next()) {
        if (context.allowNumber() && (validUnit(val, validUnits, HTMLStandardMode) || val->id == CSSValueAuto)) {
            if (val->id == CSSValueAuto)
                context.commitNumber(cssValuePool().createIdentifierValue(val->id));
            else
                context.commitNumber(createPrimitiveNumericValue(val));
        } else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            if (context.allowFinalCommit())
                m_valueList->previous(); // The shorthand loop will advance back to this point.
            break;
        }
    }

    if (context.allowFinalCommit()) {
        // Need to fully commit as a single value.
        result = context.commitBorderImageQuad();
        return true;
    }
    return false;
}

bool CSSPropertyParser::parseBorderImageWidth(RefPtrWillBeRawPtr<CSSQuadValue>& result)
{
    return parseBorderImageQuad(FLength | FNumber | FNonNeg | FPercent, result);
}

bool CSSPropertyParser::parseBorderImageOutset(RefPtrWillBeRawPtr<CSSQuadValue>& result)
{
    return parseBorderImageQuad(FLength | FNumber | FNonNeg, result);
}

bool CSSPropertyParser::parseRadii(RefPtrWillBeRawPtr<CSSPrimitiveValue> radii[4], RefPtrWillBeRawPtr<CSSPrimitiveValue> radii2[4], CSSParserValueList* args, CSSPropertyID unresolvedProperty)
{
#if ENABLE(OILPAN)
    // Unconditionally zero initialize the arrays of raw pointers.
    memset(radii, 0, 4 * sizeof(radii[0]));
    memset(radii2, 0, 4 * sizeof(radii2[0]));
#endif
    CSSParserValue* value = args->current();
    int i;
    for (i = 0; i < 4 && value && value->m_unit != CSSParserValue::Operator;++i, value = args->next()) {
        if (!validUnit(value, FLength | FPercent | FNonNeg))
            return false;

        radii[i] = createPrimitiveNumericValue(value);
    }

    if (!i || (value && value->m_unit == CSSParserValue::Operator && value->iValue != '/'))
        return false;
    // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to border-radius: l1 / l2;
    if (!value && i == 2 && unresolvedProperty == CSSPropertyAliasWebkitBorderRadius) {
        radii2[0] = radii[1];
        radii[1] = nullptr;
        completeBorderRadii(radii);
        completeBorderRadii(radii2);
        return true;
    }
    completeBorderRadii(radii);
    if (value) {
        value = args->next();
        for (i = 0; i < 4 && value && validUnit(value, FLength | FPercent | FNonNeg); ++i, value = args->next())
            radii2[i] = createPrimitiveNumericValue(value);
        if (!i || value)
            return false;
        completeBorderRadii(radii2);
    } else {
        for (unsigned i = 0; i < 4; ++i)
            radii2[i] = radii[i];
    }

    return true;
}

// This should go away once we drop support for -webkit-gradient
static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseDeprecatedGradientPoint(CSSParserValue* a, bool horizontal)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> result = nullptr;
    if (a->m_unit == CSSParserValue::Identifier) {
        if ((a->id == CSSValueLeft && horizontal)
            || (a->id == CSSValueTop && !horizontal))
            result = cssValuePool().createValue(0., CSSPrimitiveValue::UnitType::Percentage);
        else if ((a->id == CSSValueRight && horizontal)
            || (a->id == CSSValueBottom && !horizontal))
            result = cssValuePool().createValue(100., CSSPrimitiveValue::UnitType::Percentage);
        else if (a->id == CSSValueCenter)
            result = cssValuePool().createValue(50., CSSPrimitiveValue::UnitType::Percentage);
    } else if (a->unit() == CSSPrimitiveValue::UnitType::Number || a->unit() == CSSPrimitiveValue::UnitType::Percentage) {
        result = cssValuePool().createValue(a->fValue, a->unit());
    }
    return result;
}

// Used to parse colors for -webkit-gradient(...).
PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseDeprecatedGradientStopColor(const CSSParserValue* value)
{
    // Disallow currentcolor.
    if (value->id == CSSValueCurrentcolor)
        return nullptr;
    return parseColor(value);
}

bool CSSPropertyParser::parseDeprecatedGradientColorStop(CSSParserValue* a, CSSGradientColorStop& stop)
{
    if (a->m_unit != CSSParserValue::Function)
        return false;

    if (a->function->id != CSSValueFrom
        && a->function->id != CSSValueTo
        && a->function->id != CSSValueColorStop)
        return false;

    CSSParserValueList* args = a->function->args.get();
    if (!args)
        return false;

    if (a->function->id == CSSValueFrom || a->function->id == CSSValueTo) {
        // The "from" and "to" stops expect 1 argument.
        if (args->size() != 1)
            return false;

        if (a->function->id == CSSValueFrom)
            stop.m_position = cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Number);
        else
            stop.m_position = cssValuePool().createValue(1, CSSPrimitiveValue::UnitType::Number);

        stop.m_color = parseDeprecatedGradientStopColor(args->current());
        if (!stop.m_color)
            return false;
    }

    // The "color-stop" function expects 3 arguments.
    if (a->function->id == CSSValueColorStop) {
        if (args->size() != 3)
            return false;

        CSSParserValue* stopArg = args->current();
        if (stopArg->unit() == CSSPrimitiveValue::UnitType::Percentage)
            stop.m_position = cssValuePool().createValue(stopArg->fValue / 100, CSSPrimitiveValue::UnitType::Number);
        else if (stopArg->unit() == CSSPrimitiveValue::UnitType::Number)
            stop.m_position = cssValuePool().createValue(stopArg->fValue, CSSPrimitiveValue::UnitType::Number);
        else
            return false;

        args->next();
        if (!consumeComma(args))
            return false;

        stop.m_color = parseDeprecatedGradientStopColor(args->current());
        if (!stop.m_color)
            return false;
    }

    return true;
}

bool CSSPropertyParser::parseDeprecatedGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient)
{
    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || args->size() == 0)
        return false;

    // The first argument is the gradient type.  It is an identifier.
    CSSGradientType gradientType;
    CSSParserValue* a = args->current();
    if (!a || a->m_unit != CSSParserValue::Identifier)
        return false;
    if (a->id == CSSValueLinear)
        gradientType = CSSDeprecatedLinearGradient;
    else if (a->id == CSSValueRadial)
        gradientType = CSSDeprecatedRadialGradient;
    else
        return false;

    RefPtrWillBeRawPtr<CSSGradientValue> result = nullptr;
    switch (gradientType) {
    case CSSDeprecatedLinearGradient:
        result = CSSLinearGradientValue::create(NonRepeating, gradientType);
        break;
    case CSSDeprecatedRadialGradient:
        result = CSSRadialGradientValue::create(NonRepeating, gradientType);
        break;
    default:
        // The rest of the gradient types shouldn't appear here.
        ASSERT_NOT_REACHED();
    }
    args->next();

    if (!consumeComma(args))
        return false;

    // Next comes the starting point for the gradient as an x y pair.  There is no
    // comma between the x and the y values.
    // First X.  It can be left, right, number or percent.
    a = args->current();
    if (!a)
        return false;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> point = parseDeprecatedGradientPoint(a, true);
    if (!point)
        return false;
    result->setFirstX(point.release());

    // First Y.  It can be top, bottom, number or percent.
    a = args->next();
    if (!a)
        return false;
    point = parseDeprecatedGradientPoint(a, false);
    if (!point)
        return false;
    result->setFirstY(point.release());

    // Comma after the first point.
    args->next();
    if (!consumeComma(args))
        return false;

    // For radial gradients only, we now expect a numeric radius.
    if (gradientType == CSSDeprecatedRadialGradient) {
        a = args->current();
        if (!a || a->unit() != CSSPrimitiveValue::UnitType::Number)
            return false;
        toCSSRadialGradientValue(result.get())->setFirstRadius(createPrimitiveNumericValue(a));

        // Comma after the first radius.
        args->next();
        if (!consumeComma(args))
            return false;
    }

    // Next is the ending point for the gradient as an x, y pair.
    // Second X.  It can be left, right, number or percent.
    a = args->current();
    if (!a)
        return false;
    point = parseDeprecatedGradientPoint(a, true);
    if (!point)
        return false;
    result->setSecondX(point.release());

    // Second Y.  It can be top, bottom, number or percent.
    a = args->next();
    if (!a)
        return false;
    point = parseDeprecatedGradientPoint(a, false);
    if (!point)
        return false;
    result->setSecondY(point.release());
    args->next();

    // For radial gradients only, we now expect the second radius.
    if (gradientType == CSSDeprecatedRadialGradient) {
        // Comma after the second point.
        if (!consumeComma(args))
            return false;

        a = args->current();
        if (!a || a->unit() != CSSPrimitiveValue::UnitType::Number)
            return false;
        toCSSRadialGradientValue(result.get())->setSecondRadius(createPrimitiveNumericValue(a));
        args->next();
    }

    // We now will accept any number of stops (0 or more).
    a = args->current();
    while (a) {
        // Look for the comma before the next stop.
        if (!consumeComma(args))
            return false;

        // Now examine the stop itself.
        a = args->current();
        if (!a)
            return false;

        // The function name needs to be one of "from", "to", or "color-stop."
        CSSGradientColorStop stop;
        if (!parseDeprecatedGradientColorStop(a, stop))
            return false;
        result->addStop(stop);

        // Advance
        a = args->next();
    }

    gradient = result.release();
    return true;
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueFromSideKeyword(CSSParserValue* a, bool& isHorizontal)
{
    if (a->m_unit != CSSParserValue::Identifier)
        return nullptr;

    switch (a->id) {
        case CSSValueLeft:
        case CSSValueRight:
            isHorizontal = true;
            break;
        case CSSValueTop:
        case CSSValueBottom:
            isHorizontal = false;
            break;
        default:
            return nullptr;
    }
    return cssValuePool().createIdentifierValue(a->id);
}

bool CSSPropertyParser::parseDeprecatedLinearGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtrWillBeRawPtr<CSSLinearGradientValue> result = CSSLinearGradientValue::create(repeating, CSSPrefixedLinearGradient);

    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* a = args->current();
    if (!a)
        return false;

    bool expectComma = false;
    // Look for angle.
    if (validUnit(a, FAngle, HTMLStandardMode)) {
        result->setAngle(createPrimitiveNumericValue(a));

        args->next();
        expectComma = true;
    } else {
        // Look one or two optional keywords that indicate a side or corner.
        RefPtrWillBeRawPtr<CSSPrimitiveValue> startX = nullptr;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> startY = nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> location = nullptr;
        bool isHorizontal = false;
        if ((location = valueFromSideKeyword(a, isHorizontal))) {
            if (isHorizontal)
                startX = location;
            else
                startY = location;

            a = args->next();
            if (a) {
                if ((location = valueFromSideKeyword(a, isHorizontal))) {
                    if (isHorizontal) {
                        if (startX)
                            return false;
                        startX = location;
                    } else {
                        if (startY)
                            return false;
                        startY = location;
                    }

                    args->next();
                }
            }

            expectComma = true;
        }

        if (!startX && !startY)
            startY = cssValuePool().createIdentifierValue(CSSValueTop);

        result->setFirstX(startX.release());
        result->setFirstY(startY.release());
    }

    if (!parseGradientColorStops(args, result.get(), expectComma))
        return false;

    if (!result->stopCount())
        return false;

    gradient = result.release();
    return true;
}

bool CSSPropertyParser::parseDeprecatedRadialGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtrWillBeRawPtr<CSSRadialGradientValue> result = CSSRadialGradientValue::create(repeating, CSSPrefixedRadialGradient);

    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* a = args->current();
    if (!a)
        return false;

    bool expectComma = false;

    // Optional background-position
    RefPtrWillBeRawPtr<CSSValue> centerX = nullptr;
    RefPtrWillBeRawPtr<CSSValue> centerY = nullptr;
    // parse2ValuesFillPosition advances the args next pointer.
    parse2ValuesFillPosition(args, centerX, centerY);

    if ((centerX || centerY) && !consumeComma(args))
        return false;

    a = args->current();
    if (!a)
        return false;

    result->setFirstX(toCSSPrimitiveValue(centerX.get()));
    result->setSecondX(toCSSPrimitiveValue(centerX.get()));
    // CSS3 radial gradients always share the same start and end point.
    result->setFirstY(toCSSPrimitiveValue(centerY.get()));
    result->setSecondY(toCSSPrimitiveValue(centerY.get()));

    RefPtrWillBeRawPtr<CSSPrimitiveValue> shapeValue = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> sizeValue = nullptr;

    // Optional shape and/or size in any order.
    for (int i = 0; i < 2; ++i) {
        if (a->m_unit != CSSParserValue::Identifier)
            break;

        bool foundValue = false;
        switch (a->id) {
        case CSSValueCircle:
        case CSSValueEllipse:
            shapeValue = cssValuePool().createIdentifierValue(a->id);
            foundValue = true;
            break;
        case CSSValueClosestSide:
        case CSSValueClosestCorner:
        case CSSValueFarthestSide:
        case CSSValueFarthestCorner:
        case CSSValueContain:
        case CSSValueCover:
            sizeValue = cssValuePool().createIdentifierValue(a->id);
            foundValue = true;
            break;
        default:
            break;
        }

        if (foundValue) {
            a = args->next();
            if (!a)
                return false;

            expectComma = true;
        }
    }

    result->setShape(shapeValue);
    result->setSizingBehavior(sizeValue);

    // Or, two lengths or percentages
    RefPtrWillBeRawPtr<CSSPrimitiveValue> horizontalSize = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> verticalSize = nullptr;

    if (!shapeValue && !sizeValue) {
        if (validUnit(a, FLength | FPercent)) {
            horizontalSize = createPrimitiveNumericValue(a);
            a = args->next();
            if (!a)
                return false;

            expectComma = true;
        }

        if (validUnit(a, FLength | FPercent)) {
            verticalSize = createPrimitiveNumericValue(a);

            a = args->next();
            if (!a)
                return false;
            expectComma = true;
        }
    }

    // Must have neither or both.
    if (!horizontalSize != !verticalSize)
        return false;

    result->setEndHorizontalSize(horizontalSize);
    result->setEndVerticalSize(verticalSize);

    if (!parseGradientColorStops(args, result.get(), expectComma))
        return false;

    gradient = result.release();
    return true;
}

bool CSSPropertyParser::parseLinearGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtrWillBeRawPtr<CSSLinearGradientValue> result = CSSLinearGradientValue::create(repeating, CSSLinearGradient);

    CSSParserFunction* function = valueList->current()->function;
    CSSParserValueList* args = function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* a = args->current();
    if (!a)
        return false;

    bool expectComma = false;
    // Look for angle.
    if (validUnit(a, FAngle, HTMLStandardMode)) {
        result->setAngle(createPrimitiveNumericValue(a));

        args->next();
        expectComma = true;
    } else if (a->m_unit == CSSParserValue::Identifier && a->id == CSSValueTo) {
        // to [ [left | right] || [top | bottom] ]
        a = args->next();
        if (!a)
            return false;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> endX = nullptr;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> endY = nullptr;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> location = nullptr;
        bool isHorizontal = false;

        location = valueFromSideKeyword(a, isHorizontal);
        if (!location)
            return false;

        if (isHorizontal)
            endX = location;
        else
            endY = location;

        a = args->next();
        if (!a)
            return false;

        location = valueFromSideKeyword(a, isHorizontal);
        if (location) {
            if (isHorizontal) {
                if (endX)
                    return false;
                endX = location;
            } else {
                if (endY)
                    return false;
                endY = location;
            }

            args->next();
        }

        expectComma = true;
        result->setFirstX(endX.release());
        result->setFirstY(endY.release());
    }

    if (!parseGradientColorStops(args, result.get(), expectComma))
        return false;

    if (!result->stopCount())
        return false;

    gradient = result.release();
    return true;
}

bool CSSPropertyParser::parseRadialGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtrWillBeRawPtr<CSSRadialGradientValue> result = CSSRadialGradientValue::create(repeating, CSSRadialGradient);

    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* a = args->current();
    if (!a)
        return false;

    bool expectComma = false;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> shapeValue = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> sizeValue = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> horizontalSize = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> verticalSize = nullptr;

    // First part of grammar, the size/shape clause:
    // [ circle || <length> ] |
    // [ ellipse || [ <length> | <percentage> ]{2} ] |
    // [ [ circle | ellipse] || <size-keyword> ]
    for (int i = 0; i < 3; ++i) {
        if (a->m_unit == CSSParserValue::Identifier) {
            bool badIdent = false;
            switch (a->id) {
            case CSSValueCircle:
            case CSSValueEllipse:
                if (shapeValue)
                    return false;
                shapeValue = cssValuePool().createIdentifierValue(a->id);
                break;
            case CSSValueClosestSide:
            case CSSValueClosestCorner:
            case CSSValueFarthestSide:
            case CSSValueFarthestCorner:
                if (sizeValue || horizontalSize)
                    return false;
                sizeValue = cssValuePool().createIdentifierValue(a->id);
                break;
            default:
                badIdent = true;
            }

            if (badIdent)
                break;

            a = args->next();
            if (!a)
                return false;
        } else if (validUnit(a, FLength | FPercent)) {

            if (sizeValue || horizontalSize)
                return false;
            horizontalSize = createPrimitiveNumericValue(a);

            a = args->next();
            if (!a)
                return false;

            if (validUnit(a, FLength | FPercent)) {
                verticalSize = createPrimitiveNumericValue(a);
                ++i;
                a = args->next();
                if (!a)
                    return false;
            }
        } else
            break;
    }

    // You can specify size as a keyword or a length/percentage, not both.
    if (sizeValue && horizontalSize)
        return false;
    // Circles must have 0 or 1 lengths.
    if (shapeValue && shapeValue->getValueID() == CSSValueCircle && verticalSize)
        return false;
    // Ellipses must have 0 or 2 length/percentages.
    if (shapeValue && shapeValue->getValueID() == CSSValueEllipse && horizontalSize && !verticalSize)
        return false;
    // If there's only one size, it must be a length.
    if (!verticalSize && horizontalSize && horizontalSize->isPercentage())
        return false;

    result->setShape(shapeValue);
    result->setSizingBehavior(sizeValue);
    result->setEndHorizontalSize(horizontalSize);
    result->setEndVerticalSize(verticalSize);

    // Second part of grammar, the center-position clause:
    // at <position>
    RefPtrWillBeRawPtr<CSSValue> centerX = nullptr;
    RefPtrWillBeRawPtr<CSSValue> centerY = nullptr;
    if (a->m_unit == CSSParserValue::Identifier && a->id == CSSValueAt) {
        a = args->next();
        if (!a)
            return false;

        parseFillPosition(args, centerX, centerY);
        if (!(centerX && centerY))
            return false;

        a = args->current();
        if (!a)
            return false;
        result->setFirstX(centerX);
        result->setFirstY(centerY);
        // Right now, CSS radial gradients have the same start and end centers.
        result->setSecondX(centerX);
        result->setSecondY(centerY);
    }

    if (shapeValue || sizeValue || horizontalSize || centerX || centerY)
        expectComma = true;

    if (!parseGradientColorStops(args, result.get(), expectComma))
        return false;

    gradient = result.release();
    return true;
}

bool CSSPropertyParser::parseGradientColorStops(CSSParserValueList* valueList, CSSGradientValue* gradient, bool expectComma)
{
    CSSParserValue* a = valueList->current();

    // Now look for color stops.
    // <color-stop-list> = [ <color-stop> , <color-hint>? ]# , <color-stop>
    bool supportsColorHints = gradient->gradientType() == CSSLinearGradient
        || gradient->gradientType() == CSSRadialGradient;

    // The first color stop cannot be a color hint.
    bool previousStopWasColorHint = true;
    while (a) {
        // Look for the comma before the next stop.
        if (expectComma) {
            if (!isComma(a))
                return false;

            a = valueList->next();
            if (!a)
                return false;
        }

        // <color-stop> = <color> [ <percentage> | <length> ]?
        // <color-hint> = <length> | <percentage>
        CSSGradientColorStop stop;
        stop.m_color = parseColor(a);

        // Two hints in a row are not allowed.
        if (!stop.m_color && (!supportsColorHints || previousStopWasColorHint))
            return false;
        previousStopWasColorHint = !stop.m_color;

        if (stop.m_color)
            a = valueList->next();

        if (a) {
            if (validUnit(a, FLength | FPercent)) {
                stop.m_position = createPrimitiveNumericValue(a);
                a = valueList->next();
            }
        }

        if (!stop.m_color && !stop.m_position)
            return false;

        gradient->addStop(stop);
        expectComma = true;
    }

    // The last color stop cannot be a color hint.
    if (previousStopWasColorHint)
        return false;

    // Must have 2 or more stops to be valid.
    return gradient->stopCount() >= 2;
}

bool CSSPropertyParser::parseGeneratedImage(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value)
{
    CSSParserValue* val = valueList->current();

    if (val->m_unit != CSSParserValue::Function)
        return false;

    if (val->function->id == CSSValueWebkitGradient) {
        // FIXME: This should send a deprecation message.
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitGradient);
        return parseDeprecatedGradient(valueList, value);
    }

    if (val->function->id == CSSValueWebkitLinearGradient) {
        // FIXME: This should send a deprecation message.
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitLinearGradient);
        return parseDeprecatedLinearGradient(valueList, value, NonRepeating);
    }

    if (val->function->id == CSSValueLinearGradient)
        return parseLinearGradient(valueList, value, NonRepeating);

    if (val->function->id == CSSValueWebkitRepeatingLinearGradient) {
        // FIXME: This should send a deprecation message.
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitRepeatingLinearGradient);
        return parseDeprecatedLinearGradient(valueList, value, Repeating);
    }

    if (val->function->id == CSSValueRepeatingLinearGradient)
        return parseLinearGradient(valueList, value, Repeating);

    if (val->function->id == CSSValueWebkitRadialGradient) {
        // FIXME: This should send a deprecation message.
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitRadialGradient);
        return parseDeprecatedRadialGradient(valueList, value, NonRepeating);
    }

    if (val->function->id == CSSValueRadialGradient)
        return parseRadialGradient(valueList, value, NonRepeating);

    if (val->function->id == CSSValueWebkitRepeatingRadialGradient) {
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitRepeatingRadialGradient);
        return parseDeprecatedRadialGradient(valueList, value, Repeating);
    }

    if (val->function->id == CSSValueRepeatingRadialGradient)
        return parseRadialGradient(valueList, value, Repeating);

    if (val->function->id == CSSValueWebkitCrossFade)
        return parseCrossfade(valueList, value);

    return false;
}

bool CSSPropertyParser::parseCrossfade(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& crossfade)
{
    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || args->size() != 5)
        return false;
    RefPtrWillBeRawPtr<CSSValue> fromImageValue = nullptr;
    RefPtrWillBeRawPtr<CSSValue> toImageValue = nullptr;

    // The first argument is the "from" image. It is a fill image.
    if (!args->current() || !parseFillImage(args, fromImageValue))
        return false;
    args->next();

    if (!consumeComma(args))
        return false;

    // The second argument is the "to" image. It is a fill image.
    if (!args->current() || !parseFillImage(args, toImageValue))
        return false;
    args->next();

    if (!consumeComma(args))
        return false;

    // The third argument is the crossfade value. It is a percentage or a fractional number.
    RefPtrWillBeRawPtr<CSSPrimitiveValue> percentage = nullptr;
    CSSParserValue* value = args->current();
    if (!value)
        return false;

    if (value->unit() == CSSPrimitiveValue::UnitType::Percentage)
        percentage = cssValuePool().createValue(clampTo<double>(value->fValue / 100, 0, 1), CSSPrimitiveValue::UnitType::Number);
    else if (value->unit() == CSSPrimitiveValue::UnitType::Number)
        percentage = cssValuePool().createValue(clampTo<double>(value->fValue, 0, 1), CSSPrimitiveValue::UnitType::Number);
    else
        return false;

    crossfade = CSSCrossfadeValue::create(fromImageValue, toImageValue, percentage);

    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseImageSet(CSSParserValueList* valueList)
{
    CSSParserValue* function = valueList->current();

    if (function->m_unit != CSSParserValue::Function)
        return nullptr;

    CSSParserValueList* functionArgs = valueList->current()->function->args.get();
    if (!functionArgs || !functionArgs->size() || !functionArgs->current())
        return nullptr;

    RefPtrWillBeRawPtr<CSSImageSetValue> imageSet = CSSImageSetValue::create();

    while (functionArgs->current()) {
        CSSParserValue* arg = functionArgs->current();
        if (arg->m_unit != CSSParserValue::URI)
            return nullptr;

        RefPtrWillBeRawPtr<CSSValue> image = createCSSImageValueWithReferrer(arg->string, m_context);
        imageSet->append(image);

        arg = functionArgs->next();
        if (!arg)
            return nullptr;

        if (arg->m_unit != CSSParserValue::DimensionList)
            return nullptr;
        ASSERT(arg->valueList->valueAt(0)->unit() == CSSPrimitiveValue::UnitType::Number);
        ASSERT(arg->valueList->valueAt(1)->m_unit == CSSParserValue::Identifier);
        if (String(arg->valueList->valueAt(1)->string) != "x")
            return nullptr;
        double imageScaleFactor = arg->valueList->valueAt(0)->fValue;
        if (imageScaleFactor <= 0)
            return nullptr;
        imageSet->append(cssValuePool().createValue(imageScaleFactor, CSSPrimitiveValue::UnitType::Number));
        functionArgs->next();

        // If there are no more arguments, we're done.
        if (!functionArgs->current())
            break;

        // If there are more arguments, they should be after a comma.
        if (!consumeComma(functionArgs))
            return nullptr;
    }

    return imageSet.release();
}

bool CSSPropertyParser::parseCalculation(CSSParserValue* value, ValueRange range)
{
    ASSERT(isCalculation(value));

    CSSParserTokenRange args = value->calcFunction->args;

    ASSERT(!m_parsedCalculation);
    m_parsedCalculation = CSSCalcValue::create(args, range);

    if (!m_parsedCalculation)
        return false;

    return true;
}

} // namespace blink
