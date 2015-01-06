// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSParserImpl.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSSelectorParser.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/UseCounter.h"
#include "wtf/BitArray.h"

namespace blink {

CSSParserImpl::CSSParserImpl(const CSSParserContext& context, const String& string, StyleSheetContents* styleSheet)
: m_context(context)
, m_defaultNamespace(starAtom)
, m_styleSheet(styleSheet)
{
    CSSTokenizer::tokenize(string, m_tokens);
}

bool CSSParserImpl::parseValue(MutableStylePropertySet* declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    CSSRuleSourceData::Type ruleType = CSSRuleSourceData::STYLE_RULE;
    if (declaration->cssParserMode() == CSSViewportRuleMode)
        ruleType = CSSRuleSourceData::VIEWPORT_RULE;
    parser.consumeDeclarationValue(CSSParserTokenRange(parser.m_tokens), propertyID, important, ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return false;
    declaration->addParsedProperties(parser.m_parsedProperties);
    return true;
}

static inline void filterProperties(bool important, const WillBeHeapVector<CSSProperty, 256>& input, WillBeHeapVector<CSSProperty, 256>& output, size_t& unusedEntries, BitArray<numCSSProperties>& seenProperties)
{
    // Add properties in reverse order so that highest priority definitions are reached first. Duplicate definitions can then be ignored when found.
    for (size_t i = input.size(); i--; ) {
        const CSSProperty& property = input[i];
        if (property.isImportant() != important)
            continue;
        const unsigned propertyIDIndex = property.id() - firstCSSProperty;
        if (seenProperties.get(propertyIDIndex))
            continue;
        seenProperties.set(propertyIDIndex);
        output[--unusedEntries] = property;
    }
}

static PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> createStylePropertySet(const WillBeHeapVector<CSSProperty, 256>& parsedProperties, CSSParserMode mode)
{
    BitArray<numCSSProperties> seenProperties;
    size_t unusedEntries = parsedProperties.size();
    WillBeHeapVector<CSSProperty, 256> results(unusedEntries);

    filterProperties(true, parsedProperties, results, unusedEntries, seenProperties);
    filterProperties(false, parsedProperties, results, unusedEntries, seenProperties);

    return ImmutableStylePropertySet::create(results.data() + unusedEntries, results.size() - unusedEntries, mode);
}

PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> CSSParserImpl::parseInlineStyleDeclaration(const String& string, Element* element)
{
    Document& document = element->document();
    CSSParserContext context = CSSParserContext(document.elementSheet().contents()->parserContext(), UseCounter::getFrom(&document));
    CSSParserMode mode = element->isHTMLElement() && !document.inQuirksMode() ? HTMLStandardMode : HTMLQuirksMode;
    context.setMode(mode);
    CSSParserImpl parser(context, string);
    parser.consumeDeclarationList(CSSParserTokenRange(parser.m_tokens), CSSRuleSourceData::STYLE_RULE);
    return createStylePropertySet(parser.m_parsedProperties, mode);
}

bool CSSParserImpl::parseDeclaration(MutableStylePropertySet* declaration, const String& string, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    CSSRuleSourceData::Type ruleType = CSSRuleSourceData::STYLE_RULE;
    if (declaration->cssParserMode() == CSSViewportRuleMode)
        ruleType = CSSRuleSourceData::VIEWPORT_RULE;
    parser.consumeDeclarationList(CSSParserTokenRange(parser.m_tokens), ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return false;
    declaration->addParsedProperties(parser.m_parsedProperties);
    return true;
}

PassRefPtrWillBeRawPtr<StyleRuleBase> CSSParserImpl::parseRule(const String& string, const CSSParserContext& context)
{
    AllowedRulesType allowedRules = RegularRules;

    CSSParserImpl parser(context, string);
    CSSParserTokenRange range(parser.m_tokens);
    range.consumeWhitespaceAndComments();
    if (range.atEnd())
        return nullptr; // Parse error, empty rule
    RefPtrWillBeRawPtr<StyleRuleBase> rule;
    if (range.peek().type() == AtKeywordToken)
        rule = parser.consumeAtRule(range, allowedRules);
    else
        rule = parser.consumeQualifiedRule(range, allowedRules);
    if (!rule)
        return nullptr; // Parse error, failed to consume rule
    range.consumeWhitespaceAndComments();
    if (!rule || !range.atEnd())
        return nullptr; // Parse error, trailing garbage
    return rule;
}

void CSSParserImpl::parseStyleSheet(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet)
{
    CSSParserImpl parser(context, string, styleSheet);
    WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase>> rules = parser.consumeRuleList(parser.m_tokens, TopLevelRuleList);
    for (const auto& rule : rules)
        styleSheet->parserAppendRule(rule);
}

WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase>> CSSParserImpl::consumeRuleList(CSSParserTokenRange range, RuleListType ruleListType)
{
    WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase>> result;

    AllowedRulesType allowedRules;
    switch (ruleListType) {
    case TopLevelRuleList:
        allowedRules = AllowCharsetRules;
        break;
    case RegularRuleList:
        allowedRules = RegularRules;
        break;
    case KeyframesRuleList:
        allowedRules = KeyframeRules;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    while (!range.atEnd()) {
        switch (range.peek().type()) {
        case WhitespaceToken:
        case CommentToken:
            range.consumeWhitespaceAndComments();
            break;
        case AtKeywordToken:
            if (PassRefPtrWillBeRawPtr<StyleRuleBase> rule = consumeAtRule(range, allowedRules))
                result.append(rule);
            break;
        default:
            // FIXME: TopLevelRuleList should skip <CDO-token> and <CDC-token>
            if (PassRefPtrWillBeRawPtr<StyleRuleBase> rule = consumeQualifiedRule(range, allowedRules))
                result.append(rule);
            break;
        }
    }

    return result;
}

PassRefPtrWillBeRawPtr<StyleRuleBase> CSSParserImpl::consumeAtRule(CSSParserTokenRange& range, AllowedRulesType& allowedRules)
{
    ASSERT(range.peek().type() == AtKeywordToken);
    const String& name = range.consume().value();
    const CSSParserToken* preludeStart = &range.peek();
    while (!range.atEnd() && range.peek().type() != LeftBraceToken && range.peek().type() != SemicolonToken)
        range.consumeComponentValue();

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());
    // The prelude isn't used yet since individual at-rules are not yet implemented,
    // so this silences a compiler warning.
    prelude.peek();

    if (range.atEnd() || range.peek().type() == SemicolonToken) {
        range.consume();
        if (allowedRules == AllowCharsetRules && equalIgnoringCase(name, "charset")) {
            // @charset is actually parsed before we get into the CSS parser.
            // In theory we should validate the prelude is a string, but we don't
            // have error logging yet so it doesn't matter.
            return nullptr;
        }
        return nullptr; // Parser error, unrecognised at-rule without block
    }

    range.consumeBlock();
    return nullptr; // Parser error, unrecognised at-rule with block
}

PassRefPtrWillBeRawPtr<StyleRuleBase> CSSParserImpl::consumeQualifiedRule(CSSParserTokenRange& range, AllowedRulesType& allowedRules)
{
    const CSSParserToken* preludeStart = &range.peek();
    while (!range.atEnd() && range.peek().type() != LeftBraceToken)
        range.consumeComponentValue();

    if (range.atEnd())
        return nullptr; // Parse error, EOF instead of qualified rule block

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());
    CSSParserTokenRange block = range.consumeBlock();

    if (allowedRules <= RegularRules) {
        allowedRules = RegularRules;
        return consumeStyleRule(prelude, block);
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

PassRefPtrWillBeRawPtr<StyleRule> CSSParserImpl::consumeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSSelectorList selectorList;
    CSSSelectorParser::parseSelector(prelude, m_context, m_defaultNamespace, m_styleSheet, selectorList);
    if (!selectorList.isValid())
        return nullptr; // Parse error, invalid selector list
    consumeDeclarationList(block, CSSRuleSourceData::STYLE_RULE);

    RefPtrWillBeRawPtr<StyleRule> rule = StyleRule::create();
    rule->wrapperAdoptSelectorList(selectorList);
    rule->setProperties(createStylePropertySet(m_parsedProperties, m_context.mode()));
    m_parsedProperties.clear();
    return rule.release();
}

void CSSParserImpl::consumeDeclarationList(CSSParserTokenRange range, CSSRuleSourceData::Type ruleType)
{
    while (!range.atEnd()) {
        switch (range.peek().type()) {
        case CommentToken:
        case WhitespaceToken:
        case SemicolonToken:
            range.consume();
            break;
        case IdentToken: {
            const CSSParserToken* declarationStart = &range.peek();
            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();
            consumeDeclaration(range.makeSubRange(declarationStart, &range.peek()), ruleType);
            break;
        }
        default: // Parser error
            // FIXME: The spec allows at-rules in a declaration list
            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();
            break;
        }
    }
}

void CSSParserImpl::consumeDeclaration(CSSParserTokenRange range, CSSRuleSourceData::Type ruleType)
{
    ASSERT(range.peek().type() == IdentToken);
    CSSPropertyID id = range.consume().parseAsCSSPropertyID();
    range.consumeWhitespaceAndComments();
    if (range.consume().type() != ColonToken)
        return; // Parser error

    const CSSParserToken* last = range.end() - 1;
    while (last->type() == WhitespaceToken || last->type() == CommentToken)
        --last;
    if (last->type() == IdentToken && equalIgnoringCase(last->value(), "important")) {
        --last;
        while (last->type() == WhitespaceToken || last->type() == CommentToken)
            --last;
        if (last->type() == DelimiterToken && last->delimiter() == '!') {
            consumeDeclarationValue(range.makeSubRange(&range.peek(), last), id, true, ruleType);
            return;
        }
    }
    consumeDeclarationValue(range.makeSubRange(&range.peek(), range.end()), id, false, ruleType);
}

void CSSParserImpl::consumeDeclarationValue(CSSParserTokenRange range, CSSPropertyID propertyID, bool important, CSSRuleSourceData::Type ruleType)
{
    CSSParserValueList valueList(range);
    if (!valueList.size())
        return; // Parser error
    bool inViewport = ruleType == CSSRuleSourceData::VIEWPORT_RULE;
    CSSPropertyParser::parseValue(propertyID, important, &valueList, m_context, inViewport, m_parsedProperties, ruleType);
}

} // namespace blink
