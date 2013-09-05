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

#include "bindings/v8/ExceptionState.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorCheckerFastPath.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StaticNodeList.h"

namespace WebCore {

class SimpleNodeList {
public:
    virtual ~SimpleNodeList() { }
    virtual bool isEmpty() const = 0;
    virtual Node* next() = 0;
};

class SingleNodeList : public SimpleNodeList {
public:
    explicit SingleNodeList(Node* rootNode) : m_currentNode(rootNode) { }

    bool isEmpty() const { return !m_currentNode; }

    Node* next()
    {
        Node* current = m_currentNode;
        m_currentNode = 0;
        return current;
    }

private:
    Node* m_currentNode;
};

class ClassRootNodeList : public SimpleNodeList {
public:
    explicit ClassRootNodeList(Node* rootNode, const AtomicString& className)
        : m_className(className)
        , m_rootNode(rootNode)
        , m_currentElement(nextInternal(ElementTraversal::firstWithin(rootNode))) { }

    bool isEmpty() const { return !m_currentElement; }

    Node* next()
    {
        Node* current = m_currentElement;
        ASSERT(current);
        m_currentElement = nextInternal(ElementTraversal::nextSkippingChildren(m_currentElement, m_rootNode));
        return current;
    }

private:
    Element* nextInternal(Element* element)
    {
        for (; element; element = ElementTraversal::next(element, m_rootNode)) {
            if (element->hasClass() && element->classNames().contains(m_className))
                return element;
        }
        return 0;
    }

    const AtomicString& m_className;
    Node* m_rootNode;
    Element* m_currentElement;
};

class ClassElementList : public SimpleNodeList {
public:
    explicit ClassElementList(Node* rootNode, const AtomicString& className)
        : m_className(className)
        , m_rootNode(rootNode)
        , m_currentElement(nextInternal(ElementTraversal::firstWithin(rootNode))) { }

    bool isEmpty() const { return !m_currentElement; }

    Node* next()
    {
        Node* current = m_currentElement;
        ASSERT(current);
        m_currentElement = nextInternal(ElementTraversal::next(m_currentElement, m_rootNode));
        return current;
    }

private:
    Element* nextInternal(Element* element)
    {
        for (; element; element = ElementTraversal::next(element, m_rootNode)) {
            if (element->hasClass() && element->classNames().contains(m_className))
                return element;
        }
        return 0;
    }

    const AtomicString& m_className;
    Node* m_rootNode;
    Element* m_currentElement;
};

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
    executeQueryAll(rootNode, result);
    return StaticNodeList::adopt(result);
}

PassRefPtr<Element> SelectorDataList::queryFirst(Node* rootNode) const
{
    return executeQueryFirst(rootNode);
}

static inline bool isTreeScopeRoot(Node* node)
{
    ASSERT(node);
    return node->isDocumentNode() || node->isShadowRoot();
}

void SelectorDataList::collectElementsByClassName(Node* rootNode, const AtomicString& className, Vector<RefPtr<Node> >& traversalRoots) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(element, rootNode)) {
        if (element->hasClass() && element->classNames().contains(className))
            traversalRoots.append(element);
    }
}

void SelectorDataList::collectElementsByTagName(Node* rootNode, const QualifiedName& tagName, Vector<RefPtr<Node> >& traversalRoots) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(element, rootNode)) {
        if (SelectorChecker::tagMatches(element, tagName))
            traversalRoots.append(element);
    }
}

Element* SelectorDataList::findElementByClassName(Node* rootNode, const AtomicString& className) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(element, rootNode)) {
        if (element->hasClass() && element->classNames().contains(className))
            return element;
    }
    return 0;
}

Element* SelectorDataList::findElementByTagName(Node* rootNode, const QualifiedName& tagName) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(element, rootNode)) {
        if (SelectorChecker::tagMatches(element, tagName))
            return element;
    }
    return 0;
}

inline bool SelectorDataList::canUseFastQuery(Node* rootNode) const
{
    return m_selectors.size() == 1 && rootNode->inDocument() && !rootNode->document().inQuirksMode();
}

