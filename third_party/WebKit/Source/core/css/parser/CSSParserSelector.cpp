/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/css/parser/CSSParserSelector.h"

#include "core/css/CSSSelectorList.h"

namespace blink {

CSSParserSelector::CSSParserSelector()
    : m_selector(adoptPtr(new CSSSelector()))
{
}

CSSParserSelector::CSSParserSelector(const QualifiedName& tagQName, bool isImplicit)
    : m_selector(adoptPtr(new CSSSelector(tagQName, isImplicit)))
{
}

CSSParserSelector::~CSSParserSelector()
{
    if (!m_tagHistory)
        return;
    Vector<OwnPtr<CSSParserSelector>, 16> toDelete;
    OwnPtr<CSSParserSelector> selector = m_tagHistory.release();
    while (true) {
        OwnPtr<CSSParserSelector> next = selector->m_tagHistory.release();
        toDelete.append(selector.release());
        if (!next)
            break;
        selector = next.release();
    }
}

void CSSParserSelector::adoptSelectorVector(Vector<OwnPtr<CSSParserSelector>>& selectorVector)
{
    CSSSelectorList* selectorList = new CSSSelectorList(CSSSelectorList::adoptSelectorVector(selectorVector));
    m_selector->setSelectorList(adoptPtr(selectorList));
}

void CSSParserSelector::setSelectorList(PassOwnPtr<CSSSelectorList> selectorList)
{
    m_selector->setSelectorList(selectorList);
}

bool CSSParserSelector::isSimple() const
{
    if (m_selector->selectorList() || m_selector->match() == CSSSelector::PseudoElement)
        return false;

    if (!m_tagHistory)
        return true;

    if (m_selector->match() == CSSSelector::Tag) {
        // We can't check against anyQName() here because namespace may not be nullAtom.
        // Example:
        //     @namespace "http://www.w3.org/2000/svg";
        //     svg:not(:root) { ...
        if (m_selector->tagQName().localName() == starAtom)
            return m_tagHistory->isSimple();
    }

    return false;
}

void CSSParserSelector::appendTagHistory(CSSSelector::Relation relation, PassOwnPtr<CSSParserSelector> selector)
{
    CSSParserSelector* end = this;
    while (end->tagHistory())
        end = end->tagHistory();
    end->setRelation(relation);
    end->setTagHistory(selector);
}

PassOwnPtr<CSSParserSelector> CSSParserSelector::releaseTagHistory()
{
    setRelation(CSSSelector::SubSelector);
    return m_tagHistory.release();
}

void CSSParserSelector::prependTagSelector(const QualifiedName& tagQName, bool isImplicit)
{
    OwnPtr<CSSParserSelector> second = CSSParserSelector::create();
    second->m_selector = m_selector.release();
    second->m_tagHistory = m_tagHistory.release();
    m_tagHistory = second.release();
    m_selector = adoptPtr(new CSSSelector(tagQName, isImplicit));
}

bool CSSParserSelector::isHostPseudoSelector() const
{
    return pseudoType() == CSSSelector::PseudoHost || pseudoType() == CSSSelector::PseudoHostContext;
}

} // namespace blink
