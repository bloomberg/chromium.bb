// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSPropertyParser.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSValuePool.h"
#include "core/css/FontFace.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutTheme.h"
#include "core/svg/SVGPathUtilities.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSPropertyParser::CSSPropertyParser(CSSParserValueList* valueList, const CSSParserTokenRange& range,
    const CSSParserContext& context, WillBeHeapVector<CSSProperty, 256>& parsedProperties,
    StyleRule::Type ruleType)
    : m_valueList(valueList)
    , m_range(range)
    , m_context(context)
    , m_parsedProperties(parsedProperties)
    , m_inParseShorthand(0)
    , m_currentShorthand(CSSPropertyInvalid)
    , m_implicitShorthand(false)
{
}

bool CSSPropertyParser::parseValue(CSSPropertyID unresolvedProperty, bool important,
    const CSSParserTokenRange& range, const CSSParserContext& context,
    WillBeHeapVector<CSSProperty, 256>& parsedProperties, StyleRule::Type ruleType)
{
    int parsedPropertiesSize = parsedProperties.size();

    CSSParserValueList valueList(range);
    if (!valueList.size())
        return false; // Parser error
    CSSPropertyParser parser(&valueList, range, context, parsedProperties, ruleType);
    CSSPropertyID resolvedProperty = resolveCSSPropertyID(unresolvedProperty);
    bool parseSuccess;

    if (ruleType == StyleRule::Viewport) {
        parseSuccess = (RuntimeEnabledFeatures::cssViewportEnabled() || isUASheetBehavior(context.mode()))
            && parser.parseViewportDescriptor(resolvedProperty, important);
    } else if (ruleType == StyleRule::FontFace) {
        parseSuccess = parser.parseFontFaceDescriptor(resolvedProperty);
    } else {
        parseSuccess = parser.parseValue(unresolvedProperty, important);
    }

    // This doesn't count UA style sheets
    if (parseSuccess && context.useCounter())
        context.useCounter()->count(context, unresolvedProperty);

    if (!parseSuccess)
        parser.rollbackLastProperties(parsedProperties.size() - parsedPropertiesSize);

    return parseSuccess;
}

// Helper methods for consuming tokens starts here.
static bool consumeCommaIncludingWhitespace(CSSParserTokenRange& valueList)
{
    CSSParserToken value = valueList.peek();
    if (value.type() != CommaToken)
        return false;
    valueList.consumeIncludingWhitespace();
    return true;
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullptr;
    return cssValuePool().createIdentifierValue(range.consumeIncludingWhitespace().id());
}

template<typename... emptyBaseCase> inline bool identMatches(CSSValueID id) { return false; }
template<CSSValueID head, CSSValueID... tail> inline bool identMatches(CSSValueID id)
{
    return id == head || identMatches<tail...>(id);
}

template<CSSValueID... names> PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || !identMatches<names...>(range.peek().id()))
        return nullptr;
    return cssValuePool().createIdentifierValue(range.consumeIncludingWhitespace().id());
}

static PassRefPtrWillBeRawPtr<CSSCustomIdentValue> consumeCustomIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullptr;
    return CSSCustomIdentValue::create(range.consumeIncludingWhitespace().value());
}

static PassRefPtrWillBeRawPtr<CSSStringValue> consumeString(CSSParserTokenRange& range)
{
    if (range.peek().type() != StringToken)
        return nullptr;
    return CSSStringValue::create(range.consumeIncludingWhitespace().value());
}

static String consumeUrl(CSSParserTokenRange& range)
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

static CSSParserTokenRange consumeFunction(CSSParserTokenRange& range)
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
        if (!m_calcValue)
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

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, double minimumValue = -std::numeric_limits<double>::max())
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

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeInteger(range, 1);
}

static bool consumeNumberRaw(CSSParserTokenRange& range, double& result)
{
    if (range.peek().type() == NumberToken) {
        result = range.consumeIncludingWhitespace().numericValue();
        return true;
    }
    CalcParser calcParser(range, ValueRangeAll);
    return calcParser.consumeNumberRaw(result);
}

// TODO(timloh): Work out if this can just call consumeNumberRaw
static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
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

inline bool shouldAcceptUnitlessValues(double fValue, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    // Quirks mode for certain properties and presentation attributes accept unit-less values for certain units.
    return !fValue // 0 can always be unitless.
        || isUnitLessLengthParsingEnabledForMode(cssParserMode) // HTML and SVG attribute values can always be unitless.
        || (cssParserMode == HTMLQuirksMode && (unitless == UnitlessQuirk::Allow));
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
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
        if (!shouldAcceptUnitlessValues(token.numericValue(), cssParserMode, unitless)
            || (valueRange == ValueRangeNonNegative && token.numericValue() < 0))
            return nullptr;
        CSSPrimitiveValue::UnitType unitType = CSSPrimitiveValue::UnitType::Pixels;
        if (cssParserMode == SVGAttributeMode)
            unitType = CSSPrimitiveValue::UnitType::UserUnits;
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), unitType);
    }
    CalcParser calcParser(range, valueRange);
    if (calcParser.value() && calcParser.value()->category() == CalcLength)
        return calcParser.consumeValue();
    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
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

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
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

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range, CSSParserMode cssParserMode)
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
    if (token.type() == NumberToken) {
        if (!shouldAcceptUnitlessValues(token.numericValue(), cssParserMode, UnitlessQuirk::Forbid))
            return nullptr;
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Degrees);
    }
    CalcParser calcParser(range, ValueRangeAll);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcAngle)
            return calcParser.consumeValue();
    }
    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange& range, ValueRange valueRange)
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

static int clampRGBComponent(const CSSPrimitiveValue& value)
{
    double result = value.getDoubleValue();
    // TODO(timloh): Multiply by 2.55 and round instead of floor.
    if (value.isPercentage())
        result *= 2.56;
    return clampTo<int>(result, 0, 255);
}

static bool parseRGBParameters(CSSParserTokenRange& range, RGBA32& result, bool parseAlpha)
{
    ASSERT(range.peek().functionId() == CSSValueRgb || range.peek().functionId() == CSSValueRgba);
    CSSParserTokenRange args = consumeFunction(range);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> colorParameter = consumeInteger(args);
    if (!colorParameter)
        colorParameter = consumePercent(args, ValueRangeAll);
    if (!colorParameter)
        return false;
    const bool isPercent = colorParameter->isPercentage();
    int colorArray[3];
    colorArray[0] = clampRGBComponent(*colorParameter);
    for (int i = 1; i < 3; i++) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        colorParameter = isPercent ? consumePercent(args, ValueRangeAll) : consumeInteger(args);
        if (!colorParameter)
            return false;
        colorArray[i] = clampRGBComponent(*colorParameter);
    }
    if (parseAlpha) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        double alpha;
        if (!consumeNumberRaw(args, alpha))
            return false;
        // Convert the floating pointer number of alpha to an integer in the range [0, 256),
        // with an equal distribution across all 256 values.
        int alphaComponent = static_cast<int>(clampTo<double>(alpha, 0.0, 1.0) * nextafter(256.0, 0.0));
        result = makeRGBA(colorArray[0], colorArray[1], colorArray[2], alphaComponent);
    } else {
        result = makeRGB(colorArray[0], colorArray[1], colorArray[2]);
    }
    return args.atEnd();
}

static bool parseHSLParameters(CSSParserTokenRange& range, RGBA32& result, bool parseAlpha)
{
    ASSERT(range.peek().functionId() == CSSValueHsl || range.peek().functionId() == CSSValueHsla);
    CSSParserTokenRange args = consumeFunction(range);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> hslValue = consumeNumber(args, ValueRangeAll);
    if (!hslValue)
        return false;
    double colorArray[3];
    colorArray[0] = (((hslValue->getIntValue() % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; i++) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        hslValue = consumePercent(args, ValueRangeAll);
        if (!hslValue)
            return false;
        double doubleValue = hslValue->getDoubleValue();
        colorArray[i] = clampTo<double>(doubleValue, 0.0, 100.0) / 100.0; // Needs to be value between 0 and 1.0.
    }
    double alpha = 1.0;
    if (parseAlpha) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        if (!consumeNumberRaw(args, alpha))
            return false;
        alpha = clampTo<double>(alpha, 0.0, 1.0);
    }
    result = makeRGBAFromHSLA(colorArray[0], colorArray[1], colorArray[2], alpha);
    return args.atEnd();
}

