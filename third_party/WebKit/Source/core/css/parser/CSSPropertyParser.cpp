// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSPropertyParser.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSValuePool.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/frame/UseCounter.h"

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

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumePage(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeCustomIdent(range);
}

// [ <string> <string> ]+ | none
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
    default:
        return nullptr;
    }
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
