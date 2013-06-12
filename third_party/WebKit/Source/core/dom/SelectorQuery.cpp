/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/SelectorQuery.h"

#include "core/css/CSSParser.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorCheckerFastPath.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/dom/Document.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/StaticNodeList.h"

namespace WebCore {

void SelectorDataList::initialize(const CSSSelectorList& selectorList)
{
    ASSERT(m_selectors.isEmpty());

    unsigned selectorCount = 0;
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        selectorCount++;

    m_selectors.reserveInitialCapacity(selectorCount);
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
        m_selectors.uncheckedAppend(SelectorData(selector, SelectorCheckerFastPath::canUse(selector)));
}

inline bool SelectorDataList::selectorMatches(const SelectorData& selectorData, Element* element, const Node* rootNode) const
{
    if (selectorData.isFastCheckable && !element->isSVGElement()) {
        SelectorCheckerFastPath selectorCheckerFastPath(selectorData.selector, element);
        if (!selectorCheckerFastPath.matchesRightmostSelector(SelectorChecker::VisitedMatchDisabled))
            return false;
        return selectorCheckerFastPath.matches();
    }

    SelectorChecker selectorChecker(element->document(), SelectorChecker::QueryingRules);
    SelectorChecker::SelectorCheckingContext selectorCheckingContext(selectorData.selector, element, SelectorChecker::VisitedMatchDisabled);
    selectorCheckingContext.behaviorAtBoundary = SelectorChecker::StaysWithinTreeScope;
    selectorCheckingContext.scope = !rootNode->isDocumentNode() && rootNode->isContainerNode() ? toContainerNode(rootNode) : 0;
    PseudoId ignoreDynamicPseudo = NOPSEUDO;
    return selectorChecker.match(selectorCheckingContext, ignoreDynamicPseudo, DOMSiblingTraversalStrategy()) == SelectorChecker::SelectorMatches;
}

bool SelectorDataList::matches(Element* targetElement) const
{
    ASSERT(targetElement);

    unsigned selectorCount = m_selectors.size();
    for (unsigned i = 0; i < selectorCount; ++i) {
        if (selectorMatches(m_selectors[i], targetElement, targetElement))
            return true;
    }

    return false;
}

PassRefPtr<NodeList> SelectorDataList::queryAll(Node* rootNode) const
{
    Vector<RefPtr<Node> > result;
    execute<false>(rootNode, result);
    return StaticNodeList::adopt(result);
}

PassRefPtr<Element> SelectorDataList::queryFirst(Node* rootNode) const
{
    Vector<RefPtr<Node> > result;
    execute<true>(rootNode, result);
    if (result.isEmpty())
        return 0;
    ASSERT(result.size() == 1);
    ASSERT(result.first()->isElementNode());
    return toElement(result.first().get());
}

static inline bool isTreeScopeRoot(Node* node)
{
    ASSERT(node);
    return node->isDocumentNode() || node->isShadowRoot();
}

// If the first pair value is true, the returned Node is the single Element that may match the selector query.
//
// If the first value is false, the returned Node is the rootNode parameter or a descendant of rootNode representing
// the subtree for which we can limit the querySelector traversal.
//
// The returned Node may be 0, regardless of the returned bool value, if this method finds that the selectors won't
// match any element.
std::pair<bool, Node*> SelectorDataList::findTraverseRoot(Node* rootNode) const
{
    // We need to return the matches in document order. To use id lookup while there is possiblity of multiple matches
    // we would need to sort the results. For now, just traverse the document in that case.
    if (m_selectors.size() != 1)
        return std::make_pair(false, rootNode);
    if (!rootNode->inDocument())
        return std::make_pair(false, rootNode);
    if (rootNode->document()->inQuirksMode())
        return std::make_pair(false, rootNode);

    bool matchSingleNode = true;
    bool startFromParent = false;
    for (const CSSSelector* selector = m_selectors[0].selector; selector; selector = selector->tagHistory()) {
        if (selector->m_match == CSSSelector::Id && !rootNode->document()->containsMultipleElementsWithId(selector->value())) {
            Element* element = rootNode->treeScope()->getElementById(selector->value());
            if (element && (isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)))
                rootNode = element;
            else if (!element || matchSingleNode)
                rootNode = 0;
            if (matchSingleNode)
                return std::make_pair(true, rootNode);
            if (startFromParent && rootNode)
                rootNode = rootNode->parentNode();
            return std::make_pair(false, rootNode);
        }
        if (selector->relation() == CSSSelector::SubSelector)
            continue;
        matchSingleNode = false;
        if (selector->relation() == CSSSelector::DirectAdjacent || selector->relation() == CSSSelector::IndirectAdjacent)
            startFromParent = true;
        else
            startFromParent = false;
    }
    return std::make_pair(false, rootNode);
}

template <bool firstMatchOnly>
void SelectorDataList::execute(Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const
{
    std::pair<bool, Node*> traverseRoot = findTraverseRoot(rootNode);
    if (!traverseRoot.second)
        return;
    Node* traverseRootNode = traverseRoot.second;
    if (traverseRoot.first) {
        ASSERT(m_selectors.size() == 1);
        ASSERT(traverseRootNode->isElementNode());
        Element* element = toElement(traverseRootNode);
        if (selectorMatches(m_selectors[0], element, rootNode))
            matchedElements.append(element);
        return;
    }

    unsigned selectorCount = m_selectors.size();
    if (selectorCount == 1) {
        const SelectorData& selector = m_selectors[0];
        for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(element, rootNode)) {
            if (selectorMatches(selector, element, rootNode)) {
                matchedElements.append(element);
                if (firstMatchOnly)
                    return;
            }
        }
        return;
    }
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(element, rootNode)) {
        for (unsigned i = 0; i < selectorCount; ++i) {
            if (selectorMatches(m_selectors[i], element, rootNode)) {
                matchedElements.append(element);
                if (firstMatchOnly)
                    return;
                break;
            }
        }
    }
}

SelectorQuery::SelectorQuery(const CSSSelectorList& selectorList)
    : m_selectorList(selectorList)
{
    m_selectors.initialize(m_selectorList);
}

bool SelectorQuery::matches(Element* element) const
{
    return m_selectors.matches(element);
}

PassRefPtr<NodeList> SelectorQuery::queryAll(Node* rootNode) const
{
    return m_selectors.queryAll(rootNode);
}

PassRefPtr<Element> SelectorQuery::queryFirst(Node* rootNode) const
{
    return m_selectors.queryFirst(rootNode);
}

SelectorQuery* SelectorQueryCache::add(const AtomicString& selectors, Document* document, ExceptionCode& ec)
{
    HashMap<AtomicString, OwnPtr<SelectorQuery> >::iterator it = m_entries.find(selectors);
    if (it != m_entries.end())
        return it->value.get();

    CSSParser parser(document);
    CSSSelectorList selectorList;
    parser.parseSelector(selectors, selectorList);

    if (!selectorList.first() || selectorList.hasInvalidSelector()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    // throw a NAMESPACE_ERR if the selector includes any namespace prefixes.
    if (selectorList.selectorsNeedNamespaceResolution()) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    const int maximumSelectorQueryCacheSize = 256;
    if (m_entries.size() == maximumSelectorQueryCacheSize)
        m_entries.remove(m_entries.begin());
    
    OwnPtr<SelectorQuery> selectorQuery = adoptPtr(new SelectorQuery(selectorList));
    SelectorQuery* rawSelectorQuery = selectorQuery.get();
    m_entries.add(selectors, selectorQuery.release());
    return rawSelectorQuery;
}

void SelectorQueryCache::invalidate()
{
    m_entries.clear();
}

}
