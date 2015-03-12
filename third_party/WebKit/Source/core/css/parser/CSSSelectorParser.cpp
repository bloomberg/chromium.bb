// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSSelectorParser.h"

#include "core/css/CSSSelectorList.h"
#include "core/css/StyleSheetContents.h"
#include "core/frame/UseCounter.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void CSSSelectorParser::parseSelector(CSSParserTokenRange range, const CSSParserContext& context, const AtomicString& defaultNamespace, StyleSheetContents* styleSheet, CSSSelectorList& output)
{
    CSSSelectorParser parser(context, defaultNamespace, styleSheet);
    range.consumeWhitespaceAndComments();
    CSSSelectorList result;
    parser.consumeComplexSelectorList(range, result);
    if (range.atEnd()) {
        output.adopt(result);
        recordSelectorStats(context, output);
    }
    ASSERT(!(output.isValid() && parser.m_failedParsing));
}

CSSSelectorParser::CSSSelectorParser(const CSSParserContext& context, const AtomicString& defaultNamespace, StyleSheetContents* styleSheet)
: m_context(context)
, m_defaultNamespace(defaultNamespace)
, m_styleSheet(styleSheet)
, m_failedParsing(false)
{
}

void CSSSelectorParser::consumeComplexSelectorList(CSSParserTokenRange& range, CSSSelectorList& output)
{
    Vector<OwnPtr<CSSParserSelector>> selectorList;
    OwnPtr<CSSParserSelector> selector = consumeComplexSelector(range);
    if (!selector)
        return;
    selectorList.append(selector.release());
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespaceAndComments();
        selector = consumeComplexSelector(range);
        if (!selector)
            return;
        selectorList.append(selector.release());
    }

    if (!m_failedParsing)
        output.adoptSelectorVector(selectorList);
}