// If returns true, traversalRoots has the elements that may match the selector query.
//
// If returns false, traversalRoots has the rootNode parameter or descendants of rootNode representing
// the subtree for which we can limit the querySelector traversal.
//
// The travseralRoots may be empty, regardless of the returned bool value, if this method finds that the selectors won't
// match any element.
PassOwnPtr<SimpleNodeList> SelectorDataList::findTraverseRoots(Node* rootNode, bool& matchTraverseRoots) const
{
    // We need to return the matches in document order. To use id lookup while there is possiblity of multiple matches
    // we would need to sort the results. For now, just traverse the document in that case.
    ASSERT(rootNode);
    ASSERT(m_selectors.size() == 1);
    ASSERT(m_selectors[0].selector);

    bool isRightmostSelector = true;
    bool startFromParent = false;

    for (const CSSSelector* selector = m_selectors[0].selector; selector; selector = selector->tagHistory()) {
        if (selector->m_match == CSSSelector::Id && !rootNode->document().containsMultipleElementsWithId(selector->value())) {
            Element* element = rootNode->treeScope().getElementById(selector->value());
            if (element && (isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)))
                rootNode = element;
            else if (!element || isRightmostSelector)
                rootNode = 0;
            if (isRightmostSelector) {
                matchTraverseRoots = true;
                return adoptPtr(new SingleNodeList(rootNode));
            }
            if (startFromParent && rootNode)
                rootNode = rootNode->parentNode();

            matchTraverseRoots = false;
            return adoptPtr(new SingleNodeList(rootNode));
        }

        // If we have both CSSSelector::Id and CSSSelector::Class at the same time, we should use Id
        // to find traverse root.
        if (!startFromParent && selector->m_match == CSSSelector::Class) {
            if (isRightmostSelector) {
                matchTraverseRoots = true;
                return adoptPtr(new ClassElementList(rootNode, selector->value()));
            }
            matchTraverseRoots = false;
            return adoptPtr(new ClassRootNodeList(rootNode, selector->value()));
        }

        if (selector->relation() == CSSSelector::SubSelector)
            continue;
        isRightmostSelector = false;
        if (selector->relation() == CSSSelector::DirectAdjacent || selector->relation() == CSSSelector::IndirectAdjacent)
            startFromParent = true;
        else
            startFromParent = false;
    }

    matchTraverseRoots = false;
    return adoptPtr(new SingleNodeList(rootNode));
}

void SelectorDataList::executeSlowQueryAll(Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(element, rootNode)) {
        for (unsigned i = 0; i < m_selectors.size(); ++i) {
            if (selectorMatches(m_selectors[i], element, rootNode)) {
                matchedElements.append(element);
                break;
            }
        }
    }
}

void SelectorDataList::executeQueryAll(Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const
{
    if (!canUseFastQuery(rootNode))
        return executeSlowQueryAll(rootNode, matchedElements);

    ASSERT(m_selectors.size() == 1);
    ASSERT(m_selectors[0].selector);

    const CSSSelector* firstSelector = m_selectors[0].selector;

    if (!firstSelector->tagHistory()) {
        // Fast path for querySelectorAll('#id'), querySelectorAl('.foo'), and querySelectorAll('div').
        switch (firstSelector->m_match) {
        case CSSSelector::Id:
            {
                if (rootNode->document().containsMultipleElementsWithId(firstSelector->value()))
                    break;

                // Just the same as getElementById.
                Element* element = rootNode->treeScope().getElementById(firstSelector->value());
                if (element && (isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)))
                    matchedElements.append(element);
                return;
            }
        case CSSSelector::Class:
            return collectElementsByClassName(rootNode, firstSelector->value(), matchedElements);
        case CSSSelector::Tag:
            return collectElementsByTagName(rootNode, firstSelector->tagQName(), matchedElements);
        default:
            break; // If we need another fast path, add here.
        }
    }

    bool matchTraverseRoots;
    OwnPtr<SimpleNodeList> traverseRoots = findTraverseRoots(rootNode, matchTraverseRoots);
    if (traverseRoots->isEmpty())
        return;

    const SelectorData& selector = m_selectors[0];
    if (matchTraverseRoots) {
        while (!traverseRoots->isEmpty()) {
            Node* node = traverseRoots->next();
            Element* element = toElement(node);
            if (selectorMatches(selector, element, rootNode))
                matchedElements.append(element);
        }
        return;
    }

    while (!traverseRoots->isEmpty()) {
        Node* traverseRoot = traverseRoots->next();
        for (Element* element = ElementTraversal::firstWithin(traverseRoot); element; element = ElementTraversal::next(element, traverseRoot)) {
            if (selectorMatches(selector, element, rootNode))
                matchedElements.append(element);
        }
    }
}

