/*
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/editing/Position.h"

#include "core/HTMLNames.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/PositionIterator.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLTableElement.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "wtf/text/CString.h"
#include <stdio.h>

namespace blink {

using namespace HTMLNames;

#if ENABLE(ASSERT)
static bool canBeAnchorNode(Node* node)
{
    if (!node || node->isFirstLetterPseudoElement())
        return true;
    return !node->isPseudoElement();
}
#endif

template <typename Strategy>
const TreeScope* PositionAlgorithm<Strategy>::commonAncestorTreeScope(const PositionAlgorithm<Strategy>& a, const PositionAlgorithm<Strategy>& b)
{
    if (!a.computeContainerNode() || !b.computeContainerNode())
        return nullptr;
    return a.computeContainerNode()->treeScope().commonAncestorTreeScope(b.computeContainerNode()->treeScope());
}


template <typename Strategy>
PositionAlgorithm<Strategy> PositionAlgorithm<Strategy>::editingPositionOf(PassRefPtrWillBeRawPtr<Node> anchorNode, int offset)
{
    if (!anchorNode || anchorNode->isTextNode())
        return PositionAlgorithm<Strategy>(anchorNode, offset);

    if (!Strategy::editingIgnoresContent(anchorNode.get()))
        return PositionAlgorithm<Strategy>(anchorNode, offset);

    if (offset == 0)
        return PositionAlgorithm<Strategy>(anchorNode, PositionAnchorType::BeforeAnchor);

    ASSERT(offset == Strategy::lastOffsetForEditing(anchorNode.get()));
    return PositionAlgorithm<Strategy>(anchorNode, PositionAnchorType::AfterAnchor);
}

template <typename Strategy>
PositionAlgorithm<Strategy>::PositionAlgorithm(PassRefPtrWillBeRawPtr<Node> anchorNode, PositionAnchorType anchorType)
    : m_anchorNode(anchorNode)
    , m_offset(0)
    , m_anchorType(anchorType)
{
    if (!m_anchorNode) {
        m_anchorType = PositionAnchorType::OffsetInAnchor;
        return;
    }
    if (m_anchorNode->isTextNode()) {
        ASSERT(m_anchorType == PositionAnchorType::BeforeAnchor || m_anchorType == PositionAnchorType::AfterAnchor);
        return;
    }
    ASSERT(canBeAnchorNode(m_anchorNode.get()));
    ASSERT(m_anchorType != PositionAnchorType::OffsetInAnchor);
}

template <typename Strategy>
PositionAlgorithm<Strategy>::PositionAlgorithm(PassRefPtrWillBeRawPtr<Node> anchorNode, int offset)
    : m_anchorNode(anchorNode)
    , m_offset(offset)
    , m_anchorType(PositionAnchorType::OffsetInAnchor)
{
    ASSERT(offset >= 0);
    ASSERT(canBeAnchorNode(m_anchorNode.get()));
}

template <typename Strategy>
PositionAlgorithm<Strategy>::PositionAlgorithm(const PositionAlgorithm& other)
    : m_anchorNode(other.m_anchorNode)
    , m_offset(other.m_offset)
    , m_anchorType(other.m_anchorType)
{
}

// --

template <typename Strategy>
Node* PositionAlgorithm<Strategy>::computeContainerNode() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionAnchorType::BeforeChildren:
    case PositionAnchorType::AfterChildren:
    case PositionAnchorType::OffsetInAnchor:
        return m_anchorNode.get();
    case PositionAnchorType::BeforeAnchor:
    case PositionAnchorType::AfterAnchor:
        return Strategy::parent(*m_anchorNode);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

template <typename Strategy>
int PositionAlgorithm<Strategy>::computeOffsetInContainerNode() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionAnchorType::BeforeChildren:
        return 0;
    case PositionAnchorType::AfterChildren:
        return lastOffsetInNode(m_anchorNode.get());
    case PositionAnchorType::OffsetInAnchor:
        return minOffsetForNode(m_anchorNode.get(), m_offset);
    case PositionAnchorType::BeforeAnchor:
        return Strategy::index(*m_anchorNode);
    case PositionAnchorType::AfterAnchor:
        return Strategy::index(*m_anchorNode) + 1;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// Neighbor-anchored positions are invalid DOM positions, so they need to be
// fixed up before handing them off to the Range object.
template <typename Strategy>
PositionAlgorithm<Strategy> PositionAlgorithm<Strategy>::parentAnchoredEquivalent() const
{
    if (!m_anchorNode)
        return PositionAlgorithm<Strategy>();

    // FIXME: This should only be necessary for legacy positions, but is also needed for positions before and after Tables
    if (m_offset == 0 && !isAfterAnchorOrAfterChildren()) {
        if (Strategy::parent(*m_anchorNode) && (Strategy::editingIgnoresContent(m_anchorNode.get()) || isRenderedHTMLTableElement(m_anchorNode.get())))
            return inParentBeforeNode(*m_anchorNode);
        return PositionAlgorithm<Strategy>(m_anchorNode.get(), 0);
    }
    if (!m_anchorNode->offsetInCharacters()
        && (isAfterAnchorOrAfterChildren() || static_cast<unsigned>(m_offset) == m_anchorNode->countChildren())
        && (Strategy::editingIgnoresContent(m_anchorNode.get()) || isRenderedHTMLTableElement(m_anchorNode.get()))
        && computeContainerNode()) {
        return inParentAfterNode(*m_anchorNode);
    }

    return PositionAlgorithm<Strategy>(computeContainerNode(), computeOffsetInContainerNode());
}

template <typename Strategy>
PositionAlgorithm<Strategy> PositionAlgorithm<Strategy>::toOffsetInAnchor() const
{
    if (isNull())
        return PositionAlgorithm<Strategy>();

    return PositionAlgorithm<Strategy>(computeContainerNode(), computeOffsetInContainerNode());
}

template <typename Strategy>
int PositionAlgorithm<Strategy>::computeEditingOffset() const
{
    if (isAfterAnchorOrAfterChildren())
        return Strategy::lastOffsetForEditing(m_anchorNode.get());
    return m_offset;
}

template <typename Strategy>
Node* PositionAlgorithm<Strategy>::computeNodeBeforePosition() const
{
    if (!m_anchorNode)
        return 0;
    switch (anchorType()) {
    case PositionAnchorType::BeforeChildren:
        return 0;
    case PositionAnchorType::AfterChildren:
        return Strategy::lastChild(*m_anchorNode);
    case PositionAnchorType::OffsetInAnchor:
        return m_offset ? Strategy::childAt(*m_anchorNode, m_offset - 1) : 0;
    case PositionAnchorType::BeforeAnchor:
        return Strategy::previousSibling(*m_anchorNode);
    case PositionAnchorType::AfterAnchor:
        return m_anchorNode.get();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

template <typename Strategy>
Node* PositionAlgorithm<Strategy>::computeNodeAfterPosition() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionAnchorType::BeforeChildren:
        return Strategy::firstChild(*m_anchorNode);
    case PositionAnchorType::AfterChildren:
        return 0;
    case PositionAnchorType::OffsetInAnchor:
        return Strategy::childAt(*m_anchorNode, m_offset);
    case PositionAnchorType::BeforeAnchor:
        return m_anchorNode.get();
    case PositionAnchorType::AfterAnchor:
        return Strategy::nextSibling(*m_anchorNode);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// An implementation of |Range::firstNode()|.
template <typename Strategy>
Node* PositionAlgorithm<Strategy>::nodeAsRangeFirstNode() const
{
    if (!m_anchorNode)
        return nullptr;
    if (!isOffsetInAnchor())
        return toOffsetInAnchor().nodeAsRangeFirstNode();
    if (m_anchorNode->offsetInCharacters())
        return m_anchorNode.get();
    if (Node* child = Strategy::childAt(*m_anchorNode, m_offset))
        return child;
    if (!m_offset)
        return m_anchorNode.get();
    return Strategy::nextSkippingChildren(*m_anchorNode);
}

template <typename Strategy>
Node* PositionAlgorithm<Strategy>::nodeAsRangeLastNode() const
{
    if (isNull())
        return nullptr;
    if (Node* pastLastNode = nodeAsRangePastLastNode())
        return Strategy::previous(*pastLastNode);
    return &Strategy::lastWithinOrSelf(*computeContainerNode());
}

// An implementation of |Range::pastLastNode()|.
template <typename Strategy>
Node* PositionAlgorithm<Strategy>::nodeAsRangePastLastNode() const
{
    if (!m_anchorNode)
        return nullptr;
    if (!isOffsetInAnchor())
        return toOffsetInAnchor().nodeAsRangePastLastNode();
    if (m_anchorNode->offsetInCharacters())
        return Strategy::nextSkippingChildren(*m_anchorNode);
    if (Node* child = Strategy::childAt(*m_anchorNode, m_offset))
        return child;
    return Strategy::nextSkippingChildren(*m_anchorNode);
}

template <typename Strategy>
Node* PositionAlgorithm<Strategy>::commonAncestorContainer(const PositionAlgorithm<Strategy>& other) const
{
    return Strategy::commonAncestor(*computeContainerNode(), *other.computeContainerNode());
}

int comparePositions(const PositionInComposedTree& positionA, const PositionInComposedTree& positionB)
{
    ASSERT(positionA.isNotNull());
    ASSERT(positionB.isNotNull());

    Node* containerA = positionA.computeContainerNode();
    Node* containerB = positionB.computeContainerNode();
    int offsetA = positionA.computeOffsetInContainerNode();
    int offsetB = positionB.computeOffsetInContainerNode();
    return comparePositionsInComposedTree(containerA, offsetA, containerB, offsetB);
}

template <typename Strategy>
int PositionAlgorithm<Strategy>::compareTo(const PositionAlgorithm<Strategy>& other) const
{
    return comparePositions(*this, other);
}

// TODO(yosin) To avoid forward declaration, we should move implementation of
// |uncheckedPreviousOffsetForBackwardDeletion()| here.
static int uncheckedPreviousOffsetForBackwardDeletion(const Node*, int current);

template <typename Strategy>
PositionAlgorithm<Strategy> PositionAlgorithm<Strategy>::previous(PositionMoveType moveType) const
{
    Node* node = anchorNode();
    if (!node)
        return PositionAlgorithm<Strategy>(*this);

    int offset = computeEditingOffset();

    if (offset > 0) {
        if (editingIgnoresContent(node))
            return beforeNode(node);
        if (Node* child = Strategy::childAt(*node, offset - 1))
            return lastPositionInOrAfterNode(child);

        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going backward one character at a time is correct.
        //   2) The old offset was a bogus offset like (<br>, 1), and there is no child.
        //      Going from 1 to 0 is correct.
        switch (moveType) {
        case CodePoint:
            return PositionAlgorithm<Strategy>(node, offset - 1);
        case Character:
            return PositionAlgorithm<Strategy>(node, uncheckedPreviousOffset(node, offset));
        case BackwardDeletion:
            return PositionAlgorithm<Strategy>(node, uncheckedPreviousOffsetForBackwardDeletion(node, offset));
        }
    }

    if (ContainerNode* parent = Strategy::parent(*node)) {
        if (editingIgnoresContent(parent))
            return beforeNode(parent);
        // TODO(yosin) We should use |Strategy::index(Node&)| instead of
        // |Node::nodeIndex()|.
        return PositionAlgorithm<Strategy>(parent, node->nodeIndex());
    }
    return PositionAlgorithm<Strategy>(*this);
}

template <typename Strategy>
PositionAlgorithm<Strategy> PositionAlgorithm<Strategy>::next(PositionMoveType moveType) const
{
    ASSERT(moveType != BackwardDeletion);

    Node* node = anchorNode();
    if (!node)
        return PositionAlgorithm<Strategy>(*this);

    int offset = computeEditingOffset();

    if (Node* child = Strategy::childAt(*node, offset))
        return firstPositionInOrBeforeNode(child);

    // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
    // DOM tree version.
    if (!Strategy::hasChildren(*node) && offset < EditingStrategy::lastOffsetForEditing(node)) {
        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going forward one character at a time is correct.
        //   2) The new offset is a bogus offset like (<br>, 1), and there is no child.
        //      Going from 0 to 1 is correct.
        return editingPositionOf(node, (moveType == Character) ? uncheckedNextOffset(node, offset) : offset + 1);
    }

    if (ContainerNode* parent = Strategy::parent(*node))
        return editingPositionOf(parent, Strategy::index(*node) + 1);
    return PositionAlgorithm<Strategy>(*this);
}

int uncheckedPreviousOffset(const Node* n, int current)
{
    return n->layoutObject() ? n->layoutObject()->previousOffset(current) : current - 1;
}

static int uncheckedPreviousOffsetForBackwardDeletion(const Node* n, int current)
{
    return n->layoutObject() ? n->layoutObject()->previousOffsetForBackwardDeletion(current) : current - 1;
}

int uncheckedNextOffset(const Node* n, int current)
{
    return n->layoutObject() ? n->layoutObject()->nextOffset(current) : current + 1;
}

template <typename Strategy>
bool PositionAlgorithm<Strategy>::atFirstEditingPositionForNode() const
{
    if (isNull())
        return true;
    // FIXME: Position before anchor shouldn't be considered as at the first editing position for node
    // since that position resides outside of the node.
    switch (m_anchorType) {
    case PositionAnchorType::OffsetInAnchor:
        return m_offset == 0;
    case PositionAnchorType::BeforeChildren:
    case PositionAnchorType::BeforeAnchor:
        return true;
    case PositionAnchorType::AfterChildren:
    case PositionAnchorType::AfterAnchor:
        // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead
        // of DOM tree version.
        return !EditingStrategy::lastOffsetForEditing(anchorNode());
    }
    ASSERT_NOT_REACHED();
    return false;
}

template <typename Strategy>
bool PositionAlgorithm<Strategy>::atLastEditingPositionForNode() const
{
    if (isNull())
        return true;
    // TODO(yosin): Position after anchor shouldn't be considered as at the
    // first editing position for node since that position resides outside of
    // the node.
    // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
    // DOM tree version.
    return isAfterAnchorOrAfterChildren() || m_offset >= EditingStrategy::lastOffsetForEditing(anchorNode());
}

// Returns true if the visually equivalent positions around have different
// editability. A position is considered at editing boundary if one of the
// following is true:
// 1. It is the first position in the node and the next visually equivalent
//    position is non editable.
// 2. It is the last position in the node and the previous visually equivalent
//    position is non editable.
// 3. It is an editable position and both the next and previous visually
//    equivalent positions are both non editable.
template <typename Strategy>
static bool atEditingBoundary(const PositionAlgorithm<Strategy> positions)
{
    PositionAlgorithm<Strategy> nextPosition = positions.downstream(CanCrossEditingBoundary);
    if (positions.atFirstEditingPositionForNode() && nextPosition.isNotNull() && !nextPosition.anchorNode()->hasEditableStyle())
        return true;

    PositionAlgorithm<Strategy> prevPosition = positions.upstream(CanCrossEditingBoundary);
    if (positions.atLastEditingPositionForNode() && prevPosition.isNotNull() && !prevPosition.anchorNode()->hasEditableStyle())
        return true;

    return nextPosition.isNotNull() && !nextPosition.anchorNode()->hasEditableStyle()
        && prevPosition.isNotNull() && !prevPosition.anchorNode()->hasEditableStyle();
}

template <typename Strategy>
static ContainerNode* nonShadowBoundaryParentNode(Node* node)
{
    ContainerNode* parent = Strategy::parent(*node);
    return parent && !parent->isShadowRoot() ? parent : nullptr;
}

template <typename Strategy>
static Node* parentEditingBoundaryAlgorithm(const PositionAlgorithm<Strategy>& position)
{
    Node* const anchorNode = position.anchorNode();
    if (!anchorNode)
        return nullptr;

    Node* documentElement = anchorNode->document().documentElement();
    if (!documentElement)
        return nullptr;

    Node* boundary = position.computeContainerNode();
    while (boundary != documentElement && nonShadowBoundaryParentNode<Strategy>(boundary) && anchorNode->hasEditableStyle() == Strategy::parent(*boundary)->hasEditableStyle())
        boundary = nonShadowBoundaryParentNode<Strategy>(boundary);

    return boundary;
}

Node* parentEditingBoundary(const Position& position)
{
    return parentEditingBoundaryAlgorithm<EditingStrategy>(position);
}

Node* parentEditingBoundary(const PositionInComposedTree& position)
{
    return parentEditingBoundaryAlgorithm<EditingInComposedTreeStrategy>(position);
}

template <typename Strategy>
bool PositionAlgorithm<Strategy>::atStartOfTree() const
{
    if (isNull())
        return true;
    return !Strategy::parent(*anchorNode()) && m_offset == 0;
}

template <typename Strategy>
bool PositionAlgorithm<Strategy>::atEndOfTree() const
{
    if (isNull())
        return true;
    // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
    // DOM tree version.
    return !Strategy::parent(*anchorNode()) && m_offset >= EditingStrategy::lastOffsetForEditing(anchorNode());
}

template <typename Strategy>
PositionAlgorithm<Strategy> PositionAlgorithm<Strategy>::upstream(EditingBoundaryCrossingRule rule) const
{
    return mostForwardCaretPosition(*this, rule);
}

template <typename Strategy>
PositionAlgorithm<Strategy> PositionAlgorithm<Strategy>::downstream(EditingBoundaryCrossingRule rule) const
{
    return mostBackwardCaretPosition(*this, rule);
}

static int boundingBoxLogicalHeight(LayoutObject *o, const IntRect &rect)
{
    return o->style()->isHorizontalWritingMode() ? rect.height() : rect.width();
}

// TODO(yosin) We should move |hasRenderedNonAnonymousDescendantsWithHeight|
// to "VisibleUnits.cpp" to reduce |LayoutObject| dependency in "Position.cpp"
bool hasRenderedNonAnonymousDescendantsWithHeight(LayoutObject* layoutObject)
{
    LayoutObject* stop = layoutObject->nextInPreOrderAfterChildren();
    for (LayoutObject *o = layoutObject->slowFirstChild(); o && o != stop; o = o->nextInPreOrder()) {
        if (o->nonPseudoNode()) {
            if ((o->isText() && boundingBoxLogicalHeight(o, toLayoutText(o)->linesBoundingBox()))
                || (o->isBox() && toLayoutBox(o)->pixelSnappedLogicalHeight())
                || (o->isLayoutInline() && isEmptyInline(LineLayoutItem(o)) && boundingBoxLogicalHeight(o, toLayoutInline(o)->linesBoundingBox())))
                return true;
        }
    }
    return false;
}

template <typename Strategy>
Node* PositionAlgorithm<Strategy>::rootUserSelectAllForNode(Node* node)
{
    if (!node || !nodeIsUserSelectAll(node))
        return 0;
    Node* parent = Strategy::parent(*node);
    if (!parent)
        return node;

    Node* candidateRoot = node;
    while (parent) {
        if (!parent->layoutObject()) {
            parent = Strategy::parent(*parent);
            continue;
        }
        if (!nodeIsUserSelectAll(parent))
            break;
        candidateRoot = parent;
        parent = Strategy::parent(*candidateRoot);
    }
    return candidateRoot;
}

template <typename Strategy>
bool PositionAlgorithm<Strategy>::isCandidate() const
{
    if (isNull())
        return false;

    LayoutObject* layoutObject = anchorNode()->layoutObject();
    if (!layoutObject)
        return false;

    if (layoutObject->style()->visibility() != VISIBLE)
        return false;

    if (layoutObject->isBR()) {
        // TODO(leviw) The condition should be
        // m_anchorType == PositionAnchorType::BeforeAnchor, but for now we
        // still need to support legacy positions.
        return !m_offset && !isAfterAnchor() && !nodeIsUserSelectNone(Strategy::parent(*anchorNode()));
    }

    if (layoutObject->isText())
        return !nodeIsUserSelectNone(anchorNode()) && inRenderedText(*this);

    if (layoutObject->isSVG()) {
        // We don't consider SVG elements are contenteditable except for
        // associated layoutObject returns isText() true, e.g. LayoutSVGInlineText.
        return false;
    }

    if (isRenderedHTMLTableElement(anchorNode()) || Strategy::editingIgnoresContent(anchorNode()))
        return (atFirstEditingPositionForNode() || atLastEditingPositionForNode()) && !nodeIsUserSelectNone(Strategy::parent(*anchorNode()));

    if (isHTMLHtmlElement(*m_anchorNode))
        return false;

    if (layoutObject->isLayoutBlockFlow() || layoutObject->isFlexibleBox() || layoutObject->isLayoutGrid()) {
        if (toLayoutBlock(layoutObject)->logicalHeight() || isHTMLBodyElement(*m_anchorNode)) {
            if (!hasRenderedNonAnonymousDescendantsWithHeight(layoutObject))
                return atFirstEditingPositionForNode() && !nodeIsUserSelectNone(anchorNode());
            return m_anchorNode->hasEditableStyle() && !nodeIsUserSelectNone(anchorNode()) && atEditingBoundary(*this);
        }
    } else {
        LocalFrame* frame = m_anchorNode->document().frame();
        bool caretBrowsing = frame->settings() && frame->settings()->caretBrowsingEnabled();
        return (caretBrowsing || m_anchorNode->hasEditableStyle()) && !nodeIsUserSelectNone(anchorNode()) && atEditingBoundary(*this);
    }

    return false;
}

// TODO(yosin) We should move |inRenderedText()| to "VisibleUnits.h" for
// reduce dependency of |LayoutObject| in |Position| class.
template <typename Strategy>
static bool inRenderedTextAlgorithm(const PositionAlgorithm<Strategy>& position)
{
    Node* const anchorNode = position.anchorNode();
    if (!anchorNode || !anchorNode->isTextNode())
        return false;

    LayoutObject* layoutObject = anchorNode->layoutObject();
    if (!layoutObject)
        return false;

    const int offsetInNode = position.computeEditingOffset();
    LayoutText* textLayoutObject = toLayoutText(layoutObject);
    for (InlineTextBox *box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
        if (offsetInNode < static_cast<int>(box->start()) && !textLayoutObject->containsReversedText()) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not laid out. Return false.
            return false;
        }
        if (box->containsCaretOffset(offsetInNode)) {
            // Return false for offsets inside composed characters.
            return offsetInNode == 0 || offsetInNode == textLayoutObject->nextOffset(textLayoutObject->previousOffset(offsetInNode));
        }
    }

    return false;
}

bool inRenderedText(const Position& position)
{
    return inRenderedTextAlgorithm<EditingStrategy>(position);
}

bool inRenderedText(const PositionInComposedTree& position)
{
    return inRenderedTextAlgorithm<EditingInComposedTreeStrategy>(position);
}

template <typename Strategy>
InlineBoxPosition PositionAlgorithm<Strategy>::computeInlineBoxPosition(TextAffinity affinity) const
{
    return computeInlineBoxPosition(affinity, primaryDirectionOf(*anchorNode()));
}

static bool isNonTextLeafChild(LayoutObject* object)
{
    if (object->slowFirstChild())
        return false;
    if (object->isText())
        return false;
    return true;
}

static InlineTextBox* searchAheadForBetterMatch(LayoutObject* layoutObject)
{
    LayoutBlock* container = layoutObject->containingBlock();
    for (LayoutObject* next = layoutObject->nextInPreOrder(container); next; next = next->nextInPreOrder(container)) {
        if (next->isLayoutBlock())
            return 0;
        if (next->isBR())
            return 0;
        if (isNonTextLeafChild(next))
            return 0;
        if (next->isText()) {
            InlineTextBox* match = 0;
            int minOffset = INT_MAX;
            for (InlineTextBox* box = toLayoutText(next)->firstTextBox(); box; box = box->nextTextBox()) {
                int caretMinOffset = box->caretMinOffset();
                if (caretMinOffset < minOffset) {
                    match = box;
                    minOffset = caretMinOffset;
                }
            }
            if (match)
                return match;
        }
    }
    return 0;
}

template <typename Strategy>
PositionAlgorithm<Strategy> downstreamIgnoringEditingBoundaries(PositionAlgorithm<Strategy> position)
{
    PositionAlgorithm<Strategy> lastPosition;
    while (position != lastPosition) {
        lastPosition = position;
        position = position.downstream(CanCrossEditingBoundary);
    }
    return position;
}

template <typename Strategy>
PositionAlgorithm<Strategy> upstreamIgnoringEditingBoundaries(PositionAlgorithm<Strategy> position)
{
    PositionAlgorithm<Strategy> lastPosition;
    while (position != lastPosition) {
        lastPosition = position;
        position = position.upstream(CanCrossEditingBoundary);
    }
    return position;
}

template <typename Strategy>
InlineBoxPosition PositionAlgorithm<Strategy>::computeInlineBoxPosition(TextAffinity affinity, TextDirection primaryDirection) const
{
    InlineBox* inlineBox = nullptr;
    int caretOffset = computeEditingOffset();
    LayoutObject* layoutObject = m_anchorNode->isShadowRoot() ? toShadowRoot(m_anchorNode)->host()->layoutObject() : m_anchorNode->layoutObject();

    if (!layoutObject->isText()) {
        inlineBox = 0;
        if (canHaveChildrenForEditing(anchorNode()) && layoutObject->isLayoutBlockFlow() && hasRenderedNonAnonymousDescendantsWithHeight(layoutObject)) {
            // Try a visually equivalent position with possibly opposite
            // editability. This helps in case |this| is in an editable block
            // but surrounded by non-editable positions. It acts to negate the
            // logic at the beginning of LayoutObject::createVisiblePosition().
            PositionAlgorithm<Strategy> thisPosition = PositionAlgorithm<Strategy>(*this);
            PositionAlgorithm<Strategy> equivalent = downstreamIgnoringEditingBoundaries(thisPosition);
            if (equivalent == thisPosition) {
                equivalent = upstreamIgnoringEditingBoundaries(thisPosition);
                if (equivalent == thisPosition || downstreamIgnoringEditingBoundaries(equivalent) == thisPosition)
                    return InlineBoxPosition(inlineBox, caretOffset);
            }

            return equivalent.computeInlineBoxPosition(TextAffinity::Upstream, primaryDirection);
        }
        if (layoutObject->isBox()) {
            inlineBox = toLayoutBox(layoutObject)->inlineBoxWrapper();
            if (!inlineBox || (caretOffset > inlineBox->caretMinOffset() && caretOffset < inlineBox->caretMaxOffset()))
                return InlineBoxPosition(inlineBox, caretOffset);
        }
    } else {
        LayoutText* textLayoutObject = toLayoutText(layoutObject);

        InlineTextBox* box;
        InlineTextBox* candidate = 0;

        for (box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (caretOffset < caretMinOffset || caretOffset > caretMaxOffset || (caretOffset == caretMaxOffset && box->isLineBreak()))
                continue;

            if (caretOffset > caretMinOffset && caretOffset < caretMaxOffset)
                return InlineBoxPosition(box, caretOffset);

            if (((caretOffset == caretMaxOffset) ^ (affinity == TextAffinity::Downstream))
                || ((caretOffset == caretMinOffset) ^ (affinity == TextAffinity::Upstream))
                || (caretOffset == caretMaxOffset && box->nextLeafChild() && box->nextLeafChild()->isLineBreak()))
                break;

            candidate = box;
        }
        if (candidate && candidate == textLayoutObject->lastTextBox() && affinity == TextAffinity::Downstream) {
            box = searchAheadForBetterMatch(textLayoutObject);
            if (box)
                caretOffset = box->caretMinOffset();
        }
        inlineBox = box ? box : candidate;
    }

    if (!inlineBox)
        return InlineBoxPosition(inlineBox, caretOffset);

    unsigned char level = inlineBox->bidiLevel();

    if (inlineBox->direction() == primaryDirection) {
        if (caretOffset == inlineBox->caretRightmostOffset()) {
            InlineBox* nextBox = inlineBox->nextLeafChild();
            if (!nextBox || nextBox->bidiLevel() >= level)
                return InlineBoxPosition(inlineBox, caretOffset);

            level = nextBox->bidiLevel();
            InlineBox* prevBox = inlineBox;
            do {
                prevBox = prevBox->prevLeafChild();
            } while (prevBox && prevBox->bidiLevel() > level);

            // For example, abc FED 123 ^ CBA
            if (prevBox && prevBox->bidiLevel() == level)
                return InlineBoxPosition(inlineBox, caretOffset);

            // For example, abc 123 ^ CBA
            while (InlineBox* nextBox = inlineBox->nextLeafChild()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            return InlineBoxPosition(inlineBox, inlineBox->caretRightmostOffset());
        }

        InlineBox* prevBox = inlineBox->prevLeafChild();
        if (!prevBox || prevBox->bidiLevel() >= level)
            return InlineBoxPosition(inlineBox, caretOffset);

        level = prevBox->bidiLevel();
        InlineBox* nextBox = inlineBox;
        do {
            nextBox = nextBox->nextLeafChild();
        } while (nextBox && nextBox->bidiLevel() > level);

        if (nextBox && nextBox->bidiLevel() == level)
            return InlineBoxPosition(inlineBox, caretOffset);

        while (InlineBox* prevBox = inlineBox->prevLeafChild()) {
            if (prevBox->bidiLevel() < level)
                break;
            inlineBox = prevBox;
        }
        return InlineBoxPosition(inlineBox, inlineBox->caretLeftmostOffset());
    }

    if (caretOffset == inlineBox->caretLeftmostOffset()) {
        InlineBox* prevBox = inlineBox->prevLeafChildIgnoringLineBreak();
        if (!prevBox || prevBox->bidiLevel() < level) {
            // Left edge of a secondary run. Set to the right edge of the entire
            // run.
            while (InlineBox* nextBox = inlineBox->nextLeafChildIgnoringLineBreak()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            return InlineBoxPosition(inlineBox, inlineBox->caretRightmostOffset());
        }

        if (prevBox->bidiLevel() > level) {
            // Right edge of a "tertiary" run. Set to the left edge of that run.
            while (InlineBox* tertiaryBox = inlineBox->prevLeafChildIgnoringLineBreak()) {
                if (tertiaryBox->bidiLevel() <= level)
                    break;
                inlineBox = tertiaryBox;
            }
            return InlineBoxPosition(inlineBox, inlineBox->caretLeftmostOffset());
        }
        return InlineBoxPosition(inlineBox, caretOffset);
    }

    if (layoutObject && layoutObject->style()->unicodeBidi() == Plaintext) {
        if (inlineBox->bidiLevel() < level)
            return InlineBoxPosition(inlineBox, inlineBox->caretLeftmostOffset());
        return InlineBoxPosition(inlineBox, inlineBox->caretRightmostOffset());
    }

    InlineBox* nextBox = inlineBox->nextLeafChildIgnoringLineBreak();
    if (!nextBox || nextBox->bidiLevel() < level) {
        // Right edge of a secondary run. Set to the left edge of the entire
        // run.
        while (InlineBox* prevBox = inlineBox->prevLeafChildIgnoringLineBreak()) {
            if (prevBox->bidiLevel() < level)
                break;
            inlineBox = prevBox;
        }
        return InlineBoxPosition(inlineBox, inlineBox->caretLeftmostOffset());
    }

    if (nextBox->bidiLevel() <= level)
        return InlineBoxPosition(inlineBox, caretOffset);

    // Left edge of a "tertiary" run. Set to the right edge of that run.
    while (InlineBox* tertiaryBox = inlineBox->nextLeafChildIgnoringLineBreak()) {
        if (tertiaryBox->bidiLevel() <= level)
            break;
        inlineBox = tertiaryBox;
    }
    return InlineBoxPosition(inlineBox, inlineBox->caretRightmostOffset());
}

template <typename Strategy>
void PositionAlgorithm<Strategy>::debugPosition(const char* msg) const
{
    static const char* const anchorTypes[] = {
        "OffsetInAnchor",
        "BeforeAnchor",
        "AfterAnchor",
        "BeforeChildren",
        "AfterChildren",
        "Invalid",
    };

    if (isNull()) {
        fprintf(stderr, "Position [%s]: null\n", msg);
        return;
    }

    const char* anchorType = anchorTypes[std::min(static_cast<size_t>(m_anchorType), WTF_ARRAY_LENGTH(anchorTypes) - 1)];
    if (m_anchorNode->isTextNode()) {
        fprintf(stderr, "Position [%s]: %s [%p] %s, (%s) at %d\n", msg, anchorNode()->nodeName().utf8().data(), anchorNode(), anchorType, m_anchorNode->nodeValue().utf8().data(), m_offset);
        return;
    }

    fprintf(stderr, "Position [%s]: %s [%p] %s at %d\n", msg, anchorNode()->nodeName().utf8().data(), anchorNode(), anchorType, m_offset);
}

PositionInComposedTree toPositionInComposedTree(const Position& pos)
{
    if (pos.isNull())
        return PositionInComposedTree();

    PositionInComposedTree position;
    if (pos.isOffsetInAnchor()) {
        Node* anchor = pos.anchorNode();
        if (anchor->offsetInCharacters())
            return PositionInComposedTree(anchor, pos.computeOffsetInContainerNode());
        ASSERT(!isActiveInsertionPoint(*anchor));
        int offset = pos.computeOffsetInContainerNode();
        Node* child = NodeTraversal::childAt(*anchor, offset);
        if (!child) {
            if (anchor->isShadowRoot())
                return PositionInComposedTree(anchor->shadowHost(), PositionAnchorType::AfterChildren);
            return PositionInComposedTree(anchor, PositionAnchorType::AfterChildren);
        }
        child->updateDistribution();
        if (isActiveInsertionPoint(*child)) {
            if (anchor->isShadowRoot())
                return PositionInComposedTree(anchor->shadowHost(), offset);
            return PositionInComposedTree(anchor, offset);
        }
        return PositionInComposedTree(ComposedTreeTraversal::parent(*child), ComposedTreeTraversal::index(*child));
    }

    return PositionInComposedTree(pos.anchorNode(), pos.anchorType());
}

Position toPositionInDOMTree(const Position& position)
{
    return position;
}

Position toPositionInDOMTree(const PositionInComposedTree& position)
{
    if (position.isNull())
        return Position();

    Node* anchorNode = position.anchorNode();

    switch (position.anchorType()) {
    case PositionAnchorType::AfterChildren:
        // FIXME: When anchorNode is <img>, assertion fails in the constructor.
        return Position(anchorNode, PositionAnchorType::AfterChildren);
    case PositionAnchorType::AfterAnchor:
        return positionAfterNode(anchorNode);
    case PositionAnchorType::BeforeChildren:
        return Position(anchorNode, PositionAnchorType::BeforeChildren);
    case PositionAnchorType::BeforeAnchor:
        return positionBeforeNode(anchorNode);
    case PositionAnchorType::OffsetInAnchor: {
        int offset = position.offsetInContainerNode();
        if (anchorNode->offsetInCharacters())
            return Position(anchorNode, offset);
        Node* child = ComposedTreeTraversal::childAt(*anchorNode, offset);
        if (child)
            return Position(child->parentNode(), child->nodeIndex());
        if (!position.offsetInContainerNode())
            return Position(anchorNode, PositionAnchorType::BeforeChildren);

        // |child| is null when the position is at the end of the children.
        // <div>foo|</div>
        return Position(anchorNode, PositionAnchorType::AfterChildren);
    }
    default:
        ASSERT_NOT_REACHED();
        return Position();
    }
}

#ifndef NDEBUG

template <typename Strategy>
void PositionAlgorithm<Strategy>::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;

    if (isNull()) {
        result.appendLiteral("<null>");
    } else {
        char s[1024];
        result.appendLiteral("offset ");
        result.appendNumber(m_offset);
        result.appendLiteral(" of ");
        anchorNode()->formatForDebugger(s, sizeof(s));
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}

template <typename Strategy>
void PositionAlgorithm<Strategy>::showAnchorTypeAndOffset() const
{
    switch (anchorType()) {
    case PositionAnchorType::OffsetInAnchor:
        fputs("offset", stderr);
        break;
    case PositionAnchorType::BeforeChildren:
        fputs("beforeChildren", stderr);
        break;
    case PositionAnchorType::AfterChildren:
        fputs("afterChildren", stderr);
        break;
    case PositionAnchorType::BeforeAnchor:
        fputs("before", stderr);
        break;
    case PositionAnchorType::AfterAnchor:
        fputs("after", stderr);
        break;
    }
    fprintf(stderr, ", offset:%d\n", m_offset);
}

template <typename Strategy>
void PositionAlgorithm<Strategy>::showTreeForThis() const
{
    if (!anchorNode())
        return;
    anchorNode()->showTreeForThis();
    showAnchorTypeAndOffset();
}

template <typename Strategy>
void PositionAlgorithm<Strategy>::showTreeForThisInComposedTree() const
{
    if (!anchorNode())
        return;
    anchorNode()->showTreeForThisInComposedTree();
    showAnchorTypeAndOffset();
}

#endif

template class CORE_TEMPLATE_EXPORT PositionAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT PositionAlgorithm<EditingInComposedTreeStrategy>;

} // namespace blink

#ifndef NDEBUG

void showTree(const blink::Position& pos)
{
    pos.showTreeForThis();
}

void showTree(const blink::Position* pos)
{
    if (pos)
        pos->showTreeForThis();
    else
        fprintf(stderr, "Cannot showTree for (nil)\n");
}

#endif
