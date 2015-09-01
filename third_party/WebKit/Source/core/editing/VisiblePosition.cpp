/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
#include "core/editing/VisiblePosition.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/line/RootInlineBox.h"
#include "platform/geometry/FloatQuad.h"
#include "wtf/text/CString.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

using namespace HTMLNames;

VisiblePosition::VisiblePosition(const Position& position, TextAffinity affinity)
{
    // TODO(yosin) We should make |VisiblePosition| private and make this
    // constructor to populate member variables by using |createVisiblePosition()|.
    *this = createVisiblePosition(position, affinity);
}

VisiblePosition VisiblePosition::createWithoutCanonicalization(const PositionWithAffinity& canonicalized)
{
    VisiblePosition visiblePosition;
    visiblePosition.m_deepPosition = canonicalized.position();
    visiblePosition.m_affinity = canonicalized.affinity();
    return visiblePosition;
}

// TODO(yosin) We should move |rightVisuallyDistinctCandidate()| with
// |rightPositionOf()| to "VisibleUnits.cpp".
static Position leftVisuallyDistinctCandidate(const VisiblePosition& visiblePosition)
{
    const Position deepPosition = visiblePosition.deepEquivalent();
    Position p = deepPosition;
    if (p.isNull())
        return Position();

    Position downstreamStart = mostForwardCaretPosition(p);
    TextDirection primaryDirection = primaryDirectionOf(*p.anchorNode());
    const TextAffinity affinity = visiblePosition.affinity();

    while (true) {
        InlineBoxPosition boxPosition = computeInlineBoxPosition(p, affinity, primaryDirection);
        InlineBox* box = boxPosition.inlineBox;
        int offset = boxPosition.offsetInBox;
        if (!box)
            return primaryDirection == LTR ? previousVisuallyDistinctCandidate(deepPosition) : nextVisuallyDistinctCandidate(deepPosition);

        LayoutObject* layoutObject = &box->layoutObject();

        while (true) {
            if ((layoutObject->isReplaced() || layoutObject->isBR()) && offset == box->caretRightmostOffset())
                return box->isLeftToRightDirection() ? previousVisuallyDistinctCandidate(deepPosition) : nextVisuallyDistinctCandidate(deepPosition);

            if (!layoutObject->node()) {
                box = box->prevLeafChild();
                if (!box)
                    return primaryDirection == LTR ? previousVisuallyDistinctCandidate(deepPosition) : nextVisuallyDistinctCandidate(deepPosition);
                layoutObject = &box->layoutObject();
                offset = box->caretRightmostOffset();
                continue;
            }

            offset = box->isLeftToRightDirection() ? layoutObject->previousOffset(offset) : layoutObject->nextOffset(offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset < caretMinOffset : offset > caretMaxOffset) {
                // Overshot to the left.
                InlineBox* prevBox = box->prevLeafChildIgnoringLineBreak();
                if (!prevBox) {
                    Position positionOnLeft = primaryDirection == LTR ? previousVisuallyDistinctCandidate(deepPosition) : nextVisuallyDistinctCandidate(deepPosition);
                    if (positionOnLeft.isNull())
                        return Position();

                    InlineBox* boxOnLeft = computeInlineBoxPosition(positionOnLeft, affinity, primaryDirection).inlineBox;
                    if (boxOnLeft && boxOnLeft->root() == box->root())
                        return Position();
                    return positionOnLeft;
                }

                // Reposition at the other logical position corresponding to our edge's visual position and go for another round.
                box = prevBox;
                layoutObject = &box->layoutObject();
                offset = prevBox->caretRightmostOffset();
                continue;
            }

            ASSERT(offset == box->caretLeftmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* prevBox = box->prevLeafChild();

            if (box->direction() == primaryDirection) {
                if (!prevBox) {
                    InlineBox* logicalStart = 0;
                    if (primaryDirection == LTR ? box->root().getLogicalStartBoxWithNode(logicalStart) : box->root().getLogicalEndBoxWithNode(logicalStart)) {
                        box = logicalStart;
                        layoutObject = &box->layoutObject();
                        offset = primaryDirection == LTR ? box->caretMinOffset() : box->caretMaxOffset();
                    }
                    break;
                }
                if (prevBox->bidiLevel() >= level)
                    break;

                level = prevBox->bidiLevel();

                InlineBox* nextBox = box;
                do {
                    nextBox = nextBox->nextLeafChild();
                } while (nextBox && nextBox->bidiLevel() > level);

                if (nextBox && nextBox->bidiLevel() == level)
                    break;

                box = prevBox;
                layoutObject = &box->layoutObject();
                offset = box->caretRightmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            while (prevBox && !prevBox->layoutObject().node())
                prevBox = prevBox->prevLeafChild();

            if (prevBox) {
                box = prevBox;
                layoutObject = &box->layoutObject();
                offset = box->caretRightmostOffset();
                if (box->bidiLevel() > level) {
                    do {
                        prevBox = prevBox->prevLeafChild();
                    } while (prevBox && prevBox->bidiLevel() > level);

                    if (!prevBox || prevBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* nextBox = box->nextLeafChild()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* prevBox = box->prevLeafChild()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                layoutObject = &box->layoutObject();
                offset = primaryDirection == LTR ? box->caretMinOffset() : box->caretMaxOffset();
            }
            break;
        }

        p = Position::editingPositionOf(layoutObject->node(), offset);

        if ((isVisuallyEquivalentCandidate(p) && mostForwardCaretPosition(p) != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;

        ASSERT(p != deepPosition);
    }
}

// TODO(yosin) We should move |leftPositionOf()| to "VisibleUnits.cpp".
VisiblePosition leftPositionOf(const VisiblePosition& visiblePosition)
{
    const Position pos = leftVisuallyDistinctCandidate(visiblePosition);
    // TODO(yosin) Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree())
        return VisiblePosition();

    VisiblePosition left = VisiblePosition(pos);
    ASSERT(left.deepEquivalent() != visiblePosition.deepEquivalent());

    return directionOfEnclosingBlock(left.deepEquivalent()) == LTR ? honorEditingBoundaryAtOrBefore(left, visiblePosition.deepEquivalent()) : honorEditingBoundaryAtOrAfter(left, visiblePosition.deepEquivalent());
}

// TODO(yosin) We should move |rightVisuallyDistinctCandidate()| with
// |rightPositionOf()| to "VisibleUnits.cpp".
static Position rightVisuallyDistinctCandidate(const VisiblePosition& visiblePosition)
{
    const Position deepPosition = visiblePosition.deepEquivalent();
    Position p = deepPosition;
    if (p.isNull())
        return Position();

    Position downstreamStart = mostForwardCaretPosition(p);
    TextDirection primaryDirection = primaryDirectionOf(*p.anchorNode());
    const TextAffinity affinity = visiblePosition.affinity();

    while (true) {
        InlineBoxPosition boxPosition = computeInlineBoxPosition(p, affinity, primaryDirection);
        InlineBox* box = boxPosition.inlineBox;
        int offset = boxPosition.offsetInBox;
        if (!box)
            return primaryDirection == LTR ? nextVisuallyDistinctCandidate(deepPosition) : previousVisuallyDistinctCandidate(deepPosition);

        LayoutObject* layoutObject = &box->layoutObject();

        while (true) {
            if ((layoutObject->isReplaced() || layoutObject->isBR()) && offset == box->caretLeftmostOffset())
                return box->isLeftToRightDirection() ? nextVisuallyDistinctCandidate(deepPosition) : previousVisuallyDistinctCandidate(deepPosition);

            if (!layoutObject->node()) {
                box = box->nextLeafChild();
                if (!box)
                    return primaryDirection == LTR ? nextVisuallyDistinctCandidate(deepPosition) : previousVisuallyDistinctCandidate(deepPosition);
                layoutObject = &box->layoutObject();
                offset = box->caretLeftmostOffset();
                continue;
            }

            offset = box->isLeftToRightDirection() ? layoutObject->nextOffset(offset) : layoutObject->previousOffset(offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset > caretMaxOffset : offset < caretMinOffset) {
                // Overshot to the right.
                InlineBox* nextBox = box->nextLeafChildIgnoringLineBreak();
                if (!nextBox) {
                    Position positionOnRight = primaryDirection == LTR ? nextVisuallyDistinctCandidate(deepPosition) : previousVisuallyDistinctCandidate(deepPosition);
                    if (positionOnRight.isNull())
                        return Position();

                    InlineBox* boxOnRight = computeInlineBoxPosition(positionOnRight, affinity, primaryDirection).inlineBox;
                    if (boxOnRight && boxOnRight->root() == box->root())
                        return Position();
                    return positionOnRight;
                }

                // Reposition at the other logical position corresponding to our edge's visual position and go for another round.
                box = nextBox;
                layoutObject = &box->layoutObject();
                offset = nextBox->caretLeftmostOffset();
                continue;
            }

            ASSERT(offset == box->caretRightmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* nextBox = box->nextLeafChild();

            if (box->direction() == primaryDirection) {
                if (!nextBox) {
                    InlineBox* logicalEnd = 0;
                    if (primaryDirection == LTR ? box->root().getLogicalEndBoxWithNode(logicalEnd) : box->root().getLogicalStartBoxWithNode(logicalEnd)) {
                        box = logicalEnd;
                        layoutObject = &box->layoutObject();
                        offset = primaryDirection == LTR ? box->caretMaxOffset() : box->caretMinOffset();
                    }
                    break;
                }

                if (nextBox->bidiLevel() >= level)
                    break;

                level = nextBox->bidiLevel();

                InlineBox* prevBox = box;
                do {
                    prevBox = prevBox->prevLeafChild();
                } while (prevBox && prevBox->bidiLevel() > level);

                if (prevBox && prevBox->bidiLevel() == level) // For example, abc FED 123 ^ CBA
                    break;

                // For example, abc 123 ^ CBA or 123 ^ CBA abc
                box = nextBox;
                layoutObject = &box->layoutObject();
                offset = box->caretLeftmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            while (nextBox && !nextBox->layoutObject().node())
                nextBox = nextBox->nextLeafChild();

            if (nextBox) {
                box = nextBox;
                layoutObject = &box->layoutObject();
                offset = box->caretLeftmostOffset();

                if (box->bidiLevel() > level) {
                    do {
                        nextBox = nextBox->nextLeafChild();
                    } while (nextBox && nextBox->bidiLevel() > level);

                    if (!nextBox || nextBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* prevBox = box->prevLeafChild()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* nextBox = box->nextLeafChild()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                layoutObject = &box->layoutObject();
                offset = primaryDirection == LTR ? box->caretMaxOffset() : box->caretMinOffset();
            }
            break;
        }

        p = Position::editingPositionOf(layoutObject->node(), offset);

        if ((isVisuallyEquivalentCandidate(p) && mostForwardCaretPosition(p) != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;

        ASSERT(p != deepPosition);
    }
}

// TODO(yosin) We should move |rightPositionOf()| to "VisibleUnits.cpp".
VisiblePosition rightPositionOf(const VisiblePosition& visiblePosition)
{
    const Position pos = rightVisuallyDistinctCandidate(visiblePosition);
    // TODO(yosin) Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree())
        return VisiblePosition();

    VisiblePosition right = VisiblePosition(pos);
    ASSERT(right.deepEquivalent() != visiblePosition.deepEquivalent());

    return directionOfEnclosingBlock(right.deepEquivalent()) == LTR ? honorEditingBoundaryAtOrAfter(right, visiblePosition.deepEquivalent()) : honorEditingBoundaryAtOrBefore(right, visiblePosition.deepEquivalent());
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> honorEditingBoundaryAtOrBeforeAlgorithm(const PositionWithAffinityTemplate<Strategy>& pos, const PositionAlgorithm<Strategy>& anchor)
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(anchor);

    // Return empty position if pos is not somewhere inside the editable region containing this position
    if (highestRoot && !pos.position().anchorNode()->isDescendantOf(highestRoot))
        return PositionWithAffinityTemplate<Strategy>();

    // Return pos itself if the two are from the very same editable region, or both are non-editable
    // FIXME: In the non-editable case, just because the new position is non-editable doesn't mean movement
    // to it is allowed.  VisibleSelection::adjustForEditableContent has this problem too.
    if (highestEditableRoot(pos.position()) == highestRoot)
        return pos;

    // Return empty position if this position is non-editable, but pos is editable
    // FIXME: Move to the previous non-editable region.
    if (!highestRoot)
        return PositionWithAffinityTemplate<Strategy>();

    // Return the last position before pos that is in the same editable region as this position
    return lastEditablePositionBeforePositionInRoot(pos.position(), highestRoot);
}

PositionWithAffinity honorEditingBoundaryAtOrBeforeOf(const PositionWithAffinity& pos, const Position& anchor)
{
    return honorEditingBoundaryAtOrBeforeAlgorithm(pos, anchor);
}

PositionInComposedTreeWithAffinity honorEditingBoundaryAtOrBeforeOf(const PositionInComposedTreeWithAffinity& pos, const PositionInComposedTree& anchor)
{
    return honorEditingBoundaryAtOrBeforeAlgorithm(pos, anchor);
}

VisiblePosition honorEditingBoundaryAtOrBefore(const VisiblePosition& pos, const Position& anchor)
{
    return createVisiblePosition(honorEditingBoundaryAtOrBeforeOf(pos.toPositionWithAffinity(), anchor));
}

VisiblePosition honorEditingBoundaryAtOrAfter(const VisiblePosition& pos, const Position& anchor)
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(anchor);

    // Return empty position if pos is not somewhere inside the editable region containing this position
    if (highestRoot && !pos.deepEquivalent().anchorNode()->isDescendantOf(highestRoot))
        return VisiblePosition();

    // Return pos itself if the two are from the very same editable region, or both are non-editable
    // FIXME: In the non-editable case, just because the new position is non-editable doesn't mean movement
    // to it is allowed.  VisibleSelection::adjustForEditableContent has this problem too.
    if (highestEditableRoot(pos.deepEquivalent()) == highestRoot)
        return pos;

    // Return empty position if this position is non-editable, but pos is editable
    // FIXME: Move to the next non-editable region.
    if (!highestRoot)
        return VisiblePosition();

    // Return the next position after pos that is in the same editable region as this position
    return firstEditableVisiblePositionAfterPositionInRoot(pos.deepEquivalent(), highestRoot);
}

template<typename Strategy>
static PositionWithAffinityTemplate<Strategy> createVisiblePositionAlgorithm(const PositionAlgorithm<Strategy>& position, TextAffinity affinity)
{
    const PositionAlgorithm<Strategy> deepPosition = canonicalPositionOf(position);
    if (deepPosition.isNull())
        return PositionWithAffinityTemplate<Strategy>();
    if (affinity == TextAffinity::Downstream)
        return PositionWithAffinityTemplate<Strategy>(deepPosition);

    // When not at a line wrap, make sure to end up with
    // |TextAffinity::Downstream| affinity.
    if (inSameLine(PositionWithAffinityTemplate<Strategy>(deepPosition), PositionWithAffinityTemplate<Strategy>(deepPosition, TextAffinity::Upstream)))
        return PositionWithAffinityTemplate<Strategy>(deepPosition);
    return PositionWithAffinityTemplate<Strategy>(deepPosition, TextAffinity::Upstream);
}

VisiblePosition createVisiblePosition(const Position& position, TextAffinity affinity)
{
    return VisiblePosition::createWithoutCanonicalization(createVisiblePositionAlgorithm<EditingStrategy>(position, affinity));
}

VisiblePosition createVisiblePosition(const PositionWithAffinity& positionWithAffinity)
{
    return createVisiblePosition(positionWithAffinity.position(), positionWithAffinity.affinity());
}

VisiblePosition createVisiblePosition(const PositionInComposedTree& position, TextAffinity affinity)
{
    PositionInComposedTreeWithAffinity canonicalized = createVisiblePositionAlgorithm<EditingInComposedTreeStrategy>(position, affinity);
    return VisiblePosition::createWithoutCanonicalization(PositionWithAffinity(toPositionInDOMTree(canonicalized.position()), canonicalized.affinity()));
}

IntRect absoluteCaretBoundsOf(const VisiblePosition& visiblePosition)
{
    LayoutObject* layoutObject;
    LayoutRect localRect = localCaretRectOfPosition(visiblePosition.toPositionWithAffinity(), layoutObject);
    if (localRect.isEmpty() || !layoutObject)
        return IntRect();

    return layoutObject->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

#ifndef NDEBUG

void VisiblePosition::debugPosition(const char* msg) const
{
    if (isNull()) {
        fprintf(stderr, "Position [%s]: null\n", msg);
        return;
    }
    m_deepPosition.debugPosition(msg);
}

void VisiblePosition::formatForDebugger(char* buffer, unsigned length) const
{
    m_deepPosition.formatForDebugger(buffer, length);
}

void VisiblePosition::showTreeForThis() const
{
    m_deepPosition.showTreeForThis();
}

#endif

DEFINE_TRACE(VisiblePosition)
{
    visitor->trace(m_deepPosition);
}

} // namespace blink

#ifndef NDEBUG

void showTree(const blink::VisiblePosition* vpos)
{
    if (vpos)
        vpos->showTreeForThis();
    else
        fprintf(stderr, "Cannot showTree for (nil) VisiblePosition.\n");
}

void showTree(const blink::VisiblePosition& vpos)
{
    vpos.showTreeForThis();
}

#endif
