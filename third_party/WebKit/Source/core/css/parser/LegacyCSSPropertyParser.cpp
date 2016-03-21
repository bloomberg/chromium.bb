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

static inline bool isForwardSlashOperator(CSSParserValue* value)
{
    ASSERT(value);
    return value->m_unit == CSSParserValue::Operator && value->iValue == '/';
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

    // Note: m_parsedCalculation is used to pass the calc value to validUnit and then cleared at the end of this function.
    // FIXME: This is to avoid having to pass parsedCalc to all validUnit callers.
    ASSERT(!m_parsedCalculation);

    RefPtrWillBeRawPtr<CSSValue> parsedValue = nullptr;

    switch (propId) {
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

    case CSSPropertyGridTemplateAreas:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        parsedValue = parseGridTemplateAreas();
        break;

    // Everything else is handled in CSSPropertyParser.cpp
    default:
        return nullptr;
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
    case CSSPropertyGridTemplate:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridTemplateShorthand(important);

    case CSSPropertyGrid:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridShorthand(important);

    // The remaining shorthands are handled in CSSPropertyParser.cpp
    default:
        return false;
    }
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

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridTemplateColumns(bool important)
{
    if (!(m_valueList->current() && isForwardSlashOperator(m_valueList->current()) && m_valueList->next()))
        return nullptr;
    if (RefPtrWillBeRawPtr<CSSValue> columnsValue = parseGridTrackList()) {
        if (m_valueList->current())
            return nullptr;
        return columnsValue;
    }

    return nullptr;
}

bool CSSPropertyParser::parseGridTemplateRowsAndAreasAndColumns(bool important)
{
    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;
    bool trailingIdentWasAdded = false;
    RefPtrWillBeRawPtr<CSSValueList> templateRows = CSSValueList::createSpaceSeparated();

    // At least template-areas strings must be defined.
    if (!m_valueList->current() || isForwardSlashOperator(m_valueList->current()))
        return false;

    while (m_valueList->current() && !isForwardSlashOperator(m_valueList->current())) {
        // Handle leading <custom-ident>*.
        if (!parseGridLineNames(*m_valueList, *templateRows, trailingIdentWasAdded ? toCSSGridLineNamesValue(templateRows->item(templateRows->length() - 1)) : nullptr))
            return false;

        // Handle a template-area's row.
        if (!parseGridTemplateAreasRow(gridAreaMap, rowCount, columnCount))
            return false;
        ++rowCount;

        // Handle template-rows's track-size.
        if (m_valueList->current() && m_valueList->current()->m_unit != CSSParserValue::Operator && m_valueList->current()->m_unit != CSSParserValue::String) {
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

    RefPtrWillBeRawPtr<CSSValue> columnsValue = nullptr;
    if (m_valueList->current()) {
        ASSERT(isForwardSlashOperator(m_valueList->current()));
        columnsValue = parseGridTemplateColumns(important);
        if (!columnsValue)
            return false;
        // The template-columns <track-list> can't be 'none'.
        if (columnsValue->isPrimitiveValue() && toCSSPrimitiveValue(*columnsValue).getValueID() == CSSValueNone)
            return false;
    }

    addProperty(CSSPropertyGridTemplateRows, templateRows.release(), important);
    addProperty(CSSPropertyGridTemplateColumns, columnsValue ? columnsValue.release() : cssValuePool().createIdentifierValue(CSSValueNone), important);

    RefPtrWillBeRawPtr<CSSValue> templateAreas = CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
    addProperty(CSSPropertyGridTemplateAreas, templateAreas.release(), important);

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

    // 2- <grid-template-rows> / <grid-template-columns>
    RefPtrWillBeRawPtr<CSSValue> rowsValue = nullptr;
    if (firstValueIsNone) {
        rowsValue = cssValuePool().createIdentifierValue(CSSValueNone);
    } else {
        rowsValue = parseGridTrackList();
    }

    if (rowsValue) {
        RefPtrWillBeRawPtr<CSSValue> columnsValue = parseGridTemplateColumns(important);
        if (!columnsValue)
            return false;

        addProperty(CSSPropertyGridTemplateRows, rowsValue.release(), important);
        addProperty(CSSPropertyGridTemplateColumns, columnsValue.release(), important);
        addProperty(CSSPropertyGridTemplateAreas, cssValuePool().createIdentifierValue(CSSValueNone), important);
        return true;
    }

    // 3- [<line-names>? <string> <track-size>? <line-names>? ]+ syntax.
    // It requires to rewind parsing due to previous syntax failures.
    m_valueList->setCurrentIndex(0);
    return parseGridTemplateRowsAndAreasAndColumns(important);
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

    // 2- <grid-auto-flow> [ <grid-auto-rows> [ / <grid-auto-columns> ]? ]
    if (!legacyParseAndApplyValue(CSSPropertyGridAutoFlow, important))
        return false;

    RefPtrWillBeRawPtr<CSSValue> autoColumnsValue = nullptr;
    RefPtrWillBeRawPtr<CSSValue> autoRowsValue = nullptr;

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

static bool allTracksAreFixedSized(CSSValueList& valueList)
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
    if (seenAutoRepeat && !allTracksAreFixedSized(*values))
        return nullptr;

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

    RefPtrWillBeRawPtr<CSSValueList> repeatedValues = isAutoRepeat ? CSSGridAutoRepeatValue::create(currentValue->id) : CSSValueList::createSpaceSeparated();
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

    if (isAutoRepeat) {
        list.append(repeatedValues.release());
    } else {
        // We clamp the number of repetitions to a multiple of the repeat() track list's size, while staying below the max
        // grid size.
        repetitions = std::min(repetitions, kGridMaxTracks / numberOfTracks);

        for (size_t i = 0; i < repetitions; ++i) {
            for (size_t j = 0; j < repeatedValues->length(); ++j)
                list.append(repeatedValues->item(j));
        }
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
