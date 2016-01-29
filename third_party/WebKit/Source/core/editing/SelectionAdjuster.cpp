/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/SelectionAdjuster.h"

#include "core/editing/EditingUtilities.h"

namespace blink {

namespace {

Node* enclosingShadowHost(Node* node)
{
    for (Node* runner = node; runner; runner = ComposedTreeTraversal::parent(*runner)) {
        if (isShadowHost(runner))
            return runner;
    }
    return nullptr;
}

bool isEnclosedBy(const PositionInComposedTree& position, const Node& node)
{
    ASSERT(position.isNotNull());
    Node* anchorNode = position.anchorNode();
    if (anchorNode == node)
        return !position.isAfterAnchor() && !position.isBeforeAnchor();

    return ComposedTreeTraversal::isDescendantOf(*anchorNode, node);
}

bool isSelectionBoundary(const Node& node)
{
    return isHTMLTextAreaElement(node) || isHTMLInputElement(node) || isHTMLSelectElement(node);
}

Node* enclosingShadowHostForStart(const PositionInComposedTree& position)
{
    Node* node = position.nodeAsRangeFirstNode();
    if (!node)
        return nullptr;
    Node* shadowHost = enclosingShadowHost(node);
    if (!shadowHost)
        return nullptr;
    if (!isEnclosedBy(position, *shadowHost))
        return nullptr;
    return isSelectionBoundary(*shadowHost) ? shadowHost : nullptr;
}

Node* enclosingShadowHostForEnd(const PositionInComposedTree& position)
{
    Node* node = position.nodeAsRangeLastNode();
    if (!node)
        return nullptr;
    Node* shadowHost = enclosingShadowHost(node);
    if (!shadowHost)
        return nullptr;
    if (!isEnclosedBy(position, *shadowHost))
        return nullptr;
    return isSelectionBoundary(*shadowHost) ? shadowHost : nullptr;
}

PositionInComposedTree adjustPositionInComposedTreeForStart(const PositionInComposedTree& position, Node* shadowHost)
{
    if (isEnclosedBy(position, *shadowHost)) {
        if (position.isBeforeChildren())
            return PositionInComposedTree::beforeNode(shadowHost);
        return PositionInComposedTree::afterNode(shadowHost);
    }

    // We use |firstChild|'s after instead of beforeAllChildren for backward
    // compatibility. The positions are same but the anchors would be different,
    // and selection painting uses anchor nodes.
    if (Node* firstChild = ComposedTreeTraversal::firstChild(*shadowHost))
        return PositionInComposedTree::beforeNode(firstChild);
    return PositionInComposedTree();
}

Position adjustPositionForEnd(const Position& currentPosition, Node* startContainerNode)
{
    TreeScope& treeScope = startContainerNode->treeScope();

    ASSERT(currentPosition.computeContainerNode()->treeScope() != treeScope);

    if (Node* ancestor = treeScope.ancestorInThisScope(currentPosition.computeContainerNode())) {
        if (ancestor->contains(startContainerNode))
            return positionAfterNode(ancestor);
        return positionBeforeNode(ancestor);
    }

    if (Node* lastChild = treeScope.rootNode().lastChild())
        return positionAfterNode(lastChild);

    return Position();
}

PositionInComposedTree adjustPositionInComposedTreeForEnd(const PositionInComposedTree& position, Node* shadowHost)
{
    if (isEnclosedBy(position, *shadowHost)) {
        if (position.isAfterChildren())
            return PositionInComposedTree::afterNode(shadowHost);
        return PositionInComposedTree::beforeNode(shadowHost);
    }

    // We use |lastChild|'s after instead of afterAllChildren for backward
    // compatibility. The positions are same but the anchors would be different,
    // and selection painting uses anchor nodes.
    if (Node* lastChild = ComposedTreeTraversal::lastChild(*shadowHost))
        return PositionInComposedTree::afterNode(lastChild);
    return PositionInComposedTree();
}

Position adjustPositionForStart(const Position& currentPosition, Node* endContainerNode)
{
    TreeScope& treeScope = endContainerNode->treeScope();

    ASSERT(currentPosition.computeContainerNode()->treeScope() != treeScope);

    if (Node* ancestor = treeScope.ancestorInThisScope(currentPosition.computeContainerNode())) {
        if (ancestor->contains(endContainerNode))
            return positionBeforeNode(ancestor);
        return positionAfterNode(ancestor);
    }

    if (Node* firstChild = treeScope.rootNode().firstChild())
        return positionBeforeNode(firstChild);

    return Position();
}

} // namespace

// Updates |selectionInComposedTree| to match with |selection|.
void SelectionAdjuster::adjustSelectionInComposedTree(VisibleSelectionInComposedTree* selectionInComposedTree, const VisibleSelection& selection)
{
    if (selection.isNone()) {
        *selectionInComposedTree = VisibleSelectionInComposedTree();
        return;
    }

    const PositionInComposedTree& base = toPositionInComposedTree(selection.base());
    const PositionInComposedTree& extent = toPositionInComposedTree(selection.extent());
    const PositionInComposedTree& position1 = toPositionInComposedTree(selection.start());
    const PositionInComposedTree& position2 = toPositionInComposedTree(selection.end());
    position1.anchorNode()->updateDistribution();
    position2.anchorNode()->updateDistribution();
    selectionInComposedTree->m_base = base;
    selectionInComposedTree->m_extent = extent;
    selectionInComposedTree->m_affinity = selection.m_affinity;
    selectionInComposedTree->m_isDirectional = selection.m_isDirectional;
    selectionInComposedTree->m_baseIsFirst = base.isNull() || base.compareTo(extent) <= 0;
    if (position1.compareTo(position2) <= 0) {
        selectionInComposedTree->m_start = position1;
        selectionInComposedTree->m_end = position2;
    } else {
        selectionInComposedTree->m_start = position2;
        selectionInComposedTree->m_end = position1;
    }
    selectionInComposedTree->updateSelectionType();
    selectionInComposedTree->didChange();
}

static bool isCrossingShadowBoundaries(const VisibleSelectionInComposedTree& selection)
{
    if (!selection.isRange())
        return false;
    TreeScope& treeScope = selection.base().anchorNode()->treeScope();
    return selection.extent().anchorNode()->treeScope() != treeScope
        || selection.start().anchorNode()->treeScope() != treeScope
        || selection.end().anchorNode()->treeScope() != treeScope;
}

void SelectionAdjuster::adjustSelectionInDOMTree(VisibleSelection* selection, const VisibleSelectionInComposedTree& selectionInComposedTree)
{
    if (selectionInComposedTree.isNone()) {
        *selection = VisibleSelection();
        return;
    }

    const Position& base = toPositionInDOMTree(selectionInComposedTree.base());
    const Position& extent = toPositionInDOMTree(selectionInComposedTree.extent());

    if (isCrossingShadowBoundaries(selectionInComposedTree)) {
        *selection = VisibleSelection(base, extent);
        return;
    }

    const Position& position1 = toPositionInDOMTree(selectionInComposedTree.start());
    const Position& position2 = toPositionInDOMTree(selectionInComposedTree.end());
    selection->m_base = base;
    selection->m_extent = extent;
    selection->m_affinity = selectionInComposedTree.m_affinity;
    selection->m_isDirectional = selectionInComposedTree.m_isDirectional;
    selection->m_baseIsFirst = base.isNull() || base.compareTo(extent) <= 0;
    if (position1.compareTo(position2) <= 0) {
        selection->m_start = position1;
        selection->m_end = position2;
    } else {
        selection->m_start = position2;
        selection->m_end = position1;
    }
    selection->updateSelectionType();
    selection->didChange();
}

void SelectionAdjuster::adjustSelectionToAvoidCrossingShadowBoundaries(VisibleSelection* selection)
{
    // Note: |m_selectionType| isn't computed yet.
    ASSERT(selection->base().isNotNull());
    ASSERT(selection->extent().isNotNull());
    ASSERT(selection->start().isNotNull());
    ASSERT(selection->end().isNotNull());

    // TODO(hajimehoshi): Checking treeScope is wrong when a node is
    // distributed, but we leave it as it is for backward compatibility.
    if (selection->start().anchorNode()->treeScope() == selection->end().anchorNode()->treeScope())
        return;

    if (selection->isBaseFirst()) {
        const Position& newEnd = adjustPositionForEnd(selection->end(), selection->start().computeContainerNode());
        selection->m_extent = newEnd;
        selection->m_end = newEnd;
        return;
    }

    const Position& newStart = adjustPositionForStart(selection->start(), selection->end().computeContainerNode());
    selection->m_extent = newStart;
    selection->m_start = newStart;
}

// This function is called twice. The first is called when |m_start| and |m_end|
// or |m_extent| are same, and the second when |m_start| and |m_end| are changed
// after downstream/upstream.
void SelectionAdjuster::adjustSelectionToAvoidCrossingShadowBoundaries(VisibleSelectionInComposedTree* selection)
{
    Node* const shadowHostStart = enclosingShadowHostForStart(selection->start());
    Node* const shadowHostEnd = enclosingShadowHostForEnd(selection->end());
    if (shadowHostStart == shadowHostEnd)
        return;

    if (selection->isBaseFirst()) {
        Node* const shadowHost = shadowHostStart ? shadowHostStart : shadowHostEnd;
        const PositionInComposedTree& newEnd = adjustPositionInComposedTreeForEnd(selection->end(), shadowHost);
        selection->m_extent = newEnd;
        selection->m_end = newEnd;
        return;
    }
    Node* const shadowHost = shadowHostEnd ? shadowHostEnd : shadowHostStart;
    const PositionInComposedTree& newStart = adjustPositionInComposedTreeForStart(selection->start(), shadowHost);
    selection->m_extent = newStart;
    selection->m_start = newStart;
}

} // namespace blink
