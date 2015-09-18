/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/editing/PendingSelection.h"

#include "core/dom/Document.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/layout/LayoutView.h"

namespace blink {

PendingSelection::PendingSelection(FrameSelection& frameSelection)
    : m_frameSelection(&frameSelection)
    , m_hasPendingSelection(false)
{
}

const VisibleSelection& PendingSelection::visibleSelection() const
{
    return m_frameSelection->selection();
}

template <typename SelectionStrategy>
bool PendingSelection::isInDocumentAlgorithm(const Document& document) const
{
    using Strategy = typename SelectionStrategy::Strategy;

    const PositionAlgorithm<Strategy> start = SelectionStrategy::selectionStart(visibleSelection());
    if (start.isNotNull() && (!start.inDocument() || start.document() != document))
        return false;
    const PositionAlgorithm<Strategy> end = SelectionStrategy::selectionEnd(visibleSelection());
    if (end.isNotNull() && (!end.inDocument() || end.document() != document))
        return false;
    const PositionAlgorithm<Strategy> extent = SelectionStrategy::selectionExtent(visibleSelection());
    if (extent.isNotNull() && (!extent.inDocument() || extent.document() != document))
        return false;
    return true;
}

bool PendingSelection::isInDocument(const Document& document) const
{
    if (RuntimeEnabledFeatures::selectionForComposedTreeEnabled())
        return isInDocumentAlgorithm<VisibleSelection::InComposedTree>(document);
    return isInDocumentAlgorithm<VisibleSelection::InDOMTree>(document);
}

template <typename SelectionStrategy>
VisibleSelection PendingSelection::calcVisibleSelectionAlgorithm() const
{
    using Strategy = typename SelectionStrategy::Strategy;

    const PositionAlgorithm<Strategy> start = SelectionStrategy::selectionStart(visibleSelection());
    const PositionAlgorithm<Strategy> end = SelectionStrategy::selectionEnd(visibleSelection());
    SelectionType selectionType = SelectionStrategy::selectionType(visibleSelection());
    const TextAffinity affinity = visibleSelection().affinity();

    bool paintBlockCursor = m_frameSelection->shouldShowBlockCursor() && selectionType == SelectionType::CaretSelection && !isLogicalEndOfLine(createVisiblePositionInDOMTree(end, affinity));
    VisibleSelection selection;
    if (enclosingTextFormControl(start.computeContainerNode())) {
        // TODO(yosin) We should use |PositionMoveType::Character| to avoid
        // ending paint at middle of character.
        PositionAlgorithm<Strategy> endPosition = paintBlockCursor ? nextPositionOf(SelectionStrategy::selectionExtent(visibleSelection()), PositionMoveType::CodePoint) : end;
        selection.setWithoutValidation(start, endPosition);
        return selection;
    }

    VisiblePosition visibleStart = createVisiblePositionInDOMTree(start, selectionType == SelectionType::RangeSelection ? TextAffinity::Downstream : affinity);
    if (paintBlockCursor) {
        VisiblePosition visibleExtent = createVisiblePositionInDOMTree(end, affinity);
        visibleExtent = nextPositionOf(visibleExtent, CanSkipOverEditingBoundary);
        return VisibleSelection(visibleStart, visibleExtent);
    }
    VisiblePosition visibleEnd = createVisiblePositionInDOMTree(end, selectionType == SelectionType::RangeSelection ? TextAffinity::Upstream : affinity);
    return VisibleSelection(visibleStart, visibleEnd);
}

template <typename SelectionStrategy>
void PendingSelection::commitAlgorithm(LayoutView& layoutView)
{
    using Strategy = typename SelectionStrategy::Strategy;

    if (!hasPendingSelection())
        return;
    ASSERT(!layoutView.needsLayout());
    m_hasPendingSelection = false;

    // Skip if pending VisibilePositions became invalid before we reach here.
    if (!isInDocument(layoutView.document()))
        return;

    // Construct a new VisibleSolution, since visibleSelection() is not necessarily
    // valid, and the following steps assume a valid selection.
    // See <https://bugs.webkit.org/show_bug.cgi?id=69563> and
    // <rdar://problem/10232866>.
    VisibleSelection selection = calcVisibleSelectionAlgorithm<SelectionStrategy>();

    if (!selection.isRange()) {
        layoutView.clearSelection();
        return;
    }

    // Use the rightmost candidate for the start of the selection, and the
    // leftmost candidate for the end of the selection. Example: foo <a>bar</a>.
    // Imagine that a line wrap occurs after 'foo', and that 'bar' is selected.
    // If we pass [foo, 3] as the start of the selection, the selection painting
    // code will think that content on the line containing 'foo' is selected
    // and will fill the gap before 'bar'.
    PositionAlgorithm<Strategy> startPos = SelectionStrategy::selectionStart(selection);
    PositionAlgorithm<Strategy> candidate = mostForwardCaretPosition(startPos);
    if (isVisuallyEquivalentCandidate(candidate))
        startPos = candidate;
    PositionAlgorithm<Strategy> endPos = SelectionStrategy::selectionEnd(selection);
    candidate = mostBackwardCaretPosition(endPos);
    if (isVisuallyEquivalentCandidate(candidate))
        endPos = candidate;

    // We can get into a state where the selection endpoints map to the same
    // |VisiblePosition| when a selection is deleted because we don't yet notify
    // the |FrameSelection| of text removal.
    if (startPos.isNull() || endPos.isNull() || SelectionStrategy::selectionVisibleStart(selection).deepEquivalent() == SelectionStrategy::selectionVisibleEnd(selection).deepEquivalent())
        return;
    LayoutObject* startLayoutObject = startPos.anchorNode()->layoutObject();
    LayoutObject* endLayoutObject = endPos.anchorNode()->layoutObject();
    if (!startLayoutObject || !endLayoutObject)
        return;
    ASSERT(layoutView == startLayoutObject->view() && layoutView == endLayoutObject->view());
    layoutView.setSelection(startLayoutObject, startPos.computeEditingOffset(), endLayoutObject, endPos.computeEditingOffset());
}

void PendingSelection::commit(LayoutView& layoutView)
{
    if (RuntimeEnabledFeatures::selectionForComposedTreeEnabled())
        return commitAlgorithm<VisibleSelection::InComposedTree>(layoutView);
    commitAlgorithm<VisibleSelection::InDOMTree>(layoutView);
}

DEFINE_TRACE(PendingSelection)
{
    visitor->trace(m_frameSelection);
}

} // namespace blink
