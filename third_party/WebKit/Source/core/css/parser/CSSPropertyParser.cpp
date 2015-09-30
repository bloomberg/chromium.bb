// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSPropertyParser.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValuePool.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/frame/UseCounter.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSPropertyParser::CSSPropertyParser(CSSParserValueList* valueList, const CSSParserTokenRange& range,
    const CSSParserContext& context, WillBeHeapVector<CSSProperty, 256>& parsedProperties,
    StyleRule::Type ruleType)
    : m_valueList(valueList)
    , m_range(range)
    , m_context(context)
    , m_parsedProperties(parsedProperties)
    , m_ruleType(ruleType)
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
            && parser.parseViewportProperty(resolvedProperty, important);
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

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeCustomIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullptr;
    return cssValuePool().createValue(range.consumeIncludingWhitespace().value(), CSSPrimitiveValue::UnitType::CustomIdentifier);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeString(CSSParserTokenRange& range)
{
    if (range.peek().type() != StringToken)
        return nullptr;
    return cssValuePool().createValue(range.consumeIncludingWhitespace().value(), CSSPrimitiveValue::UnitType::String);
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

class CalcParseScope {
    STACK_ALLOCATED();

public:
    explicit CalcParseScope(CSSParserTokenRange& range, ValueRange valueRange = ValueRangeAll)
        : m_sourceRange(range)
        , m_range(range)
    {
        const CSSParserToken& token = range.peek();
        if (token.functionId() == CSSValueCalc || token.functionId() == CSSValueWebkitCalc)
            m_calcValue = CSSCalcValue::create(consumeFunction(m_range), valueRange);
    }

    const CSSCalcValue* value() const { return m_calcValue.get(); }
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> releaseValue()
    {
        m_sourceRange = m_range;
        return CSSPrimitiveValue::create(m_calcValue.release());
    }
    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> releaseNumber()
    {
        m_sourceRange = m_range;
        CSSPrimitiveValue::UnitType unitType = m_calcValue->isInt() ? CSSPrimitiveValue::UnitType::Integer : CSSPrimitiveValue::UnitType::Number;
        return cssValuePool().createValue(m_calcValue->doubleValue(), unitType);
    }

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    RefPtrWillBeMember<CSSCalcValue> m_calcValue;
};

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, CSSParserMode cssParserMode, double minimumValue = std::numeric_limits<int>::min())
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (token.numericValueType() == NumberValueType || token.numericValue() < minimumValue)
            return nullptr;
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }
    CalcParseScope calcScope(range);
    if (const CSSCalcValue* calculation = calcScope.value()) {
        if (calculation->category() != CalcNumber || !calculation->isInt())
            return nullptr;
        double value = calculation->doubleValue();
        if (value < minimumValue)
            return nullptr;
        return calcScope.releaseNumber();
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
        return cssValuePool().createValue(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Pixels);
    }
    CalcParseScope calcScope(range, valueRange);
    if (const CSSCalcValue* calculation = calcScope.value()) {
        if (calculation->category() != CalcLength)
            return nullptr;
        return calcScope.releaseValue();
    }
    return nullptr;
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

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumePage(CSSParserTokenRange& range)
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
        RefPtrWillBeRawPtr<CSSValue> parsedValue = consumeString(range);
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

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::consumeFontVariant()
{
    if (m_ruleType != StyleRule::FontFace) {
        if (m_range.peek().id() != CSSValueNormal && m_range.peek().id() != CSSValueSmallCaps)
            return nullptr;
        return consumeIdent(m_range);
    }
    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    do {
        if (m_range.peek().id() == CSSValueAll) {
            // FIXME: CSSPropertyParser::parseFontVariant() implements
            // the old css3 draft:
            // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802/#font-variant
            // 'all' is only allowed in @font-face and with no other values.
            if (values->length())
                return nullptr;
            return consumeIdent(m_range);
        }
        if (m_range.peek().id() == CSSValueNormal || m_range.peek().id() == CSSValueSmallCaps)
            values->append(consumeIdent(m_range));
    } while (consumeCommaIncludingWhitespace(m_range));

    if (values->length())
        return values.release();

    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSValue> consumeFontWeight(CSSParserTokenRange& range)
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
    if (range.peek().id() >= CSSValueSerif && range.peek().id() <= CSSValueWebkitBody)
        return consumeIdent(range);
    return nullptr;
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
    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue = consumeInteger(range, cssParserMode, 0);
    if (parsedValue)
        return parsedValue;
    return consumeLength(range, cssParserMode, ValueRangeNonNegative);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseSingleValue(CSSPropertyID propId)
{
    m_range.consumeWhitespace();
    switch (propId) {
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
    case CSSPropertyWebkitFontFeatureSettings:
        return consumeFontFeatureSettings(m_range);
    case CSSPropertyFontVariant:
        return consumeFontVariant();
    case CSSPropertyFontFamily:
        return consumeFontFamily(m_range);
    case CSSPropertyFontWeight:
        return consumeFontWeight(m_range);
    case CSSPropertyLetterSpacing:
    case CSSPropertyWordSpacing:
        return consumeSpacing(m_range, m_context.mode());
    case CSSPropertyTabSize:
        return consumeTabSize(m_range, m_context.mode());
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
    case CSSPropertyFontWeight:
    case CSSPropertyWebkitFontFeatureSettings:
        parsedValue = parseSingleValue(propId);
        break;
    default:
        break;
    }

    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(propId, parsedValue.release(), false);
    return true;
}

bool CSSPropertyParser::parseShorthand(CSSPropertyID propId, bool important)
{
    m_range.consumeWhitespace();
    switch (propId) {
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
    default:
        return false;
    }
}

} // namespace blink
