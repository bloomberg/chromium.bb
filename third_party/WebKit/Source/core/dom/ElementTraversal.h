/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef ElementTraversal_h
#define ElementTraversal_h

#include "core/dom/Element.h"
#include "core/dom/NodeTraversal.h"

namespace WebCore {

template <class ElementType>
class Traversal {
public:
    // First / Last element child of the node.
    static ElementType* firstWithin(const ContainerNode& current) { return firstElementWithinTemplate(current); }
    static ElementType* firstWithin(const Node& current) { return firstElementWithinTemplate(current); }
    static ElementType* lastWithin(const ContainerNode& current) { return lastWithinTemplate(current); }
    static ElementType* lastWithin(const Node& current) { return lastWithinTemplate(current); }

    // Pre-order traversal skipping non-element nodes.
    static ElementType* next(const ContainerNode& current) { return traverseNextElementTemplate(current); }
    static ElementType* next(const Node& current) { return traverseNextElementTemplate(current); }
    static ElementType* next(const ContainerNode& current, const Node* stayWithin) { return traverseNextElementTemplate(current, stayWithin); }
    static ElementType* next(const Node& current, const Node* stayWithin) { return traverseNextElementTemplate(current, stayWithin); }

    // Like next, but skips children.
    static ElementType* nextSkippingChildren(const ContainerNode& current) { return traverseNextElementSkippingChildrenTemplate(current); }
    static ElementType* nextSkippingChildren(const Node& current) { return traverseNextElementSkippingChildrenTemplate(current); }
    static ElementType* nextSkippingChildren(const ContainerNode& current, const Node* stayWithin) { return traverseNextElementSkippingChildrenTemplate(current, stayWithin); }
    static ElementType* nextSkippingChildren(const Node& current, const Node* stayWithin) { return traverseNextElementSkippingChildrenTemplate(current, stayWithin); }

    // Pre-order traversal including the pseudo-elements.
    static ElementType* previousIncludingPseudo(const Node&, const Node* stayWithin = 0);
    static ElementType* nextIncludingPseudo(const Node&, const Node* stayWithin = 0);
    static ElementType* nextIncludingPseudoSkippingChildren(const Node&, const Node* stayWithin = 0);

    // Utility function to traverse only the element and pseudo-element siblings of a node.
    static ElementType* pseudoAwarePreviousSibling(const Node&);

    // Previous / Next sibling.
    static ElementType* previousSibling(const Node&);
    static ElementType* nextSibling(const Node&);

private:
    template <class NodeType>
    static ElementType* firstElementWithinTemplate(NodeType&);
    template <class NodeType>
    static ElementType* lastWithinTemplate(NodeType&);
    template <class NodeType>
    static ElementType* traverseNextElementTemplate(NodeType&);
    template <class NodeType>
    static ElementType* traverseNextElementTemplate(NodeType&, const Node* stayWithin);
    template <class NodeType>
    static ElementType* traverseNextElementSkippingChildrenTemplate(NodeType&);
    template <class NodeType>
    static ElementType* traverseNextElementSkippingChildrenTemplate(NodeType&, const Node* stayWithin);
};

typedef Traversal<Element> ElementTraversal;

template <class ElementType>
template <class NodeType>
inline ElementType* Traversal<ElementType>::firstElementWithinTemplate(NodeType& current)
{
    // Except for the root containers, only elements can have element children.
    Node* node = current.firstChild();
    while (node && !isElementOfType<ElementType>(*node))
        node = node->nextSibling();
    return toElement<ElementType>(node);
}

template <class ElementType>
template <class NodeType>
inline ElementType* Traversal<ElementType>::lastWithinTemplate(NodeType& current)
{
    Node* node = current.lastChild();
    while (node && !isElementOfType<ElementType>(*node))
        node = node->previousSibling();
    return toElement<ElementType>(node);
}

template <class ElementType>
template <class NodeType>
inline ElementType* Traversal<ElementType>::traverseNextElementTemplate(NodeType& current)
{
    Node* node = NodeTraversal::next(current);
    while (node && !isElementOfType<ElementType>(*node))
        node = NodeTraversal::nextSkippingChildren(*node);
    return toElement<ElementType>(node);
}

template <class ElementType>
template <class NodeType>
inline ElementType* Traversal<ElementType>::traverseNextElementTemplate(NodeType& current, const Node* stayWithin)
{
    Node* node = NodeTraversal::next(current, stayWithin);
    while (node && !isElementOfType<ElementType>(*node))
        node = NodeTraversal::nextSkippingChildren(*node, stayWithin);
    return toElement<ElementType>(node);
}

template <class ElementType>
template <class NodeType>
inline ElementType* Traversal<ElementType>::traverseNextElementSkippingChildrenTemplate(NodeType& current)
{
    Node* node = NodeTraversal::nextSkippingChildren(current);
    while (node && !isElementOfType<ElementType>(*node))
        node = NodeTraversal::nextSkippingChildren(*node);
    return toElement<ElementType>(node);
}

template <class ElementType>
template <class NodeType>
inline ElementType* Traversal<ElementType>::traverseNextElementSkippingChildrenTemplate(NodeType& current, const Node* stayWithin)
{
    Node* node = NodeTraversal::nextSkippingChildren(current, stayWithin);
    while (node && !isElementOfType<ElementType>(*node))
        node = NodeTraversal::nextSkippingChildren(*node, stayWithin);
    return toElement<ElementType>(node);
}

template <class ElementType>
inline ElementType* Traversal<ElementType>::previousIncludingPseudo(const Node& current, const Node* stayWithin)
{
    Node* node = NodeTraversal::previousIncludingPseudo(current, stayWithin);
    while (node && !isElementOfType<ElementType>(*node))
        node = NodeTraversal::previousIncludingPseudo(*node, stayWithin);
    return toElement<ElementType>(node);
}

template <class ElementType>
inline ElementType* Traversal<ElementType>::nextIncludingPseudo(const Node& current, const Node* stayWithin)
{
    Node* node = NodeTraversal::nextIncludingPseudo(current, stayWithin);
    while (node && !isElementOfType<ElementType>(*node))
        node = NodeTraversal::nextIncludingPseudo(*node, stayWithin);
    return toElement<ElementType>(node);
}

template <class ElementType>
inline ElementType* Traversal<ElementType>::nextIncludingPseudoSkippingChildren(const Node& current, const Node* stayWithin)
{
    Node* node = NodeTraversal::nextIncludingPseudoSkippingChildren(current, stayWithin);
    while (node && !isElementOfType<ElementType>(*node))
        node = NodeTraversal::nextIncludingPseudoSkippingChildren(*node, stayWithin);
    return toElement<ElementType>(node);
}

template <class ElementType>
inline ElementType* Traversal<ElementType>::pseudoAwarePreviousSibling(const Node& current)
{
    Node* node = current.pseudoAwarePreviousSibling();
    while (node && !isElementOfType<ElementType>(*node))
        node = node->pseudoAwarePreviousSibling();
    return toElement<ElementType>(node);
}

template <class ElementType>
inline ElementType* Traversal<ElementType>::previousSibling(const Node& current)
{
    Node* node = current.previousSibling();
    while (node && !isElementOfType<ElementType>(*node))
        node = node->previousSibling();
    return toElement<ElementType>(node);
}

template <class ElementType>
inline ElementType* Traversal<ElementType>::nextSibling(const Node& current)
{
    Node* node = current.nextSibling();
    while (node && !isElementOfType<ElementType>(*node))
        node = node->nextSibling();
    return toElement<ElementType>(node);
}

} // namespace WebCore

#endif