// If matchTraverseRoot is true, the returned Node is the single Element that may match the selector query.
//
// If matchTraverseRoot is false, the returned Node is the rootNode parameter or a descendant of rootNode representing
// the subtree for which we can limit the querySelector traversal.
//
// The returned Node may be 0, regardless of matchTraverseRoot, if this method finds that the selectors won't
// match any element.
Node* SelectorDataList::findTraverseRoot(Node* rootNode, bool& matchTraverseRoot) const
{
    // We need to return the matches in document order. To use id lookup while there is possiblity of multiple matches
    // we would need to sort the results. For now, just traverse the document in that case.
    ASSERT(rootNode);
    ASSERT(m_selectors.size() == 1);
    ASSERT(m_selectors[0].selector);

    bool matchSingleNode = true;
    bool startFromParent = false;
    for (const CSSSelector* selector = m_selectors[0].selector; selector; selector = selector->tagHistory()) {
        if (selector->m_match == CSSSelector::Id && !rootNode->document().containsMultipleElementsWithId(selector->value())) {
            Element* element = rootNode->treeScope().getElementById(selector->value());
            if (element && (isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)))
                rootNode = element;
            else if (!element || matchSingleNode)
                rootNode = 0;
            if (matchSingleNode) {
                matchTraverseRoot = true;
                return rootNode;
            }
            if (startFromParent && rootNode)
                rootNode = rootNode->parentNode();
            matchTraverseRoot = false;
            return rootNode;
        }
        if (selector->relation() == CSSSelector::SubSelector)
            continue;
        matchSingleNode = false;
        if (selector->relation() == CSSSelector::DirectAdjacent || selector->relation() == CSSSelector::IndirectAdjacent)
            startFromParent = true;
        else
            startFromParent = false;
    }
    matchTraverseRoot = false;
    return rootNode;
}

Element* SelectorDataList::executeSlowQueryFirst(Node* rootNode) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(element, rootNode)) {
        for (unsigned i = 0; i < m_selectors.size(); ++i) {
            if (selectorMatches(m_selectors[i], element, rootNode))
                return element;
        }
    }
    return 0;
}

Element* SelectorDataList::executeQueryFirst(Node* rootNode) const
{
    if (!canUseFastQuery(rootNode))
        return executeSlowQueryFirst(rootNode);


    const CSSSelector* selector = m_selectors[0].selector;
    ASSERT(selector);

    if (!selector->tagHistory()) {
        // Fast path for querySelector('#id'), querySelector('.foo'), and querySelector('div').
        // Many web developers uses querySelector with these simple selectors.
        switch (selector->m_match) {
        case CSSSelector::Id:
            {
                if (rootNode->document().containsMultipleElementsWithId(selector->value()))
                    break;
                Element* element = rootNode->treeScope().getElementById(selector->value());
                return element && (isTreeScopeRoot(rootNode) || element->isDescendantOf(rootNode)) ? element : 0;
            }
        case CSSSelector::Class:
            return findElementByClassName(rootNode, selector->value());
        case CSSSelector::Tag:
            return findElementByTagName(rootNode, selector->tagQName());
        default:
            break; // If we need another fast path, add here.
        }
    }

    bool matchTraverseRoot;
    Node* traverseRootNode = findTraverseRoot(rootNode, matchTraverseRoot);
    if (!traverseRootNode)
        return 0;
    if (matchTraverseRoot) {
        ASSERT(m_selectors.size() == 1);
        ASSERT(traverseRootNode->isElementNode());
        Element* element = toElement(traverseRootNode);
        return selectorMatches(m_selectors[0], element, rootNode) ? element : 0;
    }

    for (Element* element = ElementTraversal::firstWithin(traverseRootNode); element; element = ElementTraversal::next(element, traverseRootNode)) {
        if (selectorMatches(m_selectors[0], element, rootNode))
            return element;
    }
    return 0;
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

SelectorQuery* SelectorQueryCache::add(const AtomicString& selectors, const Document& document, ExceptionState& es)
{
    HashMap<AtomicString, OwnPtr<SelectorQuery> >::iterator it = m_entries.find(selectors);
    if (it != m_entries.end())
        return it->value.get();

    CSSParser parser(document);
    CSSSelectorList selectorList;
    parser.parseSelector(selectors, selectorList);

    if (!selectorList.first()) {
        es.throwDOMException(SyntaxError, "Failed to execute query: '" + selectors + "' is not a valid selector.");
        return 0;
    }

    // throw a NamespaceError if the selector includes any namespace prefixes.
    if (selectorList.selectorsNeedNamespaceResolution()) {
        es.throwDOMException(NamespaceError, "Failed to execute query: '" + selectors + "' contains namespaces, which are not supported.");
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
