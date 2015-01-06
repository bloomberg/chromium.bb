// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSelectorParser_h
#define CSSSelectorParser_h

#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSParserValues.h"

namespace blink {

class CSSSelectorList;
class StyleSheetContents;

// FIXME: We should consider building CSSSelectors directly instead of using
// the intermediate CSSParserSelector.
class CSSSelectorParser {
public:
    static void parseSelector(CSSParserTokenRange, const CSSParserContext&, const AtomicString& defaultNamespace, StyleSheetContents*, CSSSelectorList&);

private:
    CSSSelectorParser(CSSParserTokenRange, const CSSParserContext&, const AtomicString& defaultNamespace, StyleSheetContents*);

    // These will all consume trailing comments if successful

    void consumeComplexSelectorList(CSSSelectorList&);
    void consumeCompoundSelectorList(CSSSelectorList&);

    PassOwnPtr<CSSParserSelector> consumeComplexSelector();
    PassOwnPtr<CSSParserSelector> consumeCompoundSelector();
    // This doesn't include element names, since they're handled specially
    PassOwnPtr<CSSParserSelector> consumeSimpleSelector();

    bool consumeName(AtomicString& name, AtomicString& namespacePrefix, bool& hasNamespace);

    // These will return nullptr when the selector is invalid
    PassOwnPtr<CSSParserSelector> consumeId();
    PassOwnPtr<CSSParserSelector> consumeClass();
    PassOwnPtr<CSSParserSelector> consumePseudo();
    PassOwnPtr<CSSParserSelector> consumeAttribute();

    CSSSelector::Relation consumeCombinator();
    CSSSelector::Match consumeAttributeMatch();
    CSSSelector::AttributeMatchType consumeAttributeFlags();

    QualifiedName determineNameInNamespace(const AtomicString& prefix, const AtomicString& localName);
    void rewriteSpecifiersWithNamespaceIfNeeded(CSSParserSelector*);
    void rewriteSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector*, bool tagIsForNamespaceRule = false);
    void rewriteSpecifiersWithElementNameForCustomPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector*, bool tagIsForNamespaceRule);
    void rewriteSpecifiersWithElementNameForContentPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector*, bool tagIsForNamespaceRule);
    PassOwnPtr<CSSParserSelector> rewriteSpecifiers(PassOwnPtr<CSSParserSelector> specifiers, PassOwnPtr<CSSParserSelector> newSpecifier);

    CSSParserTokenRange m_tokenRange;

    const CSSParserContext& m_context;
    AtomicString m_defaultNamespace;
    StyleSheetContents* m_styleSheet; // FIXME: Should be const

    bool m_failedParsing;
};

} // namespace

#endif // CSSSelectorParser_h