void CSSSelectorParser::consumeCompoundSelectorList(CSSParserTokenRange& range, CSSSelectorList& output)
{
    Vector<OwnPtr<CSSParserSelector>> selectorList;
    OwnPtr<CSSParserSelector> selector = consumeCompoundSelector(range);
    range.consumeWhitespaceAndComments();
    if (!selector)
        return;
    selectorList.append(selector.release());
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        // FIXME: This differs from the spec grammar:
        // Spec: compound_selector S* [ COMMA S* compound_selector ]* S*
        // Impl: compound_selector S* [ COMMA S* compound_selector S* ]*
        range.consumeIncludingWhitespaceAndComments();
        selector = consumeCompoundSelector(range);
        range.consumeWhitespaceAndComments();
        if (!selector)
            return;
        selectorList.append(selector.release());
    }

    if (!m_failedParsing)
        output.adoptSelectorVector(selectorList);
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeComplexSelector(CSSParserTokenRange& range)
{
    OwnPtr<CSSParserSelector> selector = consumeCompoundSelector(range);
    if (!selector)
        return nullptr;
    while (CSSSelector::Relation combinator = consumeCombinator(range)) {
        OwnPtr<CSSParserSelector> nextSelector = consumeCompoundSelector(range);
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

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeCompoundSelector(CSSParserTokenRange& range)
{
    OwnPtr<CSSParserSelector> selector;

    AtomicString namespacePrefix;
    AtomicString elementName;
    bool hasNamespace;
    if (!consumeName(range, elementName, namespacePrefix, hasNamespace)) {
        selector = consumeSimpleSelector(range);
        if (!selector)
            return nullptr;
    }
    if (m_context.isHTMLDocument())
        elementName = elementName.lower();

    while (OwnPtr<CSSParserSelector> nextSelector = consumeSimpleSelector(range)) {
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

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeSimpleSelector(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    OwnPtr<CSSParserSelector> selector;
    if (token.type() == HashToken)
        selector = consumeId(range);
    else if (token.type() == DelimiterToken && token.delimiter() == '.')
        selector = consumeClass(range);
    else if (token.type() == LeftBracketToken)
        selector = consumeAttribute(range);
    else if (token.type() == ColonToken)
        selector = consumePseudo(range);
    else
        return nullptr;
    if (!selector)
        m_failedParsing = true;
    return selector.release();
}

bool CSSSelectorParser::consumeName(CSSParserTokenRange& range, AtomicString& name, AtomicString& namespacePrefix, bool& hasNamespace)
{
    name = nullAtom;
    namespacePrefix = nullAtom;
    hasNamespace = false;

    const CSSParserToken& firstToken = range.peek();
    if (firstToken.type() == IdentToken) {
        name = firstToken.value();
        range.consumeIncludingComments();
    } else if (firstToken.type() == DelimiterToken && firstToken.delimiter() == '*') {
        name = starAtom;
        range.consumeIncludingComments();
    } else if (firstToken.type() == DelimiterToken && firstToken.delimiter() == '|') {
        // No namespace
    } else {
        return false;
    }

    if (range.peek().type() != DelimiterToken || range.peek().delimiter() != '|')
        return true;
    range.consumeIncludingComments();

    hasNamespace = true;
    namespacePrefix = name;
    const CSSParserToken& nameToken = range.consumeIncludingComments();
    if (nameToken.type() == IdentToken) {
        name = nameToken.value();
    } else if (nameToken.type() == DelimiterToken && nameToken.delimiter() == '*') {
        name = starAtom;
    } else {
        name = nullAtom;
        namespacePrefix = nullAtom;
        return false;
    }

    return true;
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeId(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == HashToken);
    if (range.peek().hashTokenType() != HashTokenId)
        return nullptr;
    OwnPtr<CSSParserSelector> selector = CSSParserSelector::create();
    selector->setMatch(CSSSelector::Id);
    const String& value = range.consumeIncludingComments().value();
    if (isQuirksModeBehavior(m_context.mode()))
        selector->setValue(AtomicString(value.lower()));
    else
        selector->setValue(AtomicString(value));
    return selector.release();
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeClass(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == DelimiterToken);
    ASSERT(range.peek().delimiter() == '.');
    range.consumeIncludingComments();
    if (range.peek().type() != IdentToken)
        return nullptr;
    OwnPtr<CSSParserSelector> selector = CSSParserSelector::create();
    selector->setMatch(CSSSelector::Class);
    const String& value = range.consumeIncludingComments().value();
    if (isQuirksModeBehavior(m_context.mode()))
        selector->setValue(AtomicString(value.lower()));
    else
        selector->setValue(AtomicString(value));
    return selector.release();
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumeAttribute(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == LeftBracketToken);
    CSSParserTokenRange block = range.consumeBlock();
    block.consumeWhitespaceAndComments();
    range.consumeComments();

    AtomicString namespacePrefix;
    AtomicString attributeName;
    bool hasNamespace;
    if (!consumeName(block, attributeName, namespacePrefix, hasNamespace))
        return nullptr;

    if (m_context.isHTMLDocument())
        attributeName = attributeName.lower();

    QualifiedName qualifiedName = hasNamespace
        ? determineNameInNamespace(namespacePrefix, attributeName)
        : QualifiedName(nullAtom, attributeName, nullAtom);

    OwnPtr<CSSParserSelector> selector = CSSParserSelector::create();

    if (block.atEnd()) {
        selector->setAttribute(qualifiedName, CSSSelector::CaseSensitive);
        selector->setMatch(CSSSelector::AttributeSet);
        return selector.release();
    }

    selector->setMatch(consumeAttributeMatch(block));

    const CSSParserToken& attributeValue = block.consumeIncludingWhitespaceAndComments();
    if (attributeValue.type() != IdentToken && attributeValue.type() != StringToken)
        return nullptr;
    selector->setValue(attributeValue.value());
    selector->setAttribute(qualifiedName, consumeAttributeFlags(block));

    if (!block.atEnd())
        return nullptr;
    return selector.release();
}

PassOwnPtr<CSSParserSelector> CSSSelectorParser::consumePseudo(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == ColonToken);
    range.consumeIncludingComments();

    int colons = 1;
    if (range.peek().type() == ColonToken) {
        range.consumeIncludingComments();
        colons++;
    }

    const CSSParserToken& token = range.peek();
    if (token.type() != IdentToken && token.type() != FunctionToken)
        return nullptr;

    OwnPtr<CSSParserSelector> selector = CSSParserSelector::create();
    selector->setMatch(colons == 1 ? CSSSelector::PseudoClass : CSSSelector::PseudoElement);
    selector->setValue(AtomicString(String(token.value()).lower()));

    if (token.type() == IdentToken) {
        range.consumeIncludingComments();
        if (selector->pseudoType() == CSSSelector::PseudoUnknown)
            return nullptr;
        return selector.release();
    }

    CSSParserTokenRange block = range.consumeBlock();
    block.consumeWhitespaceAndComments();
    range.consumeComments();

    if ((colons == 1
        && (token.valueEqualsIgnoringCase("host")
            || token.valueEqualsIgnoringCase("host-context")
            || token.valueEqualsIgnoringCase("-webkit-any")))
        || (colons == 2 && token.valueEqualsIgnoringCase("cue"))) {

        CSSSelectorList* selectorList = new CSSSelectorList();
        consumeCompoundSelectorList(block, *selectorList);
        if (!selectorList->isValid() || !block.atEnd())
            return nullptr;

        selector->setSelectorList(adoptPtr(selectorList));
        selector->pseudoType(); // FIXME: Do we need to force the pseudo type to be cached?
        ASSERT(selector->pseudoType() != CSSSelector::PseudoUnknown);
        return selector.release();
    }

    if (colons == 1 && token.valueEqualsIgnoringCase("not")) {
        OwnPtr<CSSParserSelector> innerSelector = consumeCompoundSelector(block);
        if (!innerSelector || !innerSelector->isSimple() || !block.atEnd())
            return nullptr;
        Vector<OwnPtr<CSSParserSelector>> selectorVector;
        selectorVector.append(innerSelector.release());
        selector->adoptSelectorVector(selectorVector);
        return selector.release();
    }

    if (colons == 1 && token.valueEqualsIgnoringCase("lang")) {
        // FIXME: CSS Selectors Level 4 allows :lang(*-foo)
        const CSSParserToken& ident = block.consumeIncludingWhitespaceAndComments();
        if (ident.type() != IdentToken || !block.atEnd())
            return nullptr;
        selector->setArgument(ident.value());
        selector->pseudoType(); // FIXME: Do we need to force the pseudo type to be cached?
        ASSERT(selector->pseudoType() == CSSSelector::PseudoLang);
        return selector.release();
    }

    if (colons == 1
        && (token.valueEqualsIgnoringCase("nth-child")
            || token.valueEqualsIgnoringCase("nth-last-child")
            || token.valueEqualsIgnoringCase("nth-of-type")
            || token.valueEqualsIgnoringCase("nth-last-of-type"))) {
        std::pair<int, int> ab;
        if (!consumeANPlusB(block, ab))
            return nullptr;
        block.consumeWhitespaceAndComments();
        if (!block.atEnd())
            return nullptr;
        // FIXME: We shouldn't serialize here and reparse in CSSSelector!
        // Serialization should be in CSSSelector::selectorText instead.
        int a = ab.first;
        int b = ab.second;
        String string;
        if (a == 0 && b == 0)
            string = "0";
        else if (a == 0)
            string = String::number(b);
        else if (b == 0)
            string = String::format("%dn", a);
        else if (ab.second < 0)
            string = String::format("%dn%d", a, b);
        else
            string = String::format("%dn+%d", a, b);
        selector->setArgument(AtomicString(string));
        selector->pseudoType(); // FIXME: Do we need to force the pseudo type to be cached?
        ASSERT(selector->pseudoType() != CSSSelector::PseudoUnknown);
        return selector.release();
    }

    return nullptr;
}

CSSSelector::Relation CSSSelectorParser::consumeCombinator(CSSParserTokenRange& range)
{
    CSSSelector::Relation fallbackResult = CSSSelector::SubSelector;
    while (range.peek().type() == WhitespaceToken || range.peek().type() == CommentToken) {
        if (range.consume().type() == WhitespaceToken)
            fallbackResult = CSSSelector::Descendant;
    }

    if (range.peek().type() != DelimiterToken)
        return fallbackResult;

    UChar delimiter = range.peek().delimiter();

    if (delimiter == '+' || delimiter == '~' || delimiter == '>') {
        range.consumeIncludingWhitespaceAndComments();
        if (delimiter == '+')
            return CSSSelector::DirectAdjacent;
        if (delimiter == '~')
            return CSSSelector::IndirectAdjacent;
        return CSSSelector::Child;
    }

    // Match /deep/
    if (delimiter != '/')
        return fallbackResult;
    range.consumeIncludingComments();
    const CSSParserToken& ident = range.consumeIncludingComments();
    if (ident.type() != IdentToken || !ident.valueEqualsIgnoringCase("deep"))
        m_failedParsing = true;
    const CSSParserToken& slash = range.consumeIncludingWhitespaceAndComments();
    if (slash.type() != DelimiterToken || slash.delimiter() != '/')
        m_failedParsing = true;
    return CSSSelector::ShadowDeep;
}

CSSSelector::Match CSSSelectorParser::consumeAttributeMatch(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.consumeIncludingWhitespaceAndComments();
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

CSSSelector::AttributeMatchType CSSSelectorParser::consumeAttributeFlags(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return CSSSelector::CaseSensitive;
    const CSSParserToken& flag = range.consumeIncludingWhitespaceAndComments();
    if (String(flag.value()) == "i") {
        if (RuntimeEnabledFeatures::cssAttributeCaseSensitivityEnabled() || isUASheetBehavior(m_context.mode()))
            return CSSSelector::CaseInsensitive;
    }
    m_failedParsing = true;
    return CSSSelector::CaseSensitive;
}

bool CSSSelectorParser::consumeANPlusB(CSSParserTokenRange& range, std::pair<int, int>& result)
{
    const CSSParserToken& token = range.consumeIncludingComments();
    if (token.type() == NumberToken && token.numericValueType() == IntegerValueType) {
        result = std::make_pair(0, static_cast<int>(token.numericValue()));
        return true;
    }
    if (token.type() == IdentToken) {
        if (token.valueEqualsIgnoringCase("odd")) {
            result = std::make_pair(2, 1);
            return true;
        }
        if (token.valueEqualsIgnoringCase("even")) {
            result = std::make_pair(2, 0);
            return true;
        }
    }

    // The 'n' will end up as part of an ident or dimension. For a valid <an+b>,
    // this will store a string of the form 'n', 'n-', or 'n-123'.
    String nString;

    if (token.type() == DelimiterToken && token.delimiter() == '+' && range.peek().type() == IdentToken) {
        result.first = 1;
        nString = range.consume().value();
    } else if (token.type() == DimensionToken && token.numericValueType() == IntegerValueType) {
        result.first = token.numericValue();
        nString = token.value();
    } else if (token.type() == IdentToken) {
        if (token.value()[0] == '-') {
            result.first = -1;
            nString = String(token.value()).substring(1);
        } else {
            result.first = 1;
            nString = token.value();
        }
    }

    range.consumeWhitespaceAndComments();

    if (nString.isEmpty() || !isASCIIAlphaCaselessEqual(nString[0], 'n'))
        return false;
    if (nString.length() > 1 && nString[1] != '-')
        return false;

    if (nString.length() > 2) {
        bool valid;
        result.second = nString.substring(1).toIntStrict(&valid);
        return valid;
    }

    NumericSign sign = nString.length() == 1 ? NoSign : MinusSign;
    if (sign == NoSign && range.peek().type() == DelimiterToken) {
        char delimiterSign = range.consumeIncludingWhitespaceAndComments().delimiter();
        if (delimiterSign == '+')
            sign = PlusSign;
        else if (delimiterSign == '-')
            sign = MinusSign;
        else
            return false;
    }

    if (sign == NoSign && range.peek().type() != NumberToken) {
        result.second = 0;
        return true;
    }

    const CSSParserToken& b = range.consume();
    if (b.type() != NumberToken || b.numericValueType() != IntegerValueType)
        return false;
    if ((b.numericSign() == NoSign) == (sign == NoSign))
        return false;
    result.second = b.numericValue();
    if (sign == MinusSign)
        result.second = -result.second;
    return true;
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

void CSSSelectorParser::recordSelectorStats(const CSSParserContext& context, const CSSSelectorList& selectorList)
{
    if (!context.useCounter())
        return;

    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(*selector)) {
        for (const CSSSelector* current = selector; current ; current = current->tagHistory()) {
            UseCounter::Feature feature = UseCounter::NumberOfFeatures;
            switch (current->pseudoType()) {
            case CSSSelector::PseudoUnresolved:
                feature = UseCounter::CSSSelectorPseudoUnresolved;
                break;
            case CSSSelector::PseudoShadow:
                feature = UseCounter::CSSSelectorPseudoShadow;
                break;
            case CSSSelector::PseudoContent:
                feature = UseCounter::CSSSelectorPseudoContent;
                break;
            case CSSSelector::PseudoHost:
                feature = UseCounter::CSSSelectorPseudoHost;
                break;
            case CSSSelector::PseudoHostContext:
                feature = UseCounter::CSSSelectorPseudoHostContext;
                break;
            case CSSSelector::PseudoFullScreenDocument:
                feature = UseCounter::CSSSelectorPseudoFullScreenDocument;
                break;
            case CSSSelector::PseudoFullScreenAncestor:
                feature = UseCounter::CSSSelectorPseudoFullScreenAncestor;
                break;
            case CSSSelector::PseudoFullScreen:
                feature = UseCounter::CSSSelectorPseudoFullScreen;
                break;
            default:
                break;
            }
            if (feature != UseCounter::NumberOfFeatures)
                context.useCounter()->count(feature);
            if (current->relation() == CSSSelector::ShadowDeep)
                context.useCounter()->count(UseCounter::CSSDeepCombinator);
            if (current->selectorList())
                recordSelectorStats(context, *current->selectorList());
        }
    }
}

} // namespace blink
