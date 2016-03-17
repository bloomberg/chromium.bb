// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSPropertyParserHelpers.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSStringValue.h"

namespace blink {

namespace CSSPropertyParserHelpers {

bool consumeCommaIncludingWhitespace(CSSParserTokenRange& range)
{
    CSSParserToken value = range.peek();
    if (value.type() != CommaToken)
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

bool consumeSlashIncludingWhitespace(CSSParserTokenRange& range)
{
    CSSParserToken value = range.peek();
    if (value.type() != DelimiterToken || value.delimiter() != '/')
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

CSSParserTokenRange consumeFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == FunctionToken);
    CSSParserTokenRange contents = range.consumeBlock();
    range.consumeWhitespace();
    contents.consumeWhitespace();
    return contents;
}

// TODO(rwlbuis): consider pulling in the parsing logic from CSSCalculationValue.cpp.
class CalcParser {
    STACK_ALLOCATED();

public:
    explicit CalcParser(CSSParserTokenRange& range, ValueRange valueRange = ValueRangeAll)
        : m_sourceRange(range)
        , m_range(range)
    {
        const CSSParserToken& token = range.peek();
        if (token.functionId() == CSSValueCalc || token.functionId() == CSSValueWebkitCalc)
            m_calcValue = CSSCalcValue::create(consumeFunction(m_range), valueRange);
    }

    const CSSCalcValue* value() const { return m_calcValue.get(); }
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeValue()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return CSSPrimitiveValue::create(m_calcValue.release());
    }
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeNumber()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        CSSPrimitiveValue::UnitType unitType = m_calcValue->isInt() ? CSSPrimitiveValue::UnitType::Integer : CSSPrimitiveValue::UnitType::Number;
        return cssValuePool().createValue(m_calcValue->doubleValue(), unitType);
    }

    bool consumeNumberRaw(double& result)
    {
        if (!m_calcValue || m_calcValue->category() != CalcNumber)
            return false;
        m_sourceRange = m_range;
        result = m_calcValue->doubleValue();
        return true;
    }

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    RefPtrWillBeMember<CSSCalcValue> m_calcValue;
};

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, double minimumValue)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (token.numericValueType() == NumberValueType || token.numericValue() < minimumValue)
            return nullptr;
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Integer);
    }
    CalcParser calcParser(range);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() != CalcNumber || !calculation->isInt())
            return nullptr;
        double value = calculation->doubleValue();
        if (value < minimumValue)
            return nullptr;
        return calcParser.consumeNumber();
    }
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeInteger(range, 1);
}

bool consumeNumberRaw(CSSParserTokenRange& range, double& result)
{
    if (range.peek().type() == NumberToken) {
        result = range.consumeIncludingWhitespace().numericValue();
        return true;
    }
    CalcParser calcParser(range, ValueRangeAll);
    return calcParser.consumeNumberRaw(result);
}

// TODO(timloh): Work out if this can just call consumeNumberRaw
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }
    CalcParser calcParser(range, ValueRangeAll);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        // TODO(rwlbuis) Calcs should not be subject to parse time range checks.
        // spec: https://drafts.csswg.org/css-values-3/#calc-range
        if (calculation->category() != CalcNumber || (valueRange == ValueRangeNonNegative && calculation->isNegative()))
            return nullptr;
        return calcParser.consumeNumber();
    }
    return nullptr;
}

inline bool shouldAcceptUnitlessLength(double value, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    // TODO(timloh): Presentational HTML attributes shouldn't use the CSS parser for lengths
    return value == 0
        || isUnitLessLengthParsingEnabledForMode(cssParserMode)
        || (cssParserMode == HTMLQuirksMode && unitless == UnitlessQuirk::Allow);
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSPrimitiveValue::UnitType::QuirkyEms:
            if (cssParserMode != UASheetMode)
                return nullptr;
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
            break;
        default:
            return nullptr;
        }
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }
    if (token.type() == NumberToken) {
        if (!shouldAcceptUnitlessLength(token.numericValue(), cssParserMode, unitless)
            || (valueRange == ValueRangeNonNegative && token.numericValue() < 0))
            return nullptr;
        CSSPrimitiveValue::UnitType unitType = CSSPrimitiveValue::UnitType::Pixels;
        if (cssParserMode == SVGAttributeMode)
            unitType = CSSPrimitiveValue::UnitType::UserUnits;
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), unitType);
    }
    if (cssParserMode == SVGAttributeMode)
        return nullptr;
    CalcParser calcParser(range, valueRange);
    if (calcParser.value() && calcParser.value()->category() == CalcLength)
        return calcParser.consumeValue();
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == PercentageToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Percentage);
    }
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcPercent)
            return calcParser.consumeValue();
    }
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken || token.type() == NumberToken)
        return consumeLength(range, cssParserMode, valueRange, unitless);
    if (token.type() == PercentageToken)
        return consumePercent(range, valueRange);
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcLength || calculation->category() == CalcPercent || calculation->category() == CalcPercentLength)
            return calcParser.consumeValue();
    }
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSPrimitiveValue::UnitType::Degrees:
        case CSSPrimitiveValue::UnitType::Radians:
        case CSSPrimitiveValue::UnitType::Gradians:
        case CSSPrimitiveValue::UnitType::Turns:
            return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
        default:
            return nullptr;
        }
    }
    if (token.type() == NumberToken && token.numericValue() == 0) {
        range.consumeIncludingWhitespace();
        return cssValuePool().createValue(0, CSSPrimitiveValue::UnitType::Degrees);
    }
    CalcParser calcParser(range, ValueRangeAll);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcAngle)
            return calcParser.consumeValue();
    }
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        CSSPrimitiveValue::UnitType unit = token.unitType();
        if (unit == CSSPrimitiveValue::UnitType::Milliseconds || unit == CSSPrimitiveValue::UnitType::Seconds)
            return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
        return nullptr;
    }
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcTime)
            return calcParser.consumeValue();
    }
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullptr;
    return cssValuePool().createIdentifierValue(range.consumeIncludingWhitespace().id());
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeIdentRange(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return nullptr;
    return consumeIdent(range);
}

PassRefPtrWillBeRawPtr<CSSCustomIdentValue> consumeCustomIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || isCSSWideKeyword(range.peek().id()))
        return nullptr;
    return CSSCustomIdentValue::create(range.consumeIncludingWhitespace().value());
}

PassRefPtrWillBeRawPtr<CSSStringValue> consumeString(CSSParserTokenRange& range)
{
    if (range.peek().type() != StringToken)
        return nullptr;
    return CSSStringValue::create(range.consumeIncludingWhitespace().value());
}

String consumeUrl(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == UrlToken) {
        range.consumeIncludingWhitespace();
        return token.value();
    }
    if (token.functionId() == CSSValueUrl) {
        CSSParserTokenRange urlRange = range;
        CSSParserTokenRange urlArgs = urlRange.consumeBlock();
        const CSSParserToken& next = urlArgs.consumeIncludingWhitespace();
        if (next.type() == BadStringToken || !urlArgs.atEnd())
            return String();
        ASSERT(next.type() == StringToken);
        range = urlRange;
        range.consumeWhitespace();
        return next.value();
    }

    return String();
}

} // namespace CSSPropertyParserHelpers

} // namespace blink