static bool parseHexColor(CSSParserTokenRange& range, RGBA32& result, bool acceptQuirkyColors)
{
    const CSSParserToken& token = range.peek();
    String color;
    if (acceptQuirkyColors) {
        if (token.type() == NumberToken && token.numericValueType() == IntegerValueType
            && token.numericValue() >= 0. && token.numericValue() < 1000000.) { // e.g. 112233
            color = String::format("%06d", static_cast<int>(token.numericValue()));
        } else if (token.type() == DimensionToken) { // e.g. 0001FF
            color = String::number(static_cast<int>(token.numericValue())) + String(token.value());
            if (color.length() > 6)
                return false;
            while (color.length() < 6)
                color = "0" + color;
        } else if (token.type() == IdentToken) { // e.g. FF0000
            color = token.value();
        }
    }
    if (token.type() == HashToken)
        color = token.value();
    if (!Color::parseHexColor(color, result))
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

static bool parseColorFunction(CSSParserTokenRange& range, RGBA32& result)
{
    CSSValueID functionId = range.peek().functionId();
    if (functionId < CSSValueRgb || functionId > CSSValueHsla)
        return false;
    CSSParserTokenRange colorRange = range;
    if ((functionId <= CSSValueRgba && !parseRGBParameters(colorRange, result, functionId == CSSValueRgba))
        || (functionId >= CSSValueHsl && !parseHSLParameters(colorRange, result, functionId == CSSValueHsla)))
        return false;
    range = colorRange;
    return true;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeColor(CSSParserTokenRange& range, const CSSParserContext& context, bool acceptQuirkyColors = false)
{
    CSSValueID id = range.peek().id();
    if (CSSPropertyParser::isColorKeyword(id)) {
        if (!isValueAllowedInMode(id, context.mode()))
            return nullptr;
        return consumeIdent(range);
    }
    RGBA32 color = Color::transparent;
    if (!parseHexColor(range, color, acceptQuirkyColors) && !parseColorFunction(range, color))
        return nullptr;
    return cssValuePool().createColorValue(color);
}

static inline bool isCSSWideKeyword(const CSSValueID& id)
{
    return id == CSSValueInitial || id == CSSValueInherit || id == CSSValueUnset || id == CSSValueDefault;
}

// Methods for consuming non-shorthand properties starts here.
static PassRefPtrWillBeRawPtr<CSSValue> consumeWillChange(CSSParserTokenRange& range)
{
    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    if (range.peek().id() == CSSValueAuto) {
        // FIXME: This will be read back as an empty string instead of auto
        return values.release();
    }

    // Every comma-separated list of identifiers is a valid will-change value,
    // unless the list includes an explicitly disallowed identifier.
    while (true) {
        if (range.peek().type() != IdentToken)
            return nullptr;
        CSSPropertyID unresolvedProperty = unresolvedCSSPropertyID(range.peek().value());
        if (unresolvedProperty) {
            ASSERT(CSSPropertyMetadata::isEnabledProperty(unresolvedProperty));
            // Now "all" is used by both CSSValue and CSSPropertyValue.
            // Need to return nullptr when currentValue is CSSPropertyAll.
            if (unresolvedProperty == CSSPropertyWillChange || unresolvedProperty == CSSPropertyAll)
                return nullptr;
            values->append(cssValuePool().createIdentifierValue(unresolvedProperty));
            range.consumeIncludingWhitespace();
        } else {
            switch (range.peek().id()) {
            case CSSValueNone:
            case CSSValueAll:
            case CSSValueAuto:
            case CSSValueDefault:
            case CSSValueInitial:
            case CSSValueInherit:
                return nullptr;
            case CSSValueContents:
            case CSSValueScrollPosition:
                values->append(consumeIdent(range));
                break;
            default:
                range.consumeIncludingWhitespace();
                break;
            }
        }

        if (range.atEnd())
            break;
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    return values.release();
}

static PassRefPtrWillBeRawPtr<CSSFontFeatureValue> consumeFontFeatureTag(CSSParserTokenRange& range)
{
    // Feature tag name consists of 4-letter characters.
    static const unsigned tagNameLength = 4;

    const CSSParserToken& token = range.consumeIncludingWhitespace();
    // Feature tag name comes first
    if (token.type() != StringToken)
        return nullptr;
    if (token.value().length() != tagNameLength)
        return nullptr;
    AtomicString tag = token.value();
    for (unsigned i = 0; i < tagNameLength; ++i) {
        // Limits the range of characters to 0x20-0x7E, following the tag name rules defiend in the OpenType specification.
        UChar character = tag[i];
        if (character < 0x20 || character > 0x7E)
            return nullptr;
    }

    int tagValue = 1;
    // Feature tag values could follow: <integer> | on | off
    if (range.peek().type() == NumberToken && range.peek().numericValueType() == IntegerValueType && range.peek().numericValue() >= 0) {
        tagValue = clampTo<int>(range.consumeIncludingWhitespace().numericValue());
        if (tagValue < 0)
            return nullptr;
    } else if (range.peek().id() == CSSValueOn || range.peek().id() == CSSValueOff) {
        tagValue = range.consumeIncludingWhitespace().id() == CSSValueOn;
    }
    return CSSFontFeatureValue::create(tag, tagValue);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeFontFeatureSettings(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    RefPtrWillBeRawPtr<CSSValueList> settings = CSSValueList::createCommaSeparated();
    do {
        RefPtrWillBeRawPtr<CSSFontFeatureValue> fontFeatureValue = consumeFontFeatureTag(range);
        if (!fontFeatureValue)
            return nullptr;
        settings->append(fontFeatureValue);
    } while (consumeCommaIncludingWhitespace(range));
    return settings.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumePage(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeCustomIdent(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeQuotes(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    while (!range.atEnd()) {
        RefPtrWillBeRawPtr<CSSStringValue> parsedValue = consumeString(range);
        if (!parsedValue)
            return nullptr;
        values->append(parsedValue.release());
    }
    if (values->length() && values->length() % 2 == 0)
        return values.release();
    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeWebkitHighlight(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeString(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeFontVariantLigatures(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    RefPtrWillBeRawPtr<CSSValueList> ligatureValues = CSSValueList::createSpaceSeparated();
    bool sawCommonLigaturesValue = false;
    bool sawDiscretionaryLigaturesValue = false;
    bool sawHistoricalLigaturesValue = false;
    bool sawContextualLigaturesValue = false;
    do {
        CSSValueID id = range.peek().id();
        switch (id) {
        case CSSValueNoCommonLigatures:
        case CSSValueCommonLigatures:
            if (sawCommonLigaturesValue)
                return nullptr;
            sawCommonLigaturesValue = true;
            break;
        case CSSValueNoDiscretionaryLigatures:
        case CSSValueDiscretionaryLigatures:
            if (sawDiscretionaryLigaturesValue)
                return nullptr;
            sawDiscretionaryLigaturesValue = true;
            break;
        case CSSValueNoHistoricalLigatures:
        case CSSValueHistoricalLigatures:
            if (sawHistoricalLigaturesValue)
                return nullptr;
            sawHistoricalLigaturesValue = true;
            break;
        case CSSValueNoContextual:
        case CSSValueContextual:
            if (sawContextualLigaturesValue)
                return nullptr;
            sawContextualLigaturesValue = true;
            break;
        default:
            return nullptr;
        }
        ligatureValues->append(consumeIdent(range));
    } while (!range.atEnd());

    return ligatureValues.release();
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeFontVariant(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueNormal, CSSValueSmallCaps>(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeFontVariantList(CSSParserTokenRange& range)
{
    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    do {
        if (range.peek().id() == CSSValueAll) {
            // FIXME: CSSPropertyParser::parseFontVariant() implements
            // the old css3 draft:
            // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802/#font-variant
            // 'all' is only allowed in @font-face and with no other values.
            if (values->length())
                return nullptr;
            return consumeIdent(range);
        }
        RefPtrWillBeRawPtr<CSSPrimitiveValue> fontVariant = consumeFontVariant(range);
        if (fontVariant)
            values->append(fontVariant.release());
    } while (consumeCommaIncludingWhitespace(range));

    if (values->length())
        return values.release();

    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeFontWeight(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.id() >= CSSValueNormal && token.id() <= CSSValueLighter)
        return consumeIdent(range);
    if (token.type() != NumberToken || token.numericValueType() != IntegerValueType)
        return nullptr;
    int weight = static_cast<int>(token.numericValue());
    if ((weight % 100) || weight < 100 || weight > 900)
        return nullptr;
    range.consumeIncludingWhitespace();
    return cssValuePool().createIdentifierValue(static_cast<CSSValueID>(CSSValue100 + weight / 100 - 1));
}

static String concatenateFamilyName(CSSParserTokenRange& range)
{
    StringBuilder builder;
    bool addedSpace = false;
    const CSSParserToken& firstToken = range.peek();
    while (range.peek().type() == IdentToken) {
        if (!builder.isEmpty()) {
            builder.append(' ');
            addedSpace = true;
        }
        builder.append(range.consumeIncludingWhitespace().value());
    }
    if (!addedSpace && isCSSWideKeyword(firstToken.id()))
        return String();
    return builder.toString();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeFamilyName(CSSParserTokenRange& range)
{
    if (range.peek().type() == StringToken)
        return cssValuePool().createFontFamilyValue(range.consumeIncludingWhitespace().value());
    if (range.peek().type() != IdentToken)
        return nullptr;
    String familyName = concatenateFamilyName(range);
    if (familyName.isNull())
        return nullptr;
    return cssValuePool().createFontFamilyValue(familyName);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeGenericFamily(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueSerif, CSSValueSansSerif, CSSValueCursive, CSSValueFantasy, CSSValueMonospace, CSSValueWebkitBody>(range);
}

static PassRefPtrWillBeRawPtr<CSSValueList> consumeFontFamily(CSSParserTokenRange& range)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    do {
        RefPtrWillBeRawPtr<CSSValue> parsedValue = nullptr;
        if ((parsedValue = consumeGenericFamily(range))) {
            list->append(parsedValue);
        } else if ((parsedValue = consumeFamilyName(range))) {
            list->append(parsedValue);
        } else {
            return nullptr;
        }
    } while (consumeCommaIncludingWhitespace(range));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeSpacing(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    // TODO(timloh): Don't allow unitless values, and allow <percentage>s in word-spacing.
    return consumeLength(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Allow);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeTabSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue = consumeInteger(range, 0);
    if (parsedValue)
        return parsedValue;
    return consumeLength(range, cssParserMode, ValueRangeNonNegative);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeFontSize(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() >= CSSValueXxSmall && range.peek().id() <= CSSValueLarger)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative, unitless);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLineHeight(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> lineHeight = consumeNumber(range, ValueRangeNonNegative);
    if (lineHeight)
        return lineHeight;
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
}

static PassRefPtrWillBeRawPtr<CSSValueList> consumeRotation(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    ASSERT(RuntimeEnabledFeatures::cssIndependentTransformPropertiesEnabled());
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    RefPtrWillBeRawPtr<CSSValue> rotation = consumeAngle(range, cssParserMode);
    if (!rotation)
        return nullptr;
    list->append(rotation.release());

    if (range.atEnd())
        return list.release();

    for (unsigned i = 0; i < 3; i++) { // 3 dimensions of rotation
        RefPtrWillBeRawPtr<CSSValue> dimension = consumeNumber(range, ValueRangeAll);
        if (!dimension)
            return nullptr;
        list->append(dimension.release());
    }

    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeCounter(CSSParserTokenRange& range, CSSParserMode cssParserMode, int defaultValue)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    // TODO(rwlbuis): should be space separated list.
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    do {
        RefPtrWillBeRawPtr<CSSCustomIdentValue> counterName = consumeCustomIdent(range);
        if (!counterName)
            return nullptr;
        int i = defaultValue;
        if (RefPtrWillBeRawPtr<CSSPrimitiveValue> counterValue = consumeInteger(range))
            i = clampTo<int>(counterValue->getDoubleValue());
        list->append(CSSValuePair::create(counterName.release(),
            cssValuePool().createValue(i, CSSPrimitiveValue::UnitType::Number),
            CSSValuePair::DropIdenticalValues));
    } while (!range.atEnd());
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumePageSize(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueA3, CSSValueA4, CSSValueA5, CSSValueB4, CSSValueB5, CSSValueLedger, CSSValueLegal, CSSValueLetter>(range);
}

static PassRefPtrWillBeRawPtr<CSSValueList> consumeSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    RefPtrWillBeRawPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();

    if (range.peek().id() == CSSValueAuto) {
        result->append(consumeIdent(range));
        return result.release();
    }

    if (RefPtrWillBeRawPtr<CSSValue> width = consumeLength(range, cssParserMode, ValueRangeNonNegative)) {
        RefPtrWillBeRawPtr<CSSValue> height = consumeLength(range, cssParserMode, ValueRangeNonNegative);
        result->append(width.release());
        if (height)
            result->append(height.release());
        return result.release();
    }

    RefPtrWillBeRawPtr<CSSValue> pageSize = consumePageSize(range);
    RefPtrWillBeRawPtr<CSSValue> orientation = consumeIdent<CSSValuePortrait, CSSValueLandscape>(range);
    if (!pageSize)
        pageSize = consumePageSize(range);

    if (!orientation && !pageSize)
        return nullptr;
    if (pageSize)
        result->append(pageSize.release());
    if (orientation)
        result->append(orientation.release());
    return result.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeTextIndent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    // [ <length> | <percentage> ] && hanging? && each-line?
    // Keywords only allowed when css3Text is enabled.
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    bool hasLengthOrPercentage = false;
    bool hasEachLine = false;
    bool hasHanging = false;

    do {
        if (!hasLengthOrPercentage) {
            if (RefPtrWillBeRawPtr<CSSValue> textIndent = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Allow)) {
                list->append(textIndent.release());
                hasLengthOrPercentage = true;
                continue;
            }
        }

        if (RuntimeEnabledFeatures::css3TextEnabled()) {
            CSSValueID id = range.peek().id();
            if (!hasEachLine && id == CSSValueEachLine) {
                list->append(consumeIdent(range));
                hasEachLine = true;
                continue;
            }
            if (!hasHanging && id == CSSValueHanging) {
                list->append(consumeIdent(range));
                hasHanging = true;
                continue;
            }
        }
        return nullptr;
    } while (!range.atEnd());

    if (!hasLengthOrPercentage)
        return nullptr;

    return list.release();
}

static bool validWidthOrHeightKeyword(CSSValueID id, const CSSParserContext& context)
{
    if (id == CSSValueWebkitMinContent || id == CSSValueWebkitMaxContent || id == CSSValueWebkitFillAvailable || id == CSSValueWebkitFitContent
        || id == CSSValueMinContent || id == CSSValueMaxContent || id == CSSValueFitContent) {
        if (context.useCounter()) {
            switch (id) {
            case CSSValueWebkitMinContent:
                context.useCounter()->count(UseCounter::CSSValuePrefixedMinContent);
                break;
            case CSSValueWebkitMaxContent:
                context.useCounter()->count(UseCounter::CSSValuePrefixedMaxContent);
                break;
            case CSSValueWebkitFillAvailable:
                context.useCounter()->count(UseCounter::CSSValuePrefixedFillAvailable);
                break;
            case CSSValueWebkitFitContent:
                context.useCounter()->count(UseCounter::CSSValuePrefixedFitContent);
                break;
            default:
                break;
            }
        }
        return true;
    }
    return false;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeMaxWidthOrHeight(CSSParserTokenRange& range, const CSSParserContext& context, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() == CSSValueNone || validWidthOrHeightKeyword(range.peek().id(), context))
        return consumeIdent(range);
    return consumeLengthOrPercent(range, context.mode(), ValueRangeNonNegative, unitless);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeWidthOrHeight(CSSParserTokenRange& range, const CSSParserContext& context, UnitlessQuirk unitless = UnitlessQuirk::Forbid)
{
    if (range.peek().id() == CSSValueAuto || validWidthOrHeightKeyword(range.peek().id(), context))
        return consumeIdent(range);
    return consumeLengthOrPercent(range, context.mode(), ValueRangeNonNegative, unitless);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeClipComponent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRangeAll, UnitlessQuirk::Allow);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeClip(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    if (range.peek().functionId() != CSSValueRect)
        return nullptr;

    CSSParserTokenRange args = consumeFunction(range);
    // rect(t, r, b, l) || rect(t r b l)
    RefPtrWillBeRawPtr<CSSPrimitiveValue> top = consumeClipComponent(args, cssParserMode);
    if (!top)
        return nullptr;
    bool needsComma = consumeCommaIncludingWhitespace(args);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> right = consumeClipComponent(args, cssParserMode);
    if (!right || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottom = consumeClipComponent(args, cssParserMode);
    if (!bottom || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> left = consumeClipComponent(args, cssParserMode);
    if (!left || !args.atEnd())
        return nullptr;
    return CSSQuadValue::create(top.release(), right.release(), bottom.release(), left.release(), CSSQuadValue::SerializeAsRect);
}

static bool consumePan(CSSParserTokenRange& range, RefPtrWillBeRawPtr<CSSValue>& panX, RefPtrWillBeRawPtr<CSSValue>& panY)
{
    CSSValueID id = range.peek().id();
    if ((id == CSSValuePanX || id == CSSValuePanRight || id == CSSValuePanLeft) && !panX) {
        if (id != CSSValuePanX && !RuntimeEnabledFeatures::cssTouchActionPanDirectionsEnabled())
            return false;
        panX = consumeIdent(range);
    } else if ((id == CSSValuePanY || id == CSSValuePanDown || id == CSSValuePanUp) && !panY) {
        if (id != CSSValuePanY && !RuntimeEnabledFeatures::cssTouchActionPanDirectionsEnabled())
            return false;
        panY = consumeIdent(range);
    } else {
        return false;
    }
    return true;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeTouchAction(CSSParserTokenRange& range)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    CSSValueID id = range.peek().id();
    if (id == CSSValueAuto || id == CSSValueNone || id == CSSValueManipulation) {
        list->append(consumeIdent(range));
        return list.release();
    }

    RefPtrWillBeRawPtr<CSSValue> panX = nullptr;
    RefPtrWillBeRawPtr<CSSValue> panY = nullptr;
    if (!consumePan(range, panX, panY))
        return nullptr;
    if (!range.atEnd() && !consumePan(range, panX, panY))
        return nullptr;

    if (panX)
        list->append(panX.release());
    if (panY)
        list->append(panY.release());
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLineClamp(CSSParserTokenRange& range)
{
    if (range.peek().type() != PercentageToken && range.peek().type() != NumberToken)
        return nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> clampValue = consumePercent(range, ValueRangeNonNegative);
    if (clampValue)
        return clampValue;
    // When specifying number of lines, don't allow 0 as a valid value.
    return consumePositiveInteger(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeLocale(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeString(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeColumnWidth(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    // Always parse lengths in strict mode here, since it would be ambiguous otherwise when used in
    // the 'columns' shorthand property.
    RefPtrWillBeRawPtr<CSSPrimitiveValue> columnWidth = consumeLength(range, HTMLStandardMode, ValueRangeNonNegative);
    if (!columnWidth || (!columnWidth->isCalculated() && columnWidth->getDoubleValue() == 0))
        return nullptr;
    return columnWidth.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeColumnCount(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumePositiveInteger(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeColumnGap(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRangeNonNegative);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeColumnSpan(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueAll || id == CSSValueNone)
        return consumeIdent(range);
    if (range.peek().type() != NumberToken)
        return nullptr;
    if (RefPtrWillBeRawPtr<CSSPrimitiveValue> spanValue = consumeInteger(range)) {
        // 1 (will be dropped in the unprefixed property).
        if (spanValue->getIntValue() == 1)
            return spanValue.release();
    }
    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeZoom(CSSParserTokenRange& range, const CSSParserContext& context)
{
    const CSSParserToken& token = range.peek();
    RefPtrWillBeRawPtr<CSSPrimitiveValue> zoom;
    if (token.type() == IdentToken) {
        zoom = consumeIdent<CSSValueNormal, CSSValueReset, CSSValueDocument>(range);
    } else {
        zoom = consumePercent(range, ValueRangeNonNegative);
        if (!zoom)
            zoom = consumeNumber(range, ValueRangeNonNegative);
    }
    if (zoom && context.useCounter()
        && !(token.id() == CSSValueNormal
            || (token.type() == NumberToken && zoom->getDoubleValue() == 1)
            || (token.type() == PercentageToken && zoom->getDoubleValue() == 100)))
        context.useCounter()->count(UseCounter::CSSZoomNotEqualToOne);
    return zoom.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeAnimationIterationCount(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueInfinite)
        return consumeIdent(range);
    return consumeNumber(range, ValueRangeNonNegative);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeAnimationName(CSSParserTokenRange& range, const CSSParserContext& context, bool allowQuotedName)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    if (allowQuotedName && range.peek().type() == StringToken) {
        // Legacy support for strings in prefixed animations.
        if (context.useCounter())
            context.useCounter()->count(UseCounter::QuotedAnimationName);

        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (token.valueEqualsIgnoringCase("none"))
            return cssValuePool().createIdentifierValue(CSSValueNone);
        return CSSCustomIdentValue::create(token.value());
    }

    return consumeCustomIdent(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeTransitionProperty(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    // TODO(timloh): This should check isCSSWideKeyword
    if (token.id() == CSSValueInitial || token.id() == CSSValueInherit)
        return nullptr;
    if (token.id() == CSSValueNone)
        return consumeIdent(range);

    if (CSSPropertyID property = token.parseAsUnresolvedCSSPropertyID()) {
        ASSERT(CSSPropertyMetadata::isEnabledProperty(property));
        range.consumeIncludingWhitespace();
        return cssValuePool().createIdentifierValue(property);
    }
    return consumeCustomIdent(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeSteps(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueSteps);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> steps = consumePositiveInteger(args);
    if (!steps)
        return nullptr;

    StepsTimingFunction::StepAtPosition position = StepsTimingFunction::End;
    if (consumeCommaIncludingWhitespace(args)) {
        switch (args.consumeIncludingWhitespace().id()) {
        case CSSValueMiddle:
            if (!RuntimeEnabledFeatures::webAnimationsAPIEnabled())
                return nullptr;
            position = StepsTimingFunction::Middle;
            break;
        case CSSValueStart:
            position = StepsTimingFunction::Start;
            break;
        case CSSValueEnd:
            position = StepsTimingFunction::End;
            break;
        default:
            return nullptr;
        }
    }

    if (!args.atEnd())
        return nullptr;

    range = rangeCopy;
    return CSSStepsTimingFunctionValue::create(steps->getIntValue(), position);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeCubicBezier(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueCubicBezier);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    double x1, y1, x2, y2;
    if (consumeNumberRaw(args, x1)
        && x1 >= 0 && x1 <= 1
        && consumeCommaIncludingWhitespace(args)
        && consumeNumberRaw(args, y1)
        && consumeCommaIncludingWhitespace(args)
        && consumeNumberRaw(args, x2)
        && x2 >= 0 && x2 <= 1
        && consumeCommaIncludingWhitespace(args)
        && consumeNumberRaw(args, y2)
        && args.atEnd()) {
        range = rangeCopy;
        return CSSCubicBezierTimingFunctionValue::create(x1, y1, x2, y2);
    }

    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeAnimationTimingFunction(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueEase || id == CSSValueLinear || id == CSSValueEaseIn
        || id == CSSValueEaseOut || id == CSSValueEaseInOut || id == CSSValueStepStart
        || id == CSSValueStepEnd || id == CSSValueStepMiddle)
        return consumeIdent(range);

    CSSValueID function = range.peek().functionId();
    if (function == CSSValueSteps)
        return consumeSteps(range);
    if (function == CSSValueCubicBezier)
        return consumeCubicBezier(range);
    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeAnimationValue(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context, bool useLegacyParsing)
{
    switch (property) {
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
        return consumeTime(range, ValueRangeAll);
    case CSSPropertyAnimationDirection:
        return consumeIdent<CSSValueNormal, CSSValueAlternate, CSSValueReverse, CSSValueAlternateReverse>(range);
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
        return consumeTime(range, ValueRangeNonNegative);
    case CSSPropertyAnimationFillMode:
        return consumeIdent<CSSValueNone, CSSValueForwards, CSSValueBackwards, CSSValueBoth>(range);
    case CSSPropertyAnimationIterationCount:
        return consumeAnimationIterationCount(range);
    case CSSPropertyAnimationName:
        return consumeAnimationName(range, context, useLegacyParsing);
    case CSSPropertyAnimationPlayState:
        return consumeIdent<CSSValueRunning, CSSValuePaused>(range);
    case CSSPropertyTransitionProperty:
        return consumeTransitionProperty(range);
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeAnimationTimingFunction(range);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

static bool isValidAnimationPropertyList(CSSPropertyID property, const CSSValueList& valueList)
{
    if (property != CSSPropertyTransitionProperty || valueList.length() < 2)
        return true;
    for (auto& value : valueList) {
        if (value->isPrimitiveValue() && toCSSPrimitiveValue(*value).isValueID()
            && toCSSPrimitiveValue(*value).getValueID() == CSSValueNone)
            return false;
    }
    return true;
}

static PassRefPtrWillBeRawPtr<CSSValueList> consumeAnimationPropertyList(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context, bool useLegacyParsing)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    do {
        RefPtrWillBeRawPtr<CSSValue> value = consumeAnimationValue(property, range, context, useLegacyParsing);
        if (!value)
            return nullptr;
        list->append(value.release());
    } while (consumeCommaIncludingWhitespace(range));
    if (!isValidAnimationPropertyList(property, *list))
        return nullptr;
    ASSERT(list->length());
    return list.release();
}

bool CSSPropertyParser::consumeAnimationShorthand(const StylePropertyShorthand& shorthand, bool useLegacyParsing, bool important)
{
    const unsigned longhandCount = shorthand.length();
    RefPtrWillBeRawPtr<CSSValueList> longhands[8];
    ASSERT(longhandCount <= 8);
    for (size_t i = 0; i < longhandCount; ++i)
        longhands[i] = CSSValueList::createCommaSeparated();

    do {
        bool parsedLonghand[8] = { false };
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                if (RefPtrWillBeRawPtr<CSSValue> value = consumeAnimationValue(shorthand.properties()[i], m_range, m_context, useLegacyParsing)) {
                    parsedLonghand[i] = true;
                    foundProperty = true;
                    longhands[i]->append(value.release());
                    break;
                }
            }
            if (!foundProperty)
                return false;
        } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

        // TODO(timloh): This will make invalid longhands, see crbug.com/386459
        for (size_t i = 0; i < longhandCount; ++i) {
            if (!parsedLonghand[i])
                longhands[i]->append(cssValuePool().createImplicitInitialValue());
            parsedLonghand[i] = false;
        }
    } while (consumeCommaIncludingWhitespace(m_range));

    for (size_t i = 0; i < longhandCount; ++i) {
        if (!isValidAnimationPropertyList(shorthand.properties()[i], *longhands[i]))
            return false;
    }

    for (size_t i = 0; i < longhandCount; ++i)
        addProperty(shorthand.properties()[i], longhands[i].release(), important);

    return m_range.atEnd();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeWidowsOrOrphans(CSSParserTokenRange& range)
{
    // Support for auto is non-standard and for backwards compatibility.
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumePositiveInteger(range);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeZIndex(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeInteger(range);
}

static PassRefPtrWillBeRawPtr<CSSShadowValue> parseSingleShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool allowInset, bool allowSpread)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> style = nullptr;
    RefPtrWillBeRawPtr<CSSValue> color = nullptr;

    if (range.atEnd())
        return nullptr;
    if (range.peek().id() == CSSValueInset) {
        if (!allowInset)
            return nullptr;
        style = consumeIdent(range);
    }
    color = consumeColor(range, context);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> horizontalOffset = consumeLength(range, context.mode(), ValueRangeAll);
    if (!horizontalOffset)
        return nullptr;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> verticalOffset = consumeLength(range, context.mode(), ValueRangeAll);
    if (!verticalOffset)
        return nullptr;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> blurRadius = consumeLength(range, context.mode(), ValueRangeAll);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> spreadDistance = nullptr;
    if (blurRadius) {
        // Blur radius must be non-negative.
        if (blurRadius->getDoubleValue() < 0)
            return nullptr;
        if (allowSpread)
            spreadDistance = consumeLength(range, context.mode(), ValueRangeAll);
    }

    if (!range.atEnd()) {
        if (!color)
            color = consumeColor(range, context);
        if (range.peek().id() == CSSValueInset) {
            if (!allowInset || style)
                return nullptr;
            style = consumeIdent(range);
        }
    }
    return CSSShadowValue::create(horizontalOffset.release(), verticalOffset.release(), blurRadius.release(),
        spreadDistance.release(), style.release(), color.release());
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool isBoxShadowProperty)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtrWillBeRawPtr<CSSValueList> shadowValueList = CSSValueList::createCommaSeparated();
    do {
        if (RefPtrWillBeRawPtr<CSSShadowValue> shadowValue = parseSingleShadow(range, context, isBoxShadowProperty, isBoxShadowProperty))
            shadowValueList->append(shadowValue.release());
        else
            return nullptr;
    } while (consumeCommaIncludingWhitespace(range));
    return shadowValueList;
}

static PassRefPtrWillBeRawPtr<CSSFunctionValue> consumeFilterFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValueID filterType = range.peek().functionId();
    if (filterType < CSSValueInvert || filterType > CSSValueDropShadow)
        return nullptr;
    CSSParserTokenRange args = consumeFunction(range);
    RefPtrWillBeRawPtr<CSSFunctionValue> filterValue = CSSFunctionValue::create(filterType);
    RefPtrWillBeRawPtr<CSSValue> parsedValue = nullptr;

    if (filterType == CSSValueDropShadow) {
        parsedValue = parseSingleShadow(args, context, false, false);
    } else {
        // TODO(timloh): Add UseCounters for empty filter arguments.
        if (args.atEnd())
            return filterValue.release();
        if (filterType == CSSValueBrightness) {
            // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
            parsedValue = consumePercent(args, ValueRangeAll);
            if (!parsedValue)
                parsedValue = consumeNumber(args, ValueRangeAll);
        } else if (filterType == CSSValueHueRotate) {
            parsedValue = consumeAngle(args, context.mode());
        } else if (filterType == CSSValueBlur) {
            parsedValue = consumeLength(args, HTMLStandardMode, ValueRangeNonNegative);
        } else {
            // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
            parsedValue = consumePercent(args, ValueRangeNonNegative);
            if (!parsedValue)
                parsedValue = consumeNumber(args, ValueRangeNonNegative);
            if (parsedValue && filterType != CSSValueSaturate && filterType != CSSValueContrast) {
                double maxAllowed = toCSSPrimitiveValue(parsedValue.get())->isPercentage() ? 100.0 : 1.0;
                if (toCSSPrimitiveValue(parsedValue.get())->getDoubleValue() > maxAllowed)
                    return nullptr;
            }
        }
    }
    if (!parsedValue || !args.atEnd())
        return nullptr;
    filterValue->append(parsedValue.release());
    return filterValue.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeFilter(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    do {
        String url = consumeUrl(range);
        RefPtrWillBeRawPtr<CSSFunctionValue> filterValue = nullptr;
        if (!url.isNull()) {
            filterValue = CSSFunctionValue::create(CSSValueUrl);
            filterValue->append(CSSSVGDocumentValue::create(url));
        } else {
            filterValue = consumeFilterFunction(range, context);
            if (!filterValue)
                return nullptr;
        }
        list->append(filterValue.release());
    } while (!range.atEnd());
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    RefPtrWillBeRawPtr<CSSPrimitiveValue> ident;
    while ((ident = consumeIdent<CSSValueBlink, CSSValueUnderline, CSSValueOverline, CSSValueLineThrough>(range))) {
        if (list->hasValue(ident.get()))
            return nullptr;
        list->append(ident.release());
    }

    if (!list->length())
        return nullptr;
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeMotionPath(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);
    // FIXME: Add support for <url>, <basic-shape>, <geometry-box>.
    if (range.peek().functionId() != CSSValuePath)
        return nullptr;

    // FIXME: Add support for <fill-rule>.
    CSSParserTokenRange functionRange = range;
    CSSParserTokenRange functionArgs = consumeFunction(functionRange);

    if (functionArgs.peek().type() != StringToken)
        return nullptr;
    String pathString = functionArgs.consumeIncludingWhitespace().value();
    Path path;
    if (!buildPathFromString(pathString, path) || !functionArgs.atEnd())
        return nullptr;

    range = functionRange;
    return CSSPathValue::create(pathString);
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeMotionRotation(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    RefPtrWillBeRawPtr<CSSValue> angle = consumeAngle(range, cssParserMode);
    RefPtrWillBeRawPtr<CSSValue> keyword = consumeIdent<CSSValueAuto, CSSValueReverse>(range);
    if (!angle && !keyword)
        return nullptr;

    if (!angle)
        angle = consumeAngle(range, cssParserMode);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (keyword)
        list->append(keyword.release());
    if (angle)
        list->append(angle.release());
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    if (RefPtrWillBeRawPtr<CSSValue> textEmphasisStyle = consumeString(range))
        return textEmphasisStyle.release();

    RefPtrWillBeRawPtr<CSSPrimitiveValue> fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> shape = consumeIdent<CSSValueDot, CSSValueCircle, CSSValueDoubleCircle, CSSValueTriangle, CSSValueSesame>(range);
    if (!fill)
        fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    if (fill && shape) {
        RefPtrWillBeRawPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();
        parsedValues->append(fill.release());
        parsedValues->append(shape.release());
        return parsedValues.release();
    }
    if (fill)
        return fill.release();
    if (shape)
        return shape.release();
    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeOutlineColor(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // Outline color has "invert" as additional keyword.
    // Also, we want to allow the special focus color even in HTML Standard parsing mode.
    if (range.peek().id() == CSSValueInvert || range.peek().id() == CSSValueWebkitFocusRingColor)
        return consumeIdent(range);
    return consumeColor(range, context);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLineWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRangeNonNegative);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeBorderWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeLineWidth(range, cssParserMode);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeTextStrokeWidth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    return consumeLineWidth(range, cssParserMode);
}

static bool consumeTranslate3d(CSSParserTokenRange& args, CSSParserMode cssParserMode, RefPtrWillBeRawPtr<CSSFunctionValue>& transformValue)
{
    unsigned numberOfArguments = 2;
    RefPtrWillBeRawPtr<CSSValue> parsedValue;
    do {
        parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
        if (!parsedValue)
            return false;
        transformValue->append(parsedValue);
        if (!consumeCommaIncludingWhitespace(args))
            return false;
    } while (--numberOfArguments);
    parsedValue = consumeLength(args, cssParserMode, ValueRangeAll);
    if (!parsedValue)
        return false;
    transformValue->append(parsedValue);
    return true;
}

static bool consumeNumbers(CSSParserTokenRange& args, RefPtrWillBeRawPtr<CSSFunctionValue>& transformValue, unsigned numberOfArguments)
{
    do {
        RefPtrWillBeRawPtr<CSSValue> parsedValue = consumeNumber(args, ValueRangeAll);
        if (!parsedValue)
            return false;
        transformValue->append(parsedValue);
        if (--numberOfArguments && !consumeCommaIncludingWhitespace(args))
            return false;
    } while (numberOfArguments);
    return true;
}

static bool consumePerspective(CSSParserTokenRange& args, CSSParserMode cssParserMode, RefPtrWillBeRawPtr<CSSFunctionValue>& transformValue, bool useLegacyParsing)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue = consumeLength(args, cssParserMode, ValueRangeNonNegative);
    if (!parsedValue && useLegacyParsing) {
        double perspective;
        if (!consumeNumberRaw(args, perspective) || perspective < 0)
            return false;
        parsedValue = cssValuePool().createValue(perspective, CSSPrimitiveValue::UnitType::Pixels);
    }
    if (!parsedValue)
        return false;
    transformValue->append(parsedValue);
    return true;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeTransformValue(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool useLegacyParsing)
{
    CSSValueID functionId = range.peek().functionId();
    if (functionId == CSSValueInvalid)
        return nullptr;
    CSSParserTokenRange args = consumeFunction(range);
    if (args.atEnd())
        return nullptr;
    RefPtrWillBeRawPtr<CSSFunctionValue> transformValue = CSSFunctionValue::create(functionId);
    RefPtrWillBeRawPtr<CSSValue> parsedValue = nullptr;
    switch (functionId) {
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueSkewX:
    case CSSValueSkewY:
    case CSSValueSkew:
        parsedValue = consumeAngle(args, cssParserMode);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueSkew && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(parsedValue);
            parsedValue = consumeAngle(args, cssParserMode);
        }
        break;
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale:
        parsedValue = consumeNumber(args, ValueRangeAll);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueScale && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(parsedValue);
            parsedValue = consumeNumber(args, ValueRangeAll);
        }
        break;
    case CSSValuePerspective:
        if (!consumePerspective(args, cssParserMode, transformValue, useLegacyParsing))
            return nullptr;
        break;
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslate:
        parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueTranslate && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(parsedValue);
            parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
        }
        break;
    case CSSValueTranslateZ:
        parsedValue = consumeLength(args, cssParserMode, ValueRangeAll);
        break;
    case CSSValueMatrix:
    case CSSValueMatrix3d:
        if (!consumeNumbers(args, transformValue, (functionId == CSSValueMatrix3d) ? 16 : 6))
            return nullptr;
        break;
    case CSSValueScale3d:
        if (!consumeNumbers(args, transformValue, 3))
            return nullptr;
        break;
    case CSSValueRotate3d:
        if (!consumeNumbers(args, transformValue, 3) || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        parsedValue = consumeAngle(args, cssParserMode);
        break;
    case CSSValueTranslate3d:
        if (!consumeTranslate3d(args, cssParserMode, transformValue))
            return nullptr;
        break;
    default:
        return nullptr;
    }
    if (parsedValue)
        transformValue->append(parsedValue);
    if (!args.atEnd())
        return nullptr;
    return transformValue.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeTransform(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool useLegacyParsing)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    do {
        RefPtrWillBeRawPtr<CSSValue> parsedTransformValue = consumeTransformValue(range, cssParserMode, useLegacyParsing);
        if (!parsedTransformValue)
            return nullptr;
        list->append(parsedTransformValue.release());
    } while (!range.atEnd());

    return list.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseSingleValue(CSSPropertyID unresolvedProperty)
{
    CSSPropertyID property = resolveCSSPropertyID(unresolvedProperty);
    m_range.consumeWhitespace();
    switch (property) {
    case CSSPropertyWillChange:
        return consumeWillChange(m_range);
    case CSSPropertyPage:
        return consumePage(m_range);
    case CSSPropertyQuotes:
        return consumeQuotes(m_range);
    case CSSPropertyWebkitHighlight:
        return consumeWebkitHighlight(m_range);
    case CSSPropertyFontVariantLigatures:
        return consumeFontVariantLigatures(m_range);
    case CSSPropertyFontFeatureSettings:
        return consumeFontFeatureSettings(m_range);
    case CSSPropertyFontVariant:
        return consumeFontVariant(m_range);
    case CSSPropertyFontFamily:
        return consumeFontFamily(m_range);
    case CSSPropertyFontWeight:
        return consumeFontWeight(m_range);
    case CSSPropertyLetterSpacing:
    case CSSPropertyWordSpacing:
        return consumeSpacing(m_range, m_context.mode());
    case CSSPropertyTabSize:
        return consumeTabSize(m_range, m_context.mode());
    case CSSPropertyFontSize:
        return consumeFontSize(m_range, m_context.mode(), UnitlessQuirk::Allow);
    case CSSPropertyLineHeight:
        return consumeLineHeight(m_range, m_context.mode());
    case CSSPropertyRotate:
        return consumeRotation(m_range, m_context.mode());
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
        return consumeLength(m_range, m_context.mode(), ValueRangeNonNegative);
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
        return consumeCounter(m_range, m_context.mode(), property == CSSPropertyCounterIncrement ? 1 : 0);
    case CSSPropertySize:
        return consumeSize(m_range, m_context.mode());
    case CSSPropertyTextIndent:
        return consumeTextIndent(m_range, m_context.mode());
    case CSSPropertyMaxWidth:
    case CSSPropertyMaxHeight:
        return consumeMaxWidthOrHeight(m_range, m_context, UnitlessQuirk::Allow);
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
        return consumeMaxWidthOrHeight(m_range, m_context);
    case CSSPropertyMinWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyWidth:
    case CSSPropertyHeight:
        return consumeWidthOrHeight(m_range, m_context, UnitlessQuirk::Allow);
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
        return consumeWidthOrHeight(m_range, m_context);
    case CSSPropertyClip:
        return consumeClip(m_range, m_context.mode());
    case CSSPropertyTouchAction:
        return consumeTouchAction(m_range);
    case CSSPropertyWebkitLineClamp:
        return consumeLineClamp(m_range);
    case CSSPropertyWebkitFontSizeDelta:
        return consumeLength(m_range, m_context.mode(), ValueRangeAll, UnitlessQuirk::Allow);
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitLocale:
        return consumeLocale(m_range);
    case CSSPropertyWebkitColumnWidth:
        return consumeColumnWidth(m_range);
    case CSSPropertyWebkitColumnCount:
        return consumeColumnCount(m_range);
    case CSSPropertyWebkitColumnGap:
        return consumeColumnGap(m_range, m_context.mode());
    case CSSPropertyWebkitColumnSpan:
        return consumeColumnSpan(m_range, m_context.mode());
    case CSSPropertyZoom:
        return consumeZoom(m_range, m_context);
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyTransitionProperty:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeAnimationPropertyList(property, m_range, m_context, unresolvedProperty == CSSPropertyAliasWebkitAnimationName);
    case CSSPropertyOrphans:
    case CSSPropertyWidows:
        return consumeWidowsOrOrphans(m_range);
    case CSSPropertyTextDecorationColor:
        ASSERT(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
        return consumeColor(m_range, m_context);
    case CSSPropertyWebkitTextStrokeWidth:
        return consumeTextStrokeWidth(m_range, m_context.mode());
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTapHighlightColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitTextStrokeColor:
        return consumeColor(m_range, m_context);
    case CSSPropertyColor:
        return consumeColor(m_range, m_context, inQuirksMode());
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfterWidth:
        return consumeBorderWidth(m_range, m_context.mode());
    case CSSPropertyZIndex:
        return consumeZIndex(m_range);
    case CSSPropertyTextShadow: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
    case CSSPropertyBoxShadow:
        return consumeShadow(m_range, m_context, property == CSSPropertyBoxShadow);
    case CSSPropertyWebkitFilter:
    case CSSPropertyBackdropFilter:
        return consumeFilter(m_range, m_context);
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyTextDecorationLine:
        return consumeTextDecorationLine(m_range);
    case CSSPropertyMotionPath:
        ASSERT(RuntimeEnabledFeatures::cssMotionPathEnabled());
        return consumeMotionPath(m_range);
    case CSSPropertyMotionOffset:
        ASSERT(RuntimeEnabledFeatures::cssMotionPathEnabled());
        return consumeLengthOrPercent(m_range, m_context.mode(), ValueRangeAll);
    case CSSPropertyMotionRotation:
        ASSERT(RuntimeEnabledFeatures::cssMotionPathEnabled());
        return consumeMotionRotation(m_range, m_context.mode());
    case CSSPropertyWebkitTextEmphasisStyle:
        return consumeTextEmphasisStyle(m_range);
    case CSSPropertyOutlineColor:
        return consumeOutlineColor(m_range, m_context);
    case CSSPropertyOutlineOffset:
        return consumeLength(m_range, m_context.mode(), ValueRangeAll);
    case CSSPropertyOutlineWidth:
        return consumeLineWidth(m_range, m_context.mode());
    case CSSPropertyTransform:
        return consumeTransform(m_range, m_context.mode(), unresolvedProperty == CSSPropertyAliasWebkitTransform);
    default:
        return nullptr;
    }
}

static PassRefPtrWillBeRawPtr<CSSValueList> consumeFontFaceUnicodeRange(CSSParserTokenRange& range)
{
    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createCommaSeparated();

    do {
        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (token.type() != UnicodeRangeToken)
            return nullptr;

        UChar32 start = token.unicodeRangeStart();
        UChar32 end = token.unicodeRangeEnd();
        if (start > end)
            return nullptr;
        values->append(CSSUnicodeRangeValue::create(start, end));
    } while (consumeCommaIncludingWhitespace(range));

    return values.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::consumeFontFaceSrcURI()
{
    String url = consumeUrl(m_range);
    if (url.isNull())
        return nullptr;
    RefPtrWillBeRawPtr<CSSFontFaceSrcValue> uriValue(CSSFontFaceSrcValue::create(completeURL(url), m_context.shouldCheckContentSecurityPolicy()));
    uriValue->setReferrer(m_context.referrer());

    if (m_range.peek().functionId() != CSSValueFormat)
        return uriValue.release();

    // FIXME: https://drafts.csswg.org/css-fonts says that format() contains a comma-separated list of strings,
    // but CSSFontFaceSrcValue stores only one format. Allowing one format for now.
    // FIXME: IdentToken should not be supported here.
    CSSParserTokenRange args = consumeFunction(m_range);
    const CSSParserToken& arg = args.consumeIncludingWhitespace();
    if ((arg.type() != StringToken && arg.type() != IdentToken) || !args.atEnd())
        return nullptr;
    uriValue->setFormat(arg.value());
    return uriValue.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::consumeFontFaceSrcLocal()
{
    CSSParserTokenRange args = consumeFunction(m_range);
    ContentSecurityPolicyDisposition shouldCheckContentSecurityPolicy = m_context.shouldCheckContentSecurityPolicy();
    if (args.peek().type() == StringToken) {
        const CSSParserToken& arg = args.consumeIncludingWhitespace();
        if (!args.atEnd())
            return nullptr;
        return CSSFontFaceSrcValue::createLocal(arg.value(), shouldCheckContentSecurityPolicy);
    }
    if (args.peek().type() == IdentToken) {
        String familyName = concatenateFamilyName(args);
        if (!args.atEnd())
            return nullptr;
        return CSSFontFaceSrcValue::createLocal(familyName, shouldCheckContentSecurityPolicy);
    }
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValueList> CSSPropertyParser::consumeFontFaceSrc()
{
    RefPtrWillBeRawPtr<CSSValueList> values(CSSValueList::createCommaSeparated());

    do {
        const CSSParserToken& token = m_range.peek();
        RefPtrWillBeRawPtr<CSSValue> parsedValue = nullptr;
        if (token.functionId() == CSSValueLocal)
            parsedValue = consumeFontFaceSrcLocal();
        else
            parsedValue = consumeFontFaceSrcURI();
        if (!parsedValue)
            return nullptr;
        values->append(parsedValue);
    } while (consumeCommaIncludingWhitespace(m_range));
    return values.release();
}

bool CSSPropertyParser::parseFontFaceDescriptor(CSSPropertyID propId)
{
    RefPtrWillBeRawPtr<CSSValue> parsedValue = nullptr;

    m_range.consumeWhitespace();
    switch (propId) {
    case CSSPropertyFontFamily:
        if (consumeGenericFamily(m_range))
            return false;
        parsedValue = consumeFamilyName(m_range);
        break;
    case CSSPropertySrc: // This is a list of urls or local references.
        parsedValue = consumeFontFaceSrc();
        break;
    case CSSPropertyUnicodeRange:
        parsedValue = consumeFontFaceUnicodeRange(m_range);
        break;
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle: {
        CSSValueID id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(propId, id))
            return false;
        parsedValue = cssValuePool().createIdentifierValue(id);
        break;
    }
    case CSSPropertyFontVariant:
        parsedValue = consumeFontVariantList(m_range);
        break;
    case CSSPropertyFontWeight:
        parsedValue = consumeFontWeight(m_range);
        break;
    case CSSPropertyFontFeatureSettings:
        parsedValue = consumeFontFeatureSettings(m_range);
        break;
    default:
        break;
    }

    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(propId, parsedValue.release(), false);
    return true;
}

bool CSSPropertyParser::consumeSystemFont(bool important)
{
    CSSValueID systemFontID = m_range.consumeIncludingWhitespace().id();
    ASSERT(systemFontID >= CSSValueCaption && systemFontID <= CSSValueStatusBar);
    if (!m_range.atEnd())
        return false;

    FontStyle fontStyle = FontStyleNormal;
    FontWeight fontWeight = FontWeightNormal;
    float fontSize = 0;
    AtomicString fontFamily;
    LayoutTheme::theme().systemFont(systemFontID, fontStyle, fontWeight, fontSize, fontFamily);

    addProperty(CSSPropertyFontStyle, cssValuePool().createIdentifierValue(fontStyle == FontStyleItalic ? CSSValueItalic : CSSValueNormal), important);
    addProperty(CSSPropertyFontWeight, cssValuePool().createValue(fontWeight), important);
    addProperty(CSSPropertyFontSize, cssValuePool().createValue(fontSize, CSSPrimitiveValue::UnitType::Pixels), important);
    RefPtrWillBeRawPtr<CSSValueList> fontFamilyList = CSSValueList::createCommaSeparated();
    fontFamilyList->append(cssValuePool().createFontFamilyValue(fontFamily));
    addProperty(CSSPropertyFontFamily, fontFamilyList.release(), important);

    addProperty(CSSPropertyFontStretch, cssValuePool().createIdentifierValue(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariant, cssValuePool().createIdentifierValue(CSSValueNormal), important);
    addProperty(CSSPropertyLineHeight, cssValuePool().createIdentifierValue(CSSValueNormal), important);
    return true;
}

bool CSSPropertyParser::consumeFont(bool important)
{
    // Let's check if there is an inherit or initial somewhere in the shorthand.
    CSSParserTokenRange range = m_range;
    while (!range.atEnd()) {
        CSSValueID id = range.consumeIncludingWhitespace().id();
        if (id == CSSValueInherit || id == CSSValueInitial)
            return false;
    }
    // Optional font-style, font-variant, font-stretch and font-weight.
    RefPtrWillBeRawPtr<CSSPrimitiveValue> fontStyle = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> fontVariant = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> fontWeight = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> fontStretch = nullptr;
    while (!m_range.atEnd()) {
        CSSValueID id = m_range.peek().id();
        if (!fontStyle && CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyFontStyle, id)) {
            fontStyle = consumeIdent(m_range);
            continue;
        }
        if (!fontVariant) {
            // Font variant in the shorthand is particular, it only accepts normal or small-caps.
            fontVariant = consumeFontVariant(m_range);
            if (fontVariant)
                continue;
        }
        if (!fontWeight) {
            fontWeight = consumeFontWeight(m_range);
            if (fontWeight)
                continue;
        }
        if (!fontStretch && CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyFontStretch, id))
            fontStretch = consumeIdent(m_range);
        else
            break;
    }

    if (m_range.atEnd())
        return false;

    addProperty(CSSPropertyFontStyle, fontStyle ? fontStyle.release() : cssValuePool().createIdentifierValue(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariant, fontVariant ? fontVariant.release() : cssValuePool().createIdentifierValue(CSSValueNormal), important);
    addProperty(CSSPropertyFontWeight, fontWeight ? fontWeight.release() : cssValuePool().createIdentifierValue(CSSValueNormal), important);
    addProperty(CSSPropertyFontStretch, fontStretch ? fontStretch.release() : cssValuePool().createIdentifierValue(CSSValueNormal), important);

    // Now a font size _must_ come.
    RefPtrWillBeRawPtr<CSSValue> fontSize = consumeFontSize(m_range, m_context.mode());
    if (!fontSize || m_range.atEnd())
        return false;

    addProperty(CSSPropertyFontSize, fontSize.release(), important);

    if (m_range.peek().type() == DelimiterToken && m_range.peek().delimiter() == '/') {
        m_range.consumeIncludingWhitespace();
        RefPtrWillBeRawPtr<CSSPrimitiveValue> lineHeight = consumeLineHeight(m_range, m_context.mode());
        if (!lineHeight)
            return false;
        addProperty(CSSPropertyLineHeight, lineHeight.release(), important);
    } else {
        addProperty(CSSPropertyLineHeight, cssValuePool().createIdentifierValue(CSSValueNormal), important);
    }

    // Font family must come now.
    RefPtrWillBeRawPtr<CSSValue> parsedFamilyValue = consumeFontFamily(m_range);
    if (!parsedFamilyValue)
        return false;

    addProperty(CSSPropertyFontFamily, parsedFamilyValue.release(), important);

    // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20110324/#font-prop requires that
    // "font-stretch", "font-size-adjust", and "font-kerning" be reset to their initial values
    // but we don't seem to support them at the moment. They should also be added here once implemented.
    return m_range.atEnd();
}

bool CSSPropertyParser::consumeBorderSpacing(bool important)
{
    RefPtrWillBeRawPtr<CSSValue> horizontalSpacing = consumeLength(m_range, m_context.mode(), ValueRangeNonNegative, UnitlessQuirk::Allow);
    if (!horizontalSpacing)
        return false;
    RefPtrWillBeRawPtr<CSSValue> verticalSpacing = horizontalSpacing;
    if (!m_range.atEnd())
        verticalSpacing = consumeLength(m_range, m_context.mode(), ValueRangeNonNegative, UnitlessQuirk::Allow);
    if (!verticalSpacing || !m_range.atEnd())
        return false;
    addProperty(CSSPropertyWebkitBorderHorizontalSpacing, horizontalSpacing.release(), important);
    addProperty(CSSPropertyWebkitBorderVerticalSpacing, verticalSpacing.release(), important);
    return true;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeSingleViewportDescriptor(CSSParserTokenRange& range, CSSPropertyID propId, CSSParserMode cssParserMode)
{
    CSSValueID id = range.peek().id();
    switch (propId) {
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
        if (id == CSSValueAuto || id == CSSValueInternalExtendToZoom)
            return consumeIdent(range);
        return consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom: {
        if (id == CSSValueAuto)
            return consumeIdent(range);
        RefPtrWillBeRawPtr<CSSValue> parsedValue = consumeNumber(range, ValueRangeNonNegative);
        if (parsedValue)
            return parsedValue.release();
        return consumePercent(range, ValueRangeNonNegative);
    }
    case CSSPropertyUserZoom:
        return consumeIdent<CSSValueZoom, CSSValueFixed>(range);
    case CSSPropertyOrientation:
        return consumeIdent<CSSValueAuto, CSSValuePortrait, CSSValueLandscape>(range);
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

bool CSSPropertyParser::parseViewportDescriptor(CSSPropertyID propId, bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssViewportEnabled() || isUASheetBehavior(m_context.mode()));

    m_range.consumeWhitespace();

    switch (propId) {
    case CSSPropertyWidth: {
        RefPtrWillBeRawPtr<CSSValue> minWidth = consumeSingleViewportDescriptor(m_range, CSSPropertyMinWidth, m_context.mode());
        if (!minWidth)
            return false;
        RefPtrWillBeRawPtr<CSSValue> maxWidth = minWidth;
        if (!m_range.atEnd())
            maxWidth = consumeSingleViewportDescriptor(m_range, CSSPropertyMaxWidth, m_context.mode());
        if (!maxWidth || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyMinWidth, minWidth.release(), important);
        addProperty(CSSPropertyMaxWidth, maxWidth.release(), important);
        return true;
    }
    case CSSPropertyHeight: {
        RefPtrWillBeRawPtr<CSSValue> minHeight = consumeSingleViewportDescriptor(m_range, CSSPropertyMinHeight, m_context.mode());
        if (!minHeight)
            return false;
        RefPtrWillBeRawPtr<CSSValue> maxHeight = minHeight;
        if (!m_range.atEnd())
            maxHeight = consumeSingleViewportDescriptor(m_range, CSSPropertyMaxHeight, m_context.mode());
        if (!maxHeight || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyMinHeight, minHeight.release(), important);
        addProperty(CSSPropertyMaxHeight, maxHeight.release(), important);
        return true;
    }
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom:
    case CSSPropertyUserZoom:
    case CSSPropertyOrientation: {
        RefPtrWillBeRawPtr<CSSValue> parsedValue = consumeSingleViewportDescriptor(m_range, propId, m_context.mode());
        if (!parsedValue || !m_range.atEnd())
            return false;
        addProperty(propId, parsedValue.release(), important);
        return true;
    }
    default:
        return false;
    }
}

static void consumeColumnWidthOrCount(CSSParserTokenRange& range, CSSParserMode cssParserMode, RefPtrWillBeRawPtr<CSSValue> &columnWidth, RefPtrWillBeRawPtr<CSSValue> &columnCount)
{
    if (range.peek().id() == CSSValueAuto) {
        consumeIdent(range);
        return;
    }
    if (!columnWidth) {
        if ((columnWidth = consumeColumnWidth(range)))
            return;
    }
    if (!columnCount)
        columnCount = consumeColumnCount(range);
}

bool CSSPropertyParser::consumeColumns(bool important)
{
    RefPtrWillBeRawPtr<CSSValue> columnWidth = nullptr;
    RefPtrWillBeRawPtr<CSSValue> columnCount = nullptr;
    consumeColumnWidthOrCount(m_range, m_context.mode(), columnWidth, columnCount);
    consumeColumnWidthOrCount(m_range, m_context.mode(), columnWidth, columnCount);
    if (!m_range.atEnd())
        return false;
    if (!columnWidth)
        columnWidth = cssValuePool().createIdentifierValue(CSSValueAuto);
    if (!columnCount)
        columnCount = cssValuePool().createIdentifierValue(CSSValueAuto);
    addProperty(CSSPropertyWebkitColumnWidth, columnWidth.release(), important);
    addProperty(CSSPropertyWebkitColumnCount, columnCount.release(), important);
    return true;
}

bool CSSPropertyParser::consumeShorthandGreedily(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() <= 6); // Existing shorthands have at most 6 longhands.
    RefPtrWillBeRawPtr<CSSValue> longhands[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    const CSSPropertyID* shorthandProperties = shorthand.properties();
    do {
        bool foundLonghand = false;
        for (size_t i = 0; !foundLonghand && i < shorthand.length(); ++i) {
            if (longhands[i])
                continue;
            // TODO: parseSingleValue needs to handle fastpath keywords.
            if (CSSParserFastPaths::isKeywordPropertyID(shorthandProperties[i])) {
                if (CSSParserFastPaths::isValidKeywordPropertyAndValue(shorthandProperties[i], m_range.peek().id()))
                    longhands[i] = consumeIdent(m_range);
            } else {
                longhands[i] = parseSingleValue(shorthandProperties[i]);
            }
            if (longhands[i])
                foundLonghand = true;
        }
        if (!foundLonghand)
            return false;
    } while (!m_range.atEnd());

    ImplicitScope implicitScope(this);
    for (size_t i = 0; i < shorthand.length(); ++i) {
        if (longhands[i])
            addProperty(shorthandProperties[i], longhands[i].release(), important);
        else
            addProperty(shorthandProperties[i], cssValuePool().createImplicitInitialValue(), important);
    }
    return true;
}

bool CSSPropertyParser::parseShorthand(CSSPropertyID unresolvedProperty, bool important)
{
    CSSPropertyID property = resolveCSSPropertyID(unresolvedProperty);

    m_range.consumeWhitespace();
    CSSPropertyID oldShorthand = m_currentShorthand;
    // TODO(rob.buis): Remove this when the legacy property parser is gone
    m_currentShorthand = property;
    switch (property) {
    case CSSPropertyWebkitMarginCollapse: {
        CSSValueID id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyWebkitMarginBeforeCollapse, id))
            return false;
        RefPtrWillBeRawPtr<CSSValue> beforeCollapse = cssValuePool().createIdentifierValue(id);
        addProperty(CSSPropertyWebkitMarginBeforeCollapse, beforeCollapse, important);
        if (m_range.atEnd()) {
            addProperty(CSSPropertyWebkitMarginAfterCollapse, beforeCollapse, important);
            return true;
        }
        id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyWebkitMarginAfterCollapse, id))
            return false;
        addProperty(CSSPropertyWebkitMarginAfterCollapse, cssValuePool().createIdentifierValue(id), important);
        return true;
    }
    case CSSPropertyOverflow: {
        CSSValueID id = m_range.consumeIncludingWhitespace().id();
        if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyOverflowY, id))
            return false;
        if (!m_range.atEnd())
            return false;
        RefPtrWillBeRawPtr<CSSValue> overflowYValue = cssValuePool().createIdentifierValue(id);

        RefPtrWillBeRawPtr<CSSValue> overflowXValue = nullptr;

        // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y. If this value has been
        // set using the shorthand, then for now overflow-x will default to auto, but once we implement
        // pagination controls, it should default to hidden. If the overflow-y value is anything but
        // paged-x or paged-y, then overflow-x and overflow-y should have the same value.
        if (id == CSSValueWebkitPagedX || id == CSSValueWebkitPagedY)
            overflowXValue = cssValuePool().createIdentifierValue(CSSValueAuto);
        else
            overflowXValue = overflowYValue;
        addProperty(CSSPropertyOverflowX, overflowXValue.release(), important);
        addProperty(CSSPropertyOverflowY, overflowYValue.release(), important);
        return true;
    }
    case CSSPropertyFont: {
        const CSSParserToken& token = m_range.peek();
        if (token.id() >= CSSValueCaption && token.id() <= CSSValueStatusBar)
            return consumeSystemFont(important);
        return consumeFont(important);
    }
    case CSSPropertyBorderSpacing:
        return consumeBorderSpacing(important);
    case CSSPropertyWebkitColumns: {
        // TODO(rwlbuis): investigate if this shorthand hack can be removed.
        m_currentShorthand = oldShorthand;
        return consumeColumns(important);
    }
    case CSSPropertyAnimation:
        return consumeAnimationShorthand(animationShorthandForParsing(), unresolvedProperty == CSSPropertyAliasWebkitAnimation, important);
    case CSSPropertyTransition:
        return consumeAnimationShorthand(transitionShorthandForParsing(), false, important);
    case CSSPropertyTextDecoration: {
        // Fall through 'text-decoration-line' parsing if CSS 3 Text Decoration
        // is disabled to match CSS 2.1 rules for parsing 'text-decoration'.
        if (RuntimeEnabledFeatures::css3TextDecorationsEnabled())
            return consumeShorthandGreedily(textDecorationShorthand(), important);
        // TODO(rwlbuis): investigate if this shorthand hack can be removed.
        m_currentShorthand = oldShorthand;
        RefPtrWillBeRawPtr<CSSValue> textDecoration = consumeTextDecorationLine(m_range);
        if (!textDecoration || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyTextDecoration, textDecoration.release(), important);
        return true;
    }
    case CSSPropertyMotion:
        ASSERT(RuntimeEnabledFeatures::cssMotionPathEnabled());
        return consumeShorthandGreedily(motionShorthand(), important);
    case CSSPropertyWebkitTextEmphasis:
        return consumeShorthandGreedily(webkitTextEmphasisShorthand(), important);
    case CSSPropertyOutline:
        return consumeShorthandGreedily(outlineShorthand(), important);
    case CSSPropertyWebkitBorderStart:
        return consumeShorthandGreedily(webkitBorderStartShorthand(), important);
    case CSSPropertyWebkitBorderEnd:
        return consumeShorthandGreedily(webkitBorderEndShorthand(), important);
    case CSSPropertyWebkitBorderBefore:
        return consumeShorthandGreedily(webkitBorderBeforeShorthand(), important);
    case CSSPropertyWebkitBorderAfter:
        return consumeShorthandGreedily(webkitBorderAfterShorthand(), important);
    case CSSPropertyWebkitTextStroke:
        return consumeShorthandGreedily(webkitTextStrokeShorthand(), important);
    default:
        m_currentShorthand = oldShorthand;
        return false;
    }
}

} // namespace blink
