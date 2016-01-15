// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSelectorParser_h
#define CSSSelectorParser_h

#include "core/CoreExport.h"
#include "core/css/parser/CSSParserSelector.h"
#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

class CSSSelectorList;
class StyleSheetContents;

// FIXME: We should consider building CSSSelectors directly instead of using
// the intermediate CSSParserSelector.
class CORE_EXPORT CSSSelectorParser {
    STACK_ALLOCATED();
public:
    static CSSSelectorList parseSelector(CSSParserTokenRange, const CSSParserContext&, StyleSheetContents*);

    static bool consumeANPlusB(CSSParserTokenRange&, std::pair<int, int>&);

private:
    CSSSelectorParser(const CSSParserContext&, StyleSheetContents*);

    // These will all consume trailing comments if successful

    CSSSelectorList consumeComplexSelectorList(CSSParserTokenRange&);
    CSSSelectorList consumeCompoundSelectorList(CSSParserTokenRange&);

    PassOwnPtr<CSSParserSelector> consumeComplexSelector(CSSParserTokenRange&);
    PassOwnPtr<CSSParserSelector> consumeCompoundSelector(CSSParserTokenRange&);
    // This doesn't include element names, since they're handled specially
    PassOwnPtr<CSSParserSelector> consumeSimpleSelector(CSSParserTokenRange&);

    bool consumeName(CSSParserTokenRange&, AtomicString& name, AtomicString& namespacePrefix);

    // These will return nullptr when the selector is invalid
    PassOwnPtr<CSSParserSelector> consumeId(CSSParserTokenRange&);
    PassOwnPtr<CSSParserSelector> consumeClass(CSSParserTokenRange&);
    PassOwnPtr<CSSParserSelector> consumePseudo(CSSParserTokenRange&);
    PassOwnPtr<CSSParserSelector> consumeAttribute(CSSParserTokenRange&);

    CSSSelector::Relation consumeCombinator(CSSParserTokenRange&);
    CSSSelector::Match consumeAttributeMatch(CSSParserTokenRange&);
    CSSSelector::AttributeMatchType consumeAttributeFlags(CSSParserTokenRange&);

    const AtomicString& defaultNamespace() const;
    const AtomicString& determineNamespace(const AtomicString& prefix);
    void prependTypeSelectorIfNeeded(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector*);
    static PassOwnPtr<CSSParserSelector> addSimpleSelectorToCompound(PassOwnPtr<CSSParserSelector> compoundSelector, PassOwnPtr<CSSParserSelector> simpleSelector);
    static PassOwnPtr<CSSParserSelector> splitCompoundAtImplicitShadowCrossingCombinator(PassOwnPtr<CSSParserSelector> compoundSelector);

    const CSSParserContext& m_context;
    RawPtrWillBeMember<StyleSheetContents> m_styleSheet; // FIXME: Should be const

    bool m_failedParsing = false;
    bool m_disallowPseudoElements = false;

    class DisallowPseudoElementsScope {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(DisallowPseudoElementsScope);
    public:
        DisallowPseudoElementsScope(CSSSelectorParser* parser)
            : m_parser(parser), m_wasDisallowed(m_parser->m_disallowPseudoElements)
        {
            m_parser->m_disallowPseudoElements = true;
        }

        ~DisallowPseudoElementsScope()
        {
            m_parser->m_disallowPseudoElements = m_wasDisallowed;
        }

    private:
        CSSSelectorParser* m_parser;
        bool m_wasDisallowed;
    };
};

} // namespace

#endif // CSSSelectorParser_h
