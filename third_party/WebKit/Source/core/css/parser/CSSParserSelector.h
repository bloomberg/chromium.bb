/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef CSSParserSelector_h
#define CSSParserSelector_h

#include "core/CoreExport.h"
#include "core/css/CSSSelector.h"

namespace blink {

class CORE_EXPORT CSSParserSelector {
    WTF_MAKE_NONCOPYABLE(CSSParserSelector); USING_FAST_MALLOC(CSSParserSelector);
public:
    CSSParserSelector();
    explicit CSSParserSelector(const QualifiedName&, bool isImplicit = false);

    static PassOwnPtr<CSSParserSelector> create() { return adoptPtr(new CSSParserSelector); }
    static PassOwnPtr<CSSParserSelector> create(const QualifiedName& name, bool isImplicit = false) { return adoptPtr(new CSSParserSelector(name, isImplicit)); }

    ~CSSParserSelector();

    PassOwnPtr<CSSSelector> releaseSelector() { return m_selector.release(); }

    CSSSelector::RelationType relation() const { return m_selector->relation(); }
    void setValue(const AtomicString& value, bool matchLowerCase = false) { m_selector->setValue(value, matchLowerCase); }
    void setAttribute(const QualifiedName& value, CSSSelector::AttributeMatchType matchType) { m_selector->setAttribute(value, matchType); }
    void setArgument(const AtomicString& value) { m_selector->setArgument(value); }
    void setNth(int a, int b) { m_selector->setNth(a, b); }
    void setMatch(CSSSelector::MatchType value) { m_selector->setMatch(value); }
    void setRelation(CSSSelector::RelationType value) { m_selector->setRelation(value); }
    void setForPage() { m_selector->setForPage(); }
    void setRelationIsAffectedByPseudoContent() { m_selector->setRelationIsAffectedByPseudoContent(); }
    bool relationIsAffectedByPseudoContent() const { return m_selector->relationIsAffectedByPseudoContent(); }

    void updatePseudoType(const AtomicString& value, bool hasArguments = false) const { m_selector->updatePseudoType(value, hasArguments); }

    void adoptSelectorVector(Vector<OwnPtr<CSSParserSelector>>& selectorVector);
    void setSelectorList(PassOwnPtr<CSSSelectorList>);

    bool isHostPseudoSelector() const;

    CSSSelector::MatchType match() const { return m_selector->match(); }
    CSSSelector::PseudoType pseudoType() const { return m_selector->getPseudoType(); }
    const CSSSelectorList* selectorList() const { return m_selector->selectorList(); }

    bool needsImplicitShadowCombinatorForMatching() const { return pseudoType() == CSSSelector::PseudoWebKitCustomElement || pseudoType() == CSSSelector::PseudoCue || pseudoType() == CSSSelector::PseudoShadow || pseudoType() == CSSSelector::PseudoSlotted; }

    bool isSimple() const;

    CSSParserSelector* tagHistory() const { return m_tagHistory.get(); }
    void setTagHistory(PassOwnPtr<CSSParserSelector> selector) { m_tagHistory = std::move(selector); }
    void clearTagHistory() { m_tagHistory.clear(); }
    void appendTagHistory(CSSSelector::RelationType, PassOwnPtr<CSSParserSelector>);
    PassOwnPtr<CSSParserSelector> releaseTagHistory();
    void prependTagSelector(const QualifiedName&, bool tagIsImplicit = false);

private:
    OwnPtr<CSSSelector> m_selector;
    OwnPtr<CSSParserSelector> m_tagHistory;
};

} // namespace blink

#endif
