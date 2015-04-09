// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/EditingStrategy.h"

#include "core/editing/htmlediting.h"

namespace blink {

template <typename Traversal>
short comparePositionsAlgorithm(Node* containerA, int offsetA, Node* containerB, int offsetB, bool* disconnected)
{
    ASSERT(containerA);
    ASSERT(containerB);

    if (disconnected)
        *disconnected = false;

    if (!containerA)
        return -1;
    if (!containerB)
        return 1;

    // see DOM2 traversal & range section 2.5

    // case 1: both points have the same container
    if (containerA == containerB) {
        if (offsetA == offsetB)
            return 0; // A is equal to B
        if (offsetA < offsetB)
            return -1; // A is before B
        return 1; // A is after B
    }

    // case 2: node C (container B or an ancestor) is a child node of A
    Node* c = containerB;
    while (c && Traversal::parent(*c) != containerA)
        c = Traversal::parent(*c);
    if (c) {
        int offsetC = 0;
        Node* n = Traversal::firstChild(*containerA);
        while (n != c && offsetC < offsetA) {
            offsetC++;
            n = Traversal::nextSibling(*n);
        }

        if (offsetA <= offsetC)
            return -1; // A is before B
        return 1; // A is after B
    }

    // case 3: node C (container A or an ancestor) is a child node of B
    c = containerA;
    while (c && Traversal::parent(*c) != containerB)
        c = Traversal::parent(*c);
    if (c) {
        int offsetC = 0;
        Node* n = Traversal::firstChild(*containerB);
        while (n != c && offsetC < offsetB) {
            offsetC++;
            n = Traversal::nextSibling(*n);
        }

        if (offsetC < offsetB)
            return -1; // A is before B
        return 1; // A is after B
    }

    // case 4: containers A & B are siblings, or children of siblings
    // ### we need to do a traversal here instead
    Node* commonAncestor = Traversal::commonAncestor(*containerA, *containerB);
    if (!commonAncestor) {
        if (disconnected)
            *disconnected = true;
        return 0;
    }
    Node* childA = containerA;
    while (childA && Traversal::parent(*childA) != commonAncestor)
        childA = Traversal::parent(*childA);
    if (!childA)
        childA = commonAncestor;
    Node* childB = containerB;
    while (childB && Traversal::parent(*childB) != commonAncestor)
        childB = Traversal::parent(*childB);
    if (!childB)
        childB = commonAncestor;

    if (childA == childB)
        return 0; // A is equal to B

    Node* n = Traversal::firstChild(*commonAncestor);
    while (n) {
        if (n == childA)
            return -1; // A is before B
        if (n == childB)
            return 1; // A is after B
        n = Traversal::nextSibling(*n);
    }

    // Should never reach this point.
    ASSERT_NOT_REACHED();
    return 0;
}

short EditingStrategy:: comparePositions(Node* containerA, int offsetA, Node* containerB, int offsetB, bool* disconnected)
{
    return comparePositionsAlgorithm<NodeTraversal>(containerA, offsetA, containerB, offsetB, disconnected);
}

bool EditingStrategy::editingIgnoresContent(const Node* node)
{
    return ::blink::editingIgnoresContent(node);
}

int EditingStrategy::lastOffsetForEditing(const Node* node)
{
    return ::blink::lastOffsetForEditing(node);
}

} // namespace blink
