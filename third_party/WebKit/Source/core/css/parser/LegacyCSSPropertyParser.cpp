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
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridAutoRepeatValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSValuePool.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/style/GridArea.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void CSSPropertyParser::addProperty(CSSPropertyID propId, CSSValue* value, bool important, bool implicit)
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

    m_parsedProperties->append(CSSProperty(propId, value, important, setFromShorthand, shorthandIndex, implicit));
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

CSSPrimitiveValue* CSSPropertyParser::createPrimitiveNumericValue(CSSParserValue* value)
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

static inline bool isComma(CSSParserValue* value)
{
    ASSERT(value);
    return value->m_unit == CSSParserValue::Operator && value->iValue == ',';
}

static inline bool isForwardSlashOperator(CSSParserValue* value)
{
    ASSERT(value);
    return value->m_unit == CSSParserValue::Operator && value->iValue == '/';
}

void CSSPropertyParser::addExpandedPropertyForValue(CSSPropertyID propId, CSSValue* value, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(propId);
    unsigned shorthandLength = shorthand.length();
    if (!shorthandLength) {
        addProperty(propId, value, important);
        return;
    }

    ShorthandScope scope(this, propId);
    const CSSPropertyID* longhands = shorthand.properties();
    for (unsigned i = 0; i < shorthandLength; ++i)
        addProperty(longhands[i], value, important);
}

bool CSSPropertyParser::legacyParseAndApplyValue(CSSPropertyID propertyID, bool important)
{
    CSSValue* result = legacyParseValue(propertyID);
    if (!result)
        return false;
    addProperty(propertyID, result, important);
    return true;
}

CSSValue* CSSPropertyParser::legacyParseValue(CSSPropertyID unresolvedProperty)
{
    CSSPropertyID propId = resolveCSSPropertyID(unresolvedProperty);

    // Note: m_parsedCalculation is used to pass the calc value to validUnit and then cleared at the end of this function.
    // FIXME: This is to avoid having to pass parsedCalc to all validUnit callers.
    ASSERT(!m_parsedCalculation);

    CSSValue* parsedValue = nullptr;

    switch (propId) {
    case CSSPropertyGridAutoFlow:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseGridAutoFlow(*m_valueList);
        break;

    // Everything else is handled in CSSPropertyParser.cpp
    default:
        return nullptr;
    }

    ASSERT(!m_parsedCalculation);
    if (parsedValue) {
        if (!m_valueList->current() || inShorthand())
            return parsedValue;
    }
    return nullptr;
}

bool CSSPropertyParser::legacyParseShorthand(CSSPropertyID propertyID, bool important)
{
    switch (propertyID) {
    case CSSPropertyGrid:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridShorthand(important);

    // The remaining shorthands are handled in CSSPropertyParser.cpp
    default:
        return false;
    }
}

bool CSSPropertyParser::parseGridShorthand(bool important)
{
    ShorthandScope scope(this, CSSPropertyGrid);
    ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 8);

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(important)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addProperty(CSSPropertyGridAutoFlow, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridAutoColumns, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridAutoRows, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridColumnGap, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridRowGap, cssValuePool().createImplicitInitialValue(), important);
        return true;
    }

    // 2- <grid-auto-flow> [ <grid-auto-rows> [ / <grid-auto-columns> ]? ]
    if (!legacyParseAndApplyValue(CSSPropertyGridAutoFlow, important))
        return false;

    CSSValue* autoColumnsValue = nullptr;
    CSSValue* autoRowsValue = nullptr;

    if (m_valueList->current()) {
        autoRowsValue = parseGridTrackSize(*m_valueList);
        if (!autoRowsValue)
            return false;
        if (m_valueList->current()) {
            if (!isForwardSlashOperator(m_valueList->current()) || !m_valueList->next())
                return false;
            autoColumnsValue = parseGridTrackSize(*m_valueList);
            if (!autoColumnsValue)
                return false;
        }
        if (m_valueList->current())
            return false;
    } else {
        // Other omitted values are set to their initial values.
        autoColumnsValue = cssValuePool().createImplicitInitialValue();
        autoRowsValue = cssValuePool().createImplicitInitialValue();
    }

    // if <grid-auto-columns> value is omitted, it is set to the value specified for grid-auto-rows.
    if (!autoColumnsValue)
        autoColumnsValue = autoRowsValue;

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

bool allTracksAreFixedSized(CSSValueList& valueList)
{
    for (auto value : valueList) {
        if (value->isGridLineNamesValue())
            continue;
        // The auto-repeat value holds a <fixed-size> = <fixed-breadth> | minmax( <fixed-breadth>, <track-breadth> )
        if (value->isGridAutoRepeatValue()) {
            if (!allTracksAreFixedSized(toCSSValueList(*value)))
                return false;
            continue;
        }
        ASSERT(value->isPrimitiveValue() || (value->isFunctionValue() && toCSSFunctionValue(*value).item(0)));
        const CSSPrimitiveValue& primitiveValue = value->isPrimitiveValue()
            ? toCSSPrimitiveValue(*value)
            : toCSSPrimitiveValue(*toCSSFunctionValue(*value).item(0));
        CSSValueID valueID = primitiveValue.getValueID();
        if (valueID == CSSValueMinContent || valueID == CSSValueMaxContent || valueID == CSSValueAuto || primitiveValue.isFlex())
            return false;
    }
    return true;
}


CSSValue* CSSPropertyParser::parseGridTrackSize(CSSParserValueList& inputList, TrackSizeRestriction restriction)
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

        CSSPrimitiveValue* minTrackBreadth = parseGridBreadth(arguments->valueAt(0), restriction);
        if (!minTrackBreadth)
            return nullptr;

        CSSPrimitiveValue* maxTrackBreadth = parseGridBreadth(arguments->valueAt(2));
        if (!maxTrackBreadth)
            return nullptr;

        CSSFunctionValue* result = CSSFunctionValue::create(CSSValueMinmax);
        result->append(minTrackBreadth);
        result->append(maxTrackBreadth);
        return result;
    }

    return parseGridBreadth(currentValue, restriction);
}

CSSPrimitiveValue* CSSPropertyParser::parseGridBreadth(CSSParserValue* currentValue, TrackSizeRestriction restriction)
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

bool parseGridTemplateAreasRow(const String& gridRowNames, NamedGridAreaMap& gridAreaMap, const size_t rowCount, size_t& columnCount)
{
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
            gridAreaMap.add(gridAreaName, GridArea(GridSpan::translatedDefiniteGridSpan(rowCount, rowCount + 1), GridSpan::translatedDefiniteGridSpan(currentCol, lookAheadCol)));
        } else {
            GridArea& gridArea = gridAreaIt->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (rowCount != gridArea.rows.endLine())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentCol != gridArea.columns.startLine())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadCol != gridArea.columns.endLine())
                return false;

            gridArea.rows = GridSpan::translatedDefiniteGridSpan(gridArea.rows.startLine(), gridArea.rows.endLine() + 1);
        }
        currentCol = lookAheadCol - 1;
    }

    return true;
}

CSSValue* CSSPropertyParser::parseGridAutoFlow(CSSParserValueList& list)
{
    // [ row | column ] || dense
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSParserValue* value = list.current();
    if (!value)
        return nullptr;

    CSSValueList* parsedValues = CSSValueList::createSpaceSeparated();

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
