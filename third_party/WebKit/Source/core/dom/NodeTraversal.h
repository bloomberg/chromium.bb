/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef NodeTraversal_h
#define NodeTraversal_h

#include "core/dom/Node.h"
#include "wtf/TreeNode.h"

namespace WebCore {

namespace NodeTraversal {

// Does a pre-order traversal of the tree to find the next node after this one.
// This uses the same order that tags appear in the source file. If the stayWithin
// argument is non-null, the traversal will stop once the specified node is reached.
// This can be used to restrict traversal to a particular sub-tree.
Node* next(const Node&);
Node* next(const Node&, const Node* stayWithin);
Node* next(const ContainerNode&);
Node* next(const ContainerNode&, const Node* stayWithin);

// Like next, but skips children and starts with the next sibling.
Node* nextSkippingChildren(const Node&);
Node* nextSkippingChildren(const Node&, const Node* stayWithin);
Node* nextSkippingChildren(const ContainerNode&);
Node* nextSkippingChildren(const ContainerNode&, const Node* stayWithin);

// Does a reverse pre-order traversal to find the node that comes before the current one in document order
Node* previous(const Node&, const Node* stayWithin = 0);

// Like previous, but skips children and starts with the next sibling.
Node* previousSkippingChildren(const Node&, const Node* stayWithin = 0);

// Like next, but visits parents after their children.
Node* nextPostOrder(const Node&, const Node* stayWithin = 0);

// Like previous, but visits parents before their children.
Node* previousPostOrder(const Node&, const Node* stayWithin = 0);

// Pre-order traversal including the pseudo-elements.
Node* previousIncludingPseudo(const Node&, const Node* stayWithin = 0);
Node* nextIncludingPseudo(const Node&, const Node* stayWithin = 0);
Node* nextIncludingPseudoSkippingChildren(const Node&, const Node* stayWithin = 0);

Node* nextAncestorSibling(const Node&);
Node* nextAncestorSibling(const Node&, const Node* stayWithin);

inline Node* next(const Node& current) { return traverseNext(current); }
inline Node* next(const ContainerNode& current) { return traverseNext<Node>(current); }

inline Node* next(const Node& current, const Node* stayWithin) { return traverseNext(current, stayWithin); }
inline Node* next(const ContainerNode& current, const Node* stayWithin) { return traverseNext<Node>(current, stayWithin); }

inline Node* nextSkippingChildren(const Node& current) { return traverseNextSkippingChildren(current); }
inline Node* nextSkippingChildren(const ContainerNode& current) { return traverseNextSkippingChildren<Node>(current); }

inline Node* nextSkippingChildren(const Node& current, const Node* stayWithin) { return traverseNextSkippingChildren(current, stayWithin); }
inline Node* nextSkippingChildren(const ContainerNode& current, const Node* stayWithin) { return traverseNextSkippingChildren<Node>(current, stayWithin); }

}

}

#endif
