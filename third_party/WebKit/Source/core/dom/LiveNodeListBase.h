/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
 *
 */

#ifndef LiveNodeListBase_h
#define LiveNodeListBase_h

#include "HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/html/CollectionType.h"

namespace WebCore {

enum NodeListRootType {
    NodeListIsRootedAtNode,
    NodeListIsRootedAtDocument
};

class LiveNodeListBase {
public:
    LiveNodeListBase(ContainerNode& ownerNode, NodeListRootType rootType, NodeListInvalidationType invalidationType,
        CollectionType collectionType)
        : m_ownerNode(ownerNode)
        , m_rootType(rootType)
        , m_invalidationType(invalidationType)
        , m_collectionType(collectionType)
    {
        ASSERT(m_rootType == static_cast<unsigned>(rootType));
        ASSERT(m_invalidationType == static_cast<unsigned>(invalidationType));
        ASSERT(m_collectionType == static_cast<unsigned>(collectionType));

        document().registerNodeList(this);
    }

    virtual ~LiveNodeListBase()
    {
        document().unregisterNodeList(this);
    }

    ContainerNode& rootNode() const;

    void didMoveToDocument(Document& oldDocument, Document& newDocument);
    ALWAYS_INLINE bool hasIdNameCache() const { return !isLiveNodeListType(type()); }
    ALWAYS_INLINE bool isRootedAtDocument() const { return m_rootType == NodeListIsRootedAtDocument; }
    ALWAYS_INLINE NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    ALWAYS_INLINE CollectionType type() const { return static_cast<CollectionType>(m_collectionType); }
    ContainerNode& ownerNode() const { return *m_ownerNode; }
    ALWAYS_INLINE void invalidateCache(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache();
        else if (hasIdNameCache() && (*attrName == HTMLNames::idAttr || *attrName == HTMLNames::nameAttr))
            invalidateIdNameCacheMaps();
    }
    virtual void invalidateCache(Document* oldDocument = 0) const = 0;

    static bool shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType, const QualifiedName&);

protected:
    Document& document() const { return m_ownerNode->document(); }

    ALWAYS_INLINE NodeListRootType rootType() const { return static_cast<NodeListRootType>(m_rootType); }

    template <typename Collection>
    static Element* itemBefore(const Collection&, const Element* previousItem);
    template <class NodeListType>
    static Element* firstMatchingElement(const NodeListType&);
    template <class NodeListType>
    static Element* nextMatchingElement(const NodeListType&, Element& current);
    template <class NodeListType>
    static Element* traverseMatchingElementsForwardToOffset(const NodeListType&, unsigned offset, Element& currentElement, unsigned& currentOffset);

private:
    void invalidateIdNameCacheMaps() const;
    template <typename Collection>
    static Element* iterateForPreviousNode(const Collection&, Node* current);
    static Node* previousNode(const ContainerNode&, const Node& previous, bool onlyIncludeDirectChildren);
    static Node* lastDescendant(const ContainerNode&);
    static Node* lastNode(const ContainerNode&, bool onlyIncludeDirectChildren);

    RefPtr<ContainerNode> m_ownerNode; // Cannot be null.
    const unsigned m_rootType : 1;
    const unsigned m_invalidationType : 4;
    const unsigned m_collectionType : 5;
};

ALWAYS_INLINE bool LiveNodeListBase::shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType type, const QualifiedName& attrName)
{
    switch (type) {
    case InvalidateOnClassAttrChange:
        return attrName == HTMLNames::classAttr;
    case InvalidateOnNameAttrChange:
        return attrName == HTMLNames::nameAttr;
    case InvalidateOnIdNameAttrChange:
        return attrName == HTMLNames::idAttr || attrName == HTMLNames::nameAttr;
    case InvalidateOnForAttrChange:
        return attrName == HTMLNames::forAttr;
    case InvalidateForFormControls:
        return attrName == HTMLNames::nameAttr || attrName == HTMLNames::idAttr || attrName == HTMLNames::forAttr
            || attrName == HTMLNames::formAttr || attrName == HTMLNames::typeAttr;
    case InvalidateOnHRefAttrChange:
        return attrName == HTMLNames::hrefAttr;
    case DoNotInvalidateOnAttributeChanges:
        return false;
    case InvalidateOnAnyAttrChange:
        return true;
    }
    return false;
}

inline Node* LiveNodeListBase::previousNode(const ContainerNode& base, const Node& previous, bool onlyIncludeDirectChildren)
{
    return onlyIncludeDirectChildren ? previous.previousSibling() : NodeTraversal::previous(previous, &base);
}

inline Node* LiveNodeListBase::lastDescendant(const ContainerNode& node)
{
    Node* descendant = node.lastChild();
    for (Node* current = descendant; current; current = current->lastChild())
        descendant = current;
    return descendant;
}

inline Node* LiveNodeListBase::lastNode(const ContainerNode& rootNode, bool onlyIncludeDirectChildren)
{
    return onlyIncludeDirectChildren ? rootNode.lastChild() : lastDescendant(rootNode);
}

template <typename Collection>
Element* LiveNodeListBase::iterateForPreviousNode(const Collection& collection, Node* current)
{
    bool onlyIncludeDirectChildren = collection.shouldOnlyIncludeDirectChildren();
    ContainerNode& rootNode = collection.rootNode();
    for (; current; current = previousNode(rootNode, *current, onlyIncludeDirectChildren)) {
        if (current->isElementNode() && isMatchingElement(collection, toElement(*current)))
            return toElement(current);
    }
    return 0;
}

template <typename Collection>
Element* LiveNodeListBase::itemBefore(const Collection& collection, const Element* previous)
{
    Node* current;
    if (LIKELY(!!previous)) // Without this LIKELY, length() and item() can be 10% slower.
        current = previousNode(collection.rootNode(), *previous, collection.shouldOnlyIncludeDirectChildren());
    else
        current = lastNode(collection.rootNode(), collection.shouldOnlyIncludeDirectChildren());

    return iterateForPreviousNode(collection, current);
}

template <class NodeListType>
Element* LiveNodeListBase::firstMatchingElement(const NodeListType& nodeList)
{
    ContainerNode& root = nodeList.rootNode();
    Element* element = ElementTraversal::firstWithin(root);
    while (element && !isMatchingElement(nodeList, *element))
        element = ElementTraversal::next(*element, &root);
    return element;
}

template <class NodeListType>
Element* LiveNodeListBase::nextMatchingElement(const NodeListType& nodeList, Element& current)
{
    ContainerNode& root = nodeList.rootNode();
    Element* next = &current;
    do {
        next = ElementTraversal::next(*next, &root);
    } while (next && !isMatchingElement(nodeList, *next));
    return next;
}

template <class NodeListType>
Element* LiveNodeListBase::traverseMatchingElementsForwardToOffset(const NodeListType& nodeList, unsigned offset, Element& currentElement, unsigned& currentOffset)
{
    ASSERT(currentOffset < offset);
    Element* next = &currentElement;
    while ((next = nextMatchingElement(nodeList, *next))) {
        if (++currentOffset == offset)
            return next;
    }
    return 0;
}

} // namespace WebCore

#endif // LiveNodeListBase_h
