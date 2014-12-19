// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSSelectorParser.h"

#include "core/css/CSSSelectorList.h"
#include "core/css/StyleSheetContents.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void CSSSelectorParser::parseSelector(CSSParserTokenRange tokenRange, const CSSParserContext& context, CSSSelectorList& output)
{
    CSSSelectorParser parser(tokenRange, context);
    parser.m_tokenRange.consumeWhitespaceAndComments();
    CSSSelectorList result;
    parser.consumeComplexSelectorList(result);
    if (parser.m_tokenRange.atEnd())
        output.adopt(result);
    ASSERT(!(output.isValid() && parser.m_failedParsing));
}

CSSSelectorParser::CSSSelectorParser(CSSParserTokenRange tokenRange, const CSSParserContext& context)
: m_tokenRange(tokenRange)
, m_context(context)
, m_defaultNamespace(starAtom)
, m_styleSheet(nullptr)
, m_failedParsing(false)
{
}

void CSSSelectorParser::consumeComplexSelectorList(CSSSelectorList& output)
{
    Vector<OwnPtr<CSSParserSelector>> selectorList;
    OwnPtr<CSSParserSelector> selector = consumeComplexSelector();
    if (!selector)
        return;
    selectorList.append(selector.release());
    while (!m_tokenRange.atEnd() && m_tokenRange.peek().type() == CommaToken) {
        m_tokenRange.consumeIncludingWhitespaceAndComments();
        selector = consumeComplexSelector();
        if (!selector)
            return;
        selectorList.append(selector.release());
    }

    if (!m_failedParsing)
        output.adoptSelectorVector(selectorList);
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeComplexSelector()
{
    OwnPtr<CSSParserSelector> selector = consumeCompoundSelector();
    if (!selector)
        return nullptr;
    while (CSSSelector::Relation combinator = consumeCombinator()) {
        OwnPtr<CSSParserSelector> nextSelector = consumeCompoundSelector();
        if (!nextSelector)
            return combinator == CSSSelector::Descendant ? selector.release() : nullptr;
        CSSParserSelector* end = nextSelector.get();
        while (end->tagHistory())
            end = end->tagHistory();
        end->setRelation(combinator);
        if (selector->isContentPseudoElement())
            end->setRelationIsAffectedByPseudoContent();
        end->setTagHistory(selector.release());

        selector = nextSelector.release();
    }

    return selector.release();
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeCompoundSelector()
{
    OwnPtr<CSSParserSelector> selector;

    AtomicString namespacePrefix;
    AtomicString elementName;
    bool hasNamespace;
    if (!consumeName(elementName, namespacePrefix, hasNamespace)) {
        selector = consumeSimpleSelector();
        if (!selector)
            return nullptr;
    }
    if (m_context.isHTMLDocument())
        elementName = elementName.lower();

    while (OwnPtr<CSSParserSelector> nextSelector = consumeSimpleSelector()) {
        if (selector)
            selector = rewriteSpecifiers(selector.release(), nextSelector.release());
        else
            selector = nextSelector.release();
    }

    if (!selector) {
        if (hasNamespace)
            return CSSParserSelector::create(determineNameInNamespace(namespacePrefix, elementName));
        return CSSParserSelector::create(QualifiedName(nullAtom, elementName, m_defaultNamespace));
    }
    if (elementName.isNull())
        rewriteSpecifiersWithNamespaceIfNeeded(selector.get());
    else
        rewriteSpecifiersWithElementName(namespacePrefix, elementName, selector.get());
    return selector.release();
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeSimpleSelector()
{
    const CSSParserToken& token = m_tokenRange.peek();
    OwnPtr<CSSParserSelector> selector;
    if (token.type() == HashToken)
        selector = consumeId();
    else if (token.type() == DelimiterToken && token.delimiter() == '.')
        selector = consumeClass();
    else if (token.type() == LeftBracketToken)
        selector = consumeAttribute();
    else if (token.type() == ColonToken)
        selector = consumePseudo();
    else
        return nullptr;
    if (!selector)
        m_failedParsing = true;
    return selector.release();
}

bool CSSSelectorParser::consumeName(AtomicString& name, AtomicString& namespacePrefix, bool& hasNamespace)
{
    name = nullAtom;
    namespacePrefix = nullAtom;
    hasNamespace = false;

    const CSSParserToken& firstToken = m_tokenRange.peek();
    if (firstToken.type() == IdentToken) {
        name = AtomicString(firstToken.value());
        m_tokenRange.consumeIncludingComments();
    } else if (firstToken.type() == DelimiterToken && firstToken.delimiter() == '*') {
        name = starAtom;
        m_tokenRange.consumeIncludingComments();
    } else if (firstToken.type() == DelimiterToken && firstToken.delimiter() == '|') {
        // No namespace
    } else {
        return false;
    }

    if (m_tokenRange.peek().type() != DelimiterToken || m_tokenRange.peek().delimiter() != '|')
        return true;
    m_tokenRange.consumeIncludingComments();

    hasNamespace = true;
    namespacePrefix = name;
    const CSSParserToken& nameToken = m_tokenRange.consumeIncludingComments();
    if (nameToken.type() == IdentToken) {
        name = AtomicString(nameToken.value());
    } else if (nameToken.type() == DelimiterToken && nameToken.delimiter() == '*') {
        name = starAtom;
    } else {
        name = nullAtom;
        namespacePrefix = nullAtom;
        return false;
    }

    return true;
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeId()
{
    ASSERT(m_tokenRange.peek().type() == HashToken);
    if (m_tokenRange.peek().hashTokenType() != HashTokenId)
        return nullptr;
    OwnPtr<CSSParserSelector> selector = CSSParserSelector::create();
    selector->setMatch(CSSSelector::Id);
    const String& value = m_tokenRange.consumeIncludingComments().value();
    if (isQuirksModeBehavior(m_context.mode()))
        selector->setValue(AtomicString(value.lower()));
    else
        selector->setValue(AtomicString(value));
    return selector.release();
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeClass()
{
    ASSERT(m_tokenRange.peek().type() == DelimiterToken);
    ASSERT(m_tokenRange.peek().delimiter() == '.');
    m_tokenRange.consumeIncludingComments();
    if (m_tokenRange.peek().type() != IdentToken)
        return nullptr;
    OwnPtr<CSSParserSelector> selector = CSSParserSelector::create();
    selector->setMatch(CSSSelector::Class);
    const String& value = m_tokenRange.consumeIncludingComments().value();
    if (isQuirksModeBehavior(m_context.mode()))
        selector->setValue(AtomicString(value.lower()));
    else
        selector->setValue(AtomicString(value));
    return selector.release();
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeAttribute()
{
    ASSERT(m_tokenRange.peek().type() == LeftBracketToken);
    m_tokenRange.consumeIncludingWhitespaceAndComments();

    AtomicString namespacePrefix;
    AtomicString attributeName;
    bool hasNamespace;
    if (!consumeName(attributeName, namespacePrefix, hasNamespace))
        return nullptr;

    if (m_context.isHTMLDocument())
        attributeName = attributeName.lower();

    QualifiedName qualifiedName = hasNamespace
        ? determineNameInNamespace(namespacePrefix, attributeName)
        : QualifiedName(nullAtom, attributeName, nullAtom);

    OwnPtr<CSSParserSelector> selector = CSSParserSelector::create();

    if (m_tokenRange.atEnd() || m_tokenRange.peek().type() == RightBracketToken) {
        m_tokenRange.consumeIncludingComments();
        selector->setAttribute(qualifiedName, CSSSelector::CaseSensitive);
        selector->setMatch(CSSSelector::AttributeSet);
        return selector.release();
    }

    selector->setMatch(consumeAttributeMatch());

    const CSSParserToken& attributeValue = m_tokenRange.consumeIncludingWhitespaceAndComments();
    if (attributeValue.type() != IdentToken && attributeValue.type() != StringToken)
        return nullptr;
    selector->setValue(AtomicString(attributeValue.value()));
    selector->setAttribute(qualifiedName, consumeAttributeFlags());

    if (!m_tokenRange.atEnd() && m_tokenRange.consumeIncludingComments().type() != RightBracketToken)
        return nullptr;

    return selector.release();
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumePseudo()
{
    // FIXME: Implement pseudo-element and pseudo-class parsing
    return nullptr;
}

CSSSelector::Relation CSSSelectorParser::consumeCombinator()
{
    CSSSelector::Relation fallbackResult = CSSSelector::SubSelector;
    while (m_tokenRange.peek().type() == WhitespaceToken || m_tokenRange.peek().type() == CommentToken) {
        if (m_tokenRange.consume().type() == WhitespaceToken)
            fallbackResult = CSSSelector::Descendant;
    }

    if (m_tokenRange.peek().type() != DelimiterToken)
        return fallbackResult;

    UChar delimiter = m_tokenRange.peek().delimiter();

    if (delimiter == '+' || delimiter == '~' || delimiter == '>') {
        m_tokenRange.consumeIncludingWhitespaceAndComments();
        if (delimiter == '+')
            return CSSSelector::DirectAdjacent;
        if (delimiter == '~')
            return CSSSelector::IndirectAdjacent;
        return CSSSelector::Child;
    }

    // Match /deep/
    if (delimiter != '/')
        return fallbackResult;
    m_tokenRange.consumeIncludingComments();
    const CSSParserToken& ident = m_tokenRange.consumeIncludingComments();
    if (ident.type() != IdentToken || !equalIgnoringCase(ident.value(), "deep"))
        m_failedParsing = true;
    const CSSParserToken& slash = m_tokenRange.consumeIncludingWhitespaceAndComments();
    if (slash.type() != DelimiterToken || slash.delimiter() != '/')
        m_failedParsing = true;
    return CSSSelector::ShadowDeep;
}

CSSSelector::Match CSSSelectorParser::consumeAttributeMatch()
{
    const CSSParserToken& token = m_tokenRange.consumeIncludingWhitespaceAndComments();
    switch (token.type()) {
    case IncludeMatchToken:
        return CSSSelector::AttributeList;
    case DashMatchToken:
        return CSSSelector::AttributeHyphen;
    case PrefixMatchToken:
        return CSSSelector::AttributeBegin;
    case SuffixMatchToken:
        return CSSSelector::AttributeEnd;
    case SubstringMatchToken:
        return CSSSelector::AttributeContain;
    case DelimiterToken:
        if (token.delimiter() == '=')
            return CSSSelector::AttributeExact;
    default:
        m_failedParsing = true;
        return CSSSelector::AttributeExact;
    }
}

CSSSelector::AttributeMatchType CSSSelectorParser::consumeAttributeFlags()
{
    if (m_tokenRange.peek().type() != IdentToken)
        return CSSSelector::CaseSensitive;
    const CSSParserToken& flag = m_tokenRange.consumeIncludingWhitespaceAndComments();
    if (flag.value() == "i") {
        if (RuntimeEnabledFeatures::cssAttributeCaseSensitivityEnabled() || isUASheetBehavior(m_context.mode()))
            return CSSSelector::CaseInsensitive;
    }
    m_failedParsing = true;
    return CSSSelector::CaseSensitive;
}

QualifiedName CSSSelectorParser::determineNameInNamespace(const AtomicString& prefix, const AtomicString& localName)
{
    if (!m_styleSheet)
        return QualifiedName(prefix, localName, m_defaultNamespace);
    return QualifiedName(prefix, localName, m_styleSheet->determineNamespace(prefix));
}

void CSSSelectorParser::rewriteSpecifiersWithNamespaceIfNeeded(CSSParserSelector* specifiers)
{
    if (m_defaultNamespace != starAtom || specifiers->crossesTreeScopes())
        rewriteSpecifiersWithElementName(nullAtom, starAtom, specifiers, /*tagIsForNamespaceRule*/true);
}

void CSSSelectorParser::rewriteSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule)
{
    AtomicString determinedNamespace = namespacePrefix != nullAtom && m_styleSheet ? m_styleSheet->determineNamespace(namespacePrefix) : m_defaultNamespace;
    QualifiedName tag(namespacePrefix, elementName, determinedNamespace);

    if (specifiers->crossesTreeScopes())
        return rewriteSpecifiersWithElementNameForCustomPseudoElement(tag, elementName, specifiers, tagIsForNamespaceRule);

    if (specifiers->isContentPseudoElement())
        return rewriteSpecifiersWithElementNameForContentPseudoElement(tag, elementName, specifiers, tagIsForNamespaceRule);

    // *:host never matches, so we can't discard the * otherwise we can't tell the
    // difference between *:host and just :host.
    if (tag == anyQName() && !specifiers->hasHostPseudoSelector())
        return;
    if (specifiers->pseudoType() != CSSSelector::PseudoCue)
        specifiers->prependTagSelector(tag, tagIsForNamespaceRule);
}

void CSSSelectorParser::rewriteSpecifiersWithElementNameForCustomPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule)
{
    CSSParserSelector* lastShadowPseudo = specifiers;
    CSSParserSelector* history = specifiers;
    while (history->tagHistory()) {
        history = history->tagHistory();
        if (history->crossesTreeScopes() || history->hasShadowPseudo())
            lastShadowPseudo = history;
    }

    if (lastShadowPseudo->tagHistory()) {
        if (tag != anyQName())
            lastShadowPseudo->tagHistory()->prependTagSelector(tag, tagIsForNamespaceRule);
        return;
    }

    // For shadow-ID pseudo-elements to be correctly matched, the ShadowPseudo combinator has to be used.
    // We therefore create a new Selector with that combinator here in any case, even if matching any (host) element in any namespace (i.e. '*').
    OwnPtr<CSSParserSelector> elementNameSelector = adoptPtr(new CSSParserSelector(tag));
    lastShadowPseudo->setTagHistory(elementNameSelector.release());
    lastShadowPseudo->setRelation(CSSSelector::ShadowPseudo);
}

void CSSSelectorParser::rewriteSpecifiersWithElementNameForContentPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule)
{
    CSSParserSelector* last = specifiers;
    CSSParserSelector* history = specifiers;
    while (history->tagHistory()) {
        history = history->tagHistory();
        if (history->isContentPseudoElement() || history->relationIsAffectedByPseudoContent())
            last = history;
    }

    if (last->tagHistory()) {
        if (tag != anyQName())
            last->tagHistory()->prependTagSelector(tag, tagIsForNamespaceRule);
        return;
    }

    // For shadow-ID pseudo-elements to be correctly matched, the ShadowPseudo combinator has to be used.
    // We therefore create a new Selector with that combinator here in any case, even if matching any (host) element in any namespace (i.e. '*').
    OwnPtr<CSSParserSelector> elementNameSelector = adoptPtr(new CSSParserSelector(tag));
    last->setTagHistory(elementNameSelector.release());
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::rewriteSpecifiers(PassOwnPtr<CSSParserSelector> specifiers, PassOwnPtr<CSSParserSelector> newSpecifier)
{
    if (newSpecifier->crossesTreeScopes()) {
        // Unknown pseudo element always goes at the top of selector chain.
        newSpecifier->appendTagHistory(CSSSelector::ShadowPseudo, specifiers);
        return newSpecifier;
    }
    if (newSpecifier->isContentPseudoElement()) {
        newSpecifier->appendTagHistory(CSSSelector::SubSelector, specifiers);
        return newSpecifier;
    }
    if (specifiers->crossesTreeScopes()) {
        // Specifiers for unknown pseudo element go right behind it in the chain.
        specifiers->insertTagHistory(CSSSelector::SubSelector, newSpecifier, CSSSelector::ShadowPseudo);
        return specifiers;
    }
    if (specifiers->isContentPseudoElement()) {
        specifiers->insertTagHistory(CSSSelector::SubSelector, newSpecifier, CSSSelector::SubSelector);
        return specifiers;
    }
    specifiers->appendTagHistory(CSSSelector::SubSelector, newSpecifier);
    return specifiers;
}

} // namespace blink
