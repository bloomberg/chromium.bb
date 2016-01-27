/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/VisibleSelection.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/layout/LayoutObject.h"
#include "platform/geometry/LayoutPoint.h"
#include "wtf/Assertions.h"
#include "wtf/text/CString.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate()
    : m_affinity(TextAffinity::Downstream)
    , m_changeObserver(nullptr)
    , m_selectionType(NoSelection)
    , m_baseIsFirst(true)
    , m_isDirectional(false)
{
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(const PositionTemplate<Strategy>& pos, TextAffinity affinity, bool isDirectional)
    : VisibleSelectionTemplate(pos, pos, affinity, isDirectional)
{
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(const PositionTemplate<Strategy>& base, const PositionTemplate<Strategy>& extent, TextAffinity affinity, bool isDirectional)
    : m_base(base)
    , m_extent(extent)
    , m_affinity(affinity)
    , m_changeObserver(nullptr)
    , m_isDirectional(isDirectional)
{
    validate();
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(const PositionWithAffinityTemplate<Strategy>& pos, bool isDirectional)
    : VisibleSelectionTemplate(pos.position(), pos.affinity(), isDirectional)
{
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(const VisiblePositionTemplate<Strategy>& pos, bool isDirectional)
    : VisibleSelectionTemplate(pos, pos, isDirectional)
{
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(const VisiblePositionTemplate<Strategy>& base, const VisiblePositionTemplate<Strategy>& extent, bool isDirectional)
    : VisibleSelectionTemplate(base.deepEquivalent(), extent.deepEquivalent(), base.affinity(), isDirectional)
{
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(const EphemeralRangeTemplate<Strategy>& range, TextAffinity affinity, bool isDirectional)
    : VisibleSelectionTemplate(range.startPosition(), range.endPosition(), affinity, isDirectional)
{
}

template <typename Strategy>
static SelectionType computeSelectionType(const PositionTemplate<Strategy>& start, const PositionTemplate<Strategy>& end)
{
    if (start.isNull()) {
        ASSERT(end.isNull());
        return NoSelection;
    }
    if (start == end)
        return CaretSelection;
    // TODO(yosin) We should call |Document::updateLayout()| here for
    // |mostBackwardCaretPosition()|. However, we are here during
    // |Node::removeChild()|.
    start.anchorNode()->updateDistribution();
    end.anchorNode()->updateDistribution();
    if (mostBackwardCaretPosition(start) == mostBackwardCaretPosition(end))
        return CaretSelection;
    return RangeSelection;
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent,
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    TextAffinity affinity,
    bool isDirectional)
    : m_base(base)
    , m_extent(extent)
    , m_start(start)
    , m_end(end)
    , m_affinity(affinity)
    , m_changeObserver(nullptr) // Observer is associated with only one VisibleSelection, so this should not be copied.
    , m_selectionType(computeSelectionType(start, end))
    , m_baseIsFirst(base.isNull() || base.compareTo(extent) <= 0)
    , m_isDirectional(isDirectional)
{
    ASSERT(base.isNull() == extent.isNull());
    ASSERT(base.isNull() == start.isNull());
    ASSERT(base.isNull() == end.isNull());
    ASSERT(start.isNull() || start.compareTo(end) <= 0);
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(const VisibleSelectionTemplate<Strategy>& other)
    : m_base(other.m_base)
    , m_extent(other.m_extent)
    , m_start(other.m_start)
    , m_end(other.m_end)
    , m_affinity(other.m_affinity)
    , m_changeObserver(nullptr) // Observer is associated with only one VisibleSelection, so this should not be copied.
    , m_selectionType(other.m_selectionType)
    , m_baseIsFirst(other.m_baseIsFirst)
    , m_isDirectional(other.m_isDirectional)
{
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>& VisibleSelectionTemplate<Strategy>::operator=(const VisibleSelectionTemplate<Strategy>& other)
{
    didChange();

    m_base = other.m_base;
    m_extent = other.m_extent;
    m_start = other.m_start;
    m_end = other.m_end;
    m_affinity = other.m_affinity;
    m_changeObserver = nullptr;
    m_selectionType = other.m_selectionType;
    m_baseIsFirst = other.m_baseIsFirst;
    m_isDirectional = other.m_isDirectional;
    return *this;
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::createWithoutValidation(const PositionTemplate<Strategy>& base, const PositionTemplate<Strategy>& extent, const PositionTemplate<Strategy>& start, const PositionTemplate<Strategy>& end, TextAffinity affinity, bool isDirectional)
{
    return VisibleSelectionTemplate<Strategy>(base, extent, start, end, affinity, isDirectional);
}

#if !ENABLE(OILPAN)
template <typename Strategy>
VisibleSelectionTemplate<Strategy>::~VisibleSelectionTemplate()
{
    didChange();
}
#endif

template <typename Strategy>
VisibleSelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::selectionFromContentsOfNode(Node* node)
{
    ASSERT(!Strategy::editingIgnoresContent(node));
    return VisibleSelectionTemplate(PositionTemplate<Strategy>::firstPositionInNode(node), PositionTemplate<Strategy>::lastPositionInNode(node));
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setBase(const PositionTemplate<Strategy>& position)
{
    const PositionTemplate<Strategy> oldBase = m_base;
    m_base = position;
    validate();
    if (m_base != oldBase)
        didChange();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setBase(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    const PositionTemplate<Strategy> oldBase = m_base;
    m_base = visiblePosition.deepEquivalent();
    validate();
    if (m_base != oldBase)
        didChange();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setExtent(const PositionTemplate<Strategy>& position)
{
    const PositionTemplate<Strategy> oldExtent = m_extent;
    m_extent = position;
    validate();
    if (m_extent != oldExtent)
        didChange();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setExtent(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    const PositionTemplate<Strategy> oldExtent = m_extent;
    m_extent = visiblePosition.deepEquivalent();
    validate();
    if (m_extent != oldExtent)
        didChange();
}

EphemeralRange firstEphemeralRangeOf(const VisibleSelection& selection)
{
    if (selection.isNone())
        return EphemeralRange();
    Position start = selection.start().parentAnchoredEquivalent();
    Position end = selection.end().parentAnchoredEquivalent();
    return EphemeralRange(start, end);
}

PassRefPtrWillBeRawPtr<Range> firstRangeOf(const VisibleSelection& selection)
{
    return createRange(firstEphemeralRangeOf(selection));
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy> VisibleSelectionTemplate<Strategy>::toNormalizedEphemeralRange() const
{
    if (isNone())
        return EphemeralRangeTemplate<Strategy>();

    // Make sure we have an updated layout since this function is called
    // in the course of running edit commands which modify the DOM.
    // Failing to call this can result in equivalentXXXPosition calls returning
    // incorrect results.
    m_start.document()->updateLayout();

    // Check again, because updating layout can clear the selection.
    if (isNone())
        return EphemeralRangeTemplate<Strategy>();

    if (isCaret()) {
        // If the selection is a caret, move the range start upstream. This
        // helps us match the conventions of text editors tested, which make
        // style determinations based on the character before the caret, if any.
        const PositionTemplate<Strategy> start = mostBackwardCaretPosition(m_start).parentAnchoredEquivalent();
        return EphemeralRangeTemplate<Strategy>(start, start);
    }
    // If the selection is a range, select the minimum range that encompasses
    // the selection. Again, this is to match the conventions of text editors
    // tested, which make style determinations based on the first character of
    // the selection. For instance, this operation helps to make sure that the
    // "X" selected below is the only thing selected. The range should not be
    // allowed to "leak" out to the end of the previous text node, or to the
    // beginning of the next text node, each of which has a different style.
    //
    // On a treasure map, <b>X</b> marks the spot.
    //                       ^ selected
    //
    ASSERT(isRange());
    return normalizeRange(EphemeralRangeTemplate<Strategy>(m_start, m_end));
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::expandUsingGranularity(TextGranularity granularity)
{
    if (isNone())
        return false;

    // TODO(yosin) Do we need to check all of them?
    const PositionTemplate<Strategy>oldBase = m_base;
    const PositionTemplate<Strategy>oldExtent = m_extent;
    const PositionTemplate<Strategy>oldStart = m_start;
    const PositionTemplate<Strategy>oldEnd = m_end;
    validate(granularity);
    if (m_base != oldBase || m_extent != oldExtent || m_start != oldStart || m_end != oldEnd)
        didChange();
    return true;
}

template <typename Strategy>
static EphemeralRangeTemplate<Strategy> makeSearchRange(const PositionTemplate<Strategy>& pos)
{
    Node* node = pos.anchorNode();
    if (!node)
        return EphemeralRangeTemplate<Strategy>();
    Document& document = node->document();
    if (!document.documentElement())
        return EphemeralRangeTemplate<Strategy>();
    Element* boundary = enclosingBlockFlowElement(*node);
    if (!boundary)
        return EphemeralRangeTemplate<Strategy>();

    return EphemeralRangeTemplate<Strategy>(pos, PositionTemplate<Strategy>::lastPositionInNode(boundary));
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::appendTrailingWhitespace()
{
    const EphemeralRangeTemplate<Strategy> searchRange = makeSearchRange(end());
    if (searchRange.isNull())
        return;

    CharacterIteratorAlgorithm<Strategy> charIt(searchRange.startPosition(), searchRange.endPosition(), TextIteratorEmitsCharactersBetweenAllVisiblePositions);
    bool changed = false;

    for (; charIt.length(); charIt.advance(1)) {
        UChar c = charIt.characterAt(0);
        if ((!isSpaceOrNewline(c) && c != noBreakSpaceCharacter) || c == '\n')
            break;
        m_end = charIt.endPosition();
        changed = true;
    }
    if (changed)
        didChange();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setBaseAndExtentToDeepEquivalents()
{
    // Move the selection to rendered positions, if possible.
    bool baseAndExtentEqual = m_base == m_extent;
    if (m_base.isNotNull()) {
        m_base = createVisiblePosition(m_base, m_affinity).deepEquivalent();
        if (baseAndExtentEqual)
            m_extent = m_base;
    }
    if (m_extent.isNotNull() && !baseAndExtentEqual)
        m_extent = createVisiblePosition(m_extent, m_affinity).deepEquivalent();

    // Make sure we do not have a dangling base or extent.
    if (m_base.isNull() && m_extent.isNull()) {
        m_baseIsFirst = true;
    } else if (m_base.isNull()) {
        m_base = m_extent;
        m_baseIsFirst = true;
    } else if (m_extent.isNull()) {
        m_extent = m_base;
        m_baseIsFirst = true;
    } else {
        m_baseIsFirst = m_base.compareTo(m_extent) <= 0;
    }
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setStartRespectingGranularity(TextGranularity granularity, EWordSide wordSide)
{
    ASSERT(m_base.isNotNull());
    ASSERT(m_extent.isNotNull());

    m_start = m_baseIsFirst ? m_base : m_extent;

    switch (granularity) {
    case CharacterGranularity:
        // Don't do any expansion.
        break;
    case WordGranularity: {
        // General case: Select the word the caret is positioned inside of.
        // If the caret is on the word boundary, select the word according to |wordSide|.
        // Edge case: If the caret is after the last word in a soft-wrapped line or the last word in
        // the document, select that last word (LeftWordIfOnBoundary).
        // Edge case: If the caret is after the last word in a paragraph, select from the the end of the
        // last word to the line break (also RightWordIfOnBoundary);
        const VisiblePositionTemplate<Strategy> visibleStart = createVisiblePosition(m_start, m_affinity);
        EWordSide side = wordSide;
        if (isEndOfEditableOrNonEditableContent(visibleStart) || (isEndOfLine(visibleStart) && !isStartOfLine(visibleStart) && !isEndOfParagraph(visibleStart)))
            side = LeftWordIfOnBoundary;
        m_start = startOfWord(visibleStart, side).deepEquivalent();
        break;
    }
    case SentenceGranularity: {
        m_start = startOfSentence(createVisiblePosition(m_start, m_affinity)).deepEquivalent();
        break;
    }
    case LineGranularity: {
        m_start = startOfLine(createVisiblePosition(m_start, m_affinity)).deepEquivalent();
        break;
    }
    case LineBoundary:
        m_start = startOfLine(createVisiblePosition(m_start, m_affinity)).deepEquivalent();
        break;
    case ParagraphGranularity: {
        VisiblePositionTemplate<Strategy> pos = createVisiblePosition(m_start, m_affinity);
        if (isStartOfLine(pos) && isEndOfEditableOrNonEditableContent(pos))
            pos = previousPositionOf(pos);
        m_start = startOfParagraph(pos).deepEquivalent();
        break;
    }
    case DocumentBoundary:
        m_start = startOfDocument(createVisiblePosition(m_start, m_affinity)).deepEquivalent();
        break;
    case ParagraphBoundary:
        m_start = startOfParagraph(createVisiblePosition(m_start, m_affinity)).deepEquivalent();
        break;
    case SentenceBoundary:
        m_start = startOfSentence(createVisiblePosition(m_start, m_affinity)).deepEquivalent();
        break;
    }

    // Make sure we do not have a Null position.
    if (m_start.isNull())
        m_start = m_baseIsFirst ? m_base : m_extent;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setEndRespectingGranularity(TextGranularity granularity, EWordSide wordSide)
{
    ASSERT(m_base.isNotNull());
    ASSERT(m_extent.isNotNull());

    m_end = m_baseIsFirst ? m_extent : m_base;

    switch (granularity) {
    case CharacterGranularity:
        // Don't do any expansion.
        break;
    case WordGranularity: {
        // General case: Select the word the caret is positioned inside of.
        // If the caret is on the word boundary, select the word according to
        // |wordSide|.
        // Edge case: If the caret is after the last word in a soft-wrapped line
        // or the last word in the document, select that last word
        // (|LeftWordIfOnBoundary|).
        // Edge case: If the caret is after the last word in a paragraph, select
        // from the the end of the last word to the line break (also
        // |RightWordIfOnBoundary|);
        const VisiblePositionTemplate<Strategy> originalEnd = createVisiblePosition(m_end, m_affinity);
        EWordSide side = wordSide;
        if (isEndOfEditableOrNonEditableContent(originalEnd) || (isEndOfLine(originalEnd) && !isStartOfLine(originalEnd) && !isEndOfParagraph(originalEnd)))
            side = LeftWordIfOnBoundary;

        const VisiblePositionTemplate<Strategy> wordEnd = endOfWord(originalEnd, side);
        VisiblePositionTemplate<Strategy> end = wordEnd;

        if (isEndOfParagraph(originalEnd) && !isEmptyTableCell(m_start.anchorNode())) {
            // Select the paragraph break (the space from the end of a paragraph
            // to the start of the next one) to match TextEdit.
            end = nextPositionOf(wordEnd);

            if (Element* table = isFirstPositionAfterTable(end)) {
                // The paragraph break after the last paragraph in the last cell
                // of a block table ends at the start of the paragraph after the
                // table.
                if (isEnclosingBlock(table))
                    end = nextPositionOf(end, CannotCrossEditingBoundary);
                else
                    end = wordEnd;
            }

            if (end.isNull())
                end = wordEnd;

        }

        m_end = end.deepEquivalent();
        break;
    }
    case SentenceGranularity: {
        m_end = endOfSentence(createVisiblePosition(m_end, m_affinity)).deepEquivalent();
        break;
    }
    case LineGranularity: {
        VisiblePositionTemplate<Strategy> end = endOfLine(createVisiblePosition(m_end, m_affinity));
        // If the end of this line is at the end of a paragraph, include the
        // space after the end of the line in the selection.
        if (isEndOfParagraph(end)) {
            VisiblePositionTemplate<Strategy> next = nextPositionOf(end);
            if (next.isNotNull())
                end = next;
        }
        m_end = end.deepEquivalent();
        break;
    }
    case LineBoundary:
        m_end = endOfLine(createVisiblePosition(m_end, m_affinity)).deepEquivalent();
        break;
    case ParagraphGranularity: {
        const VisiblePositionTemplate<Strategy> visibleParagraphEnd = endOfParagraph(createVisiblePosition(m_end, m_affinity));

        // Include the "paragraph break" (the space from the end of this
        // paragraph to the start of the next one) in the selection.
        VisiblePositionTemplate<Strategy> end = nextPositionOf(visibleParagraphEnd);

        if (Element* table = isFirstPositionAfterTable(end)) {
            // The paragraph break after the last paragraph in the last cell of
            // a block table ends at the start of the paragraph after the table,
            // not at the position just after the table.
            if (isEnclosingBlock(table)) {
                end = nextPositionOf(end, CannotCrossEditingBoundary);
            } else {
                // There is no parargraph break after the last paragraph in the
                // last cell of an inline table.
                end = visibleParagraphEnd;
            }
        }

        if (end.isNull())
            end = visibleParagraphEnd;

        m_end = end.deepEquivalent();
        break;
    }
    case DocumentBoundary:
        m_end = endOfDocument(createVisiblePosition(m_end, m_affinity)).deepEquivalent();
        break;
    case ParagraphBoundary:
        m_end = endOfParagraph(createVisiblePosition(m_end, m_affinity)).deepEquivalent();
        break;
    case SentenceBoundary:
        m_end = endOfSentence(createVisiblePosition(m_end, m_affinity)).deepEquivalent();
        break;
    }

    // Make sure we do not have a Null position.
    if (m_end.isNull())
        m_end = m_baseIsFirst ? m_extent : m_base;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::updateSelectionType()
{
    m_selectionType = computeSelectionType(m_start, m_end);

    // Affinity only makes sense for a caret
    if (m_selectionType != CaretSelection)
        m_affinity = TextAffinity::Downstream;
}

static Node* enclosingShadowHost(Node* node)
{
    for (Node* runner = node; runner; runner = ComposedTreeTraversal::parent(*runner)) {
        if (isShadowHost(runner))
            return runner;
    }
    return nullptr;
}

static bool isEnclosedBy(const PositionInComposedTree& position, const Node& node)
{
    ASSERT(position.isNotNull());
    Node* anchorNode = position.anchorNode();
    if (anchorNode == node)
        return !position.isAfterAnchor() && !position.isBeforeAnchor();

    return ComposedTreeTraversal::isDescendantOf(*anchorNode, node);
}

static bool isSelectionBoundary(const Node& node)
{
    return isHTMLTextAreaElement(node) || isHTMLInputElement(node) || isHTMLSelectElement(node);
}

static Node* enclosingShadowHostForStart(const PositionInComposedTree& position)
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

static Node* enclosingShadowHostForEnd(const PositionInComposedTree& position)
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

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::validate(TextGranularity granularity)
{
    setBaseAndExtentToDeepEquivalents();
    if (m_base.isNull() || m_extent.isNull()) {
        m_base = m_extent = m_start = m_end = PositionTemplate<Strategy>();
        updateSelectionType();
        return;
    }

    m_start = m_baseIsFirst ? m_base : m_extent;
    m_end = m_baseIsFirst ? m_extent : m_base;
    setStartRespectingGranularity(granularity);
    ASSERT(m_start.isNotNull());
    setEndRespectingGranularity(granularity);
    ASSERT(m_end.isNotNull());
    adjustSelectionToAvoidCrossingShadowBoundaries();
    adjustSelectionToAvoidCrossingEditingBoundaries();
    updateSelectionType();

    if (selectionType() == RangeSelection) {
        // "Constrain" the selection to be the smallest equivalent range of
        // nodes. This is a somewhat arbitrary choice, but experience shows that
        // it is useful to make to make the selection "canonical" (if only for
        // purposes of comparing selections). This is an ideal point of the code
        // to do this operation, since all selection changes that result in a
        // RANGE come through here before anyone uses it.
        // TODO(yosin) Canonicalizing is good, but haven't we already done it
        // (when we set these two positions to |VisiblePosition|
        // |deepEquivalent()|s above)?
        m_start = mostForwardCaretPosition(m_start);
        m_end = mostBackwardCaretPosition(m_end);
    }
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::isValidFor(const Document& document) const
{
    if (isNone())
        return true;

    return m_base.document() == &document
        && !m_base.isOrphan() && !m_extent.isOrphan()
        && !m_start.isOrphan() && !m_end.isOrphan();
}

// TODO(yosin) This function breaks the invariant of this class.
// But because we use VisibleSelection to store values in editing commands for
// use when undoing the command, we need to be able to create a selection that
// while currently invalid, will be valid once the changes are undone. This is a
// design problem. To fix it we either need to change the invariants of
// |VisibleSelection| or create a new class for editing to use that can
// manipulate selections that are not currently valid.
template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setWithoutValidation(const PositionTemplate<Strategy>& base, const PositionTemplate<Strategy>& extent)
{
    if (base.isNull() || extent.isNull()) {
        m_base = m_extent = m_start = m_end = PositionTemplate<Strategy>();
        updateSelectionType();
        return;
    }

    m_base = base;
    m_extent = extent;
    m_baseIsFirst = base.compareTo(extent) <= 0;
    if (m_baseIsFirst) {
        m_start = base;
        m_end = extent;
    } else {
        m_start = extent;
        m_end = base;
    }
    m_selectionType = base == extent ? CaretSelection : RangeSelection;
    if (m_selectionType != CaretSelection) {
        // Since |m_affinity| for non-|CaretSelection| is always |Downstream|,
        // we should keep this invariant. Note: This function can be called with
        // |m_affinity| is |TextAffinity::Upstream|.
        m_affinity = TextAffinity::Downstream;
    }
    didChange();
}

static PositionInComposedTree adjustPositionInComposedTreeForStart(const PositionInComposedTree& position, Node* shadowHost)
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

static Position adjustPositionForEnd(const Position& currentPosition, Node* startContainerNode)
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

static PositionInComposedTree adjustPositionInComposedTreeForEnd(const PositionInComposedTree& position, Node* shadowHost)
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

static Position adjustPositionForStart(const Position& currentPosition, Node* endContainerNode)
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

static VisibleSelection computeSelectionToAvoidCrossingShadowBoundaries(const VisibleSelection& selection)
{
    // Note: |m_selectionType| isn't computed yet.
    ASSERT(selection.base().isNotNull());
    ASSERT(selection.extent().isNotNull());
    ASSERT(selection.start().isNotNull());
    ASSERT(selection.end().isNotNull());

    // TODO(hajimehoshi): Checking treeScope is wrong when a node is
    // distributed, but we leave it as it is for backward compatibility.
    if (selection.start().anchorNode()->treeScope() == selection.end().anchorNode()->treeScope())
        return selection;

    if (selection.isBaseFirst()) {
        const Position newEnd = adjustPositionForEnd(selection.end(), selection.start().computeContainerNode());
        return VisibleSelection::createWithoutValidation(selection.base(), newEnd, selection.start(), newEnd, selection.affinity(), selection.isDirectional());
    }

    const Position newStart = adjustPositionForStart(selection.start(), selection.end().computeContainerNode());
    return VisibleSelection::createWithoutValidation(selection.base(), newStart, newStart, selection.end(), selection.affinity(), selection.isDirectional());
}

// This function is called twice. The first is called when |m_start| and |m_end|
// or |m_extent| are same, and the second when |m_start| and |m_end| are changed
// after downstream/upstream.
static VisibleSelectionInComposedTree computeSelectionToAvoidCrossingShadowBoundaries(const VisibleSelectionInComposedTree& selection)
{
    Node* shadowHostStart = enclosingShadowHostForStart(selection.start());
    Node* shadowHostEnd = enclosingShadowHostForEnd(selection.end());
    if (shadowHostStart == shadowHostEnd)
        return selection;

    if (selection.isBaseFirst()) {
        Node* shadowHost = shadowHostStart ? shadowHostStart : shadowHostEnd;
        const PositionInComposedTree newEnd = adjustPositionInComposedTreeForEnd(selection.end(), shadowHost);
        return VisibleSelectionInComposedTree::createWithoutValidation(selection.base(), newEnd, selection.start(), newEnd, selection.affinity(), selection.isDirectional());
    }
    Node* shadowHost = shadowHostEnd ? shadowHostEnd : shadowHostStart;
    const PositionInComposedTree newStart = adjustPositionInComposedTreeForStart(selection.start(), shadowHost);
    return VisibleSelectionInComposedTree::createWithoutValidation(selection.base(), newStart, newStart, selection.end(), selection.affinity(), selection.isDirectional());
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::adjustSelectionToAvoidCrossingShadowBoundaries()
{
    if (m_base.isNull() || m_start.isNull() || m_base.isNull())
        return;
    *this = computeSelectionToAvoidCrossingShadowBoundaries(*this);
}

static Element* lowestEditableAncestor(Node* node)
{
    while (node) {
        if (node->hasEditableStyle())
            return node->rootEditableElement();
        if (isHTMLBodyElement(*node))
            break;
        node = node->parentNode();
    }

    return nullptr;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::adjustSelectionToAvoidCrossingEditingBoundaries()
{
    if (m_base.isNull() || m_start.isNull() || m_end.isNull())
        return;

    ContainerNode* baseRoot = highestEditableRoot(m_base);
    ContainerNode* startRoot = highestEditableRoot(m_start);
    ContainerNode* endRoot = highestEditableRoot(m_end);

    Element* baseEditableAncestor = lowestEditableAncestor(m_base.computeContainerNode());

    // The base, start and end are all in the same region.  No adjustment necessary.
    if (baseRoot == startRoot && baseRoot == endRoot)
        return;

    // The selection is based in editable content.
    if (baseRoot) {
        // If the start is outside the base's editable root, cap it at the start of that root.
        // If the start is in non-editable content that is inside the base's editable root, put it
        // at the first editable position after start inside the base's editable root.
        if (startRoot != baseRoot) {
            const VisiblePositionTemplate<Strategy> first = firstEditableVisiblePositionAfterPositionInRoot(m_start, *baseRoot);
            m_start = first.deepEquivalent();
            if (m_start.isNull()) {
                ASSERT_NOT_REACHED();
                m_start = m_end;
            }
        }
        // If the end is outside the base's editable root, cap it at the end of that root.
        // If the end is in non-editable content that is inside the base's root, put it
        // at the last editable position before the end inside the base's root.
        if (endRoot != baseRoot) {
            const VisiblePositionTemplate<Strategy> last = lastEditableVisiblePositionBeforePositionInRoot(m_end, *baseRoot);
            m_end = last.deepEquivalent();
            if (m_end.isNull())
                m_end = m_start;
        }
    // The selection is based in non-editable content.
    } else {
        // FIXME: Non-editable pieces inside editable content should be atomic, in the same way that editable
        // pieces in non-editable content are atomic.

        // The selection ends in editable content or non-editable content inside a different editable ancestor,
        // move backward until non-editable content inside the same lowest editable ancestor is reached.
        Element* endEditableAncestor = lowestEditableAncestor(m_end.computeContainerNode());
        if (endRoot || endEditableAncestor != baseEditableAncestor) {

            PositionTemplate<Strategy>p = previousVisuallyDistinctCandidate(m_end);
            Element* shadowAncestor = endRoot ? endRoot->shadowHost() : nullptr;
            if (p.isNull() && shadowAncestor)
                p = PositionTemplate<Strategy>::afterNode(shadowAncestor);
            while (p.isNotNull() && !(lowestEditableAncestor(p.computeContainerNode()) == baseEditableAncestor && !isEditablePosition(p))) {
                Element* root = rootEditableElementOf(p);
                shadowAncestor = root ? root->shadowHost() : nullptr;
                p = isAtomicNode(p.computeContainerNode()) ? PositionTemplate<Strategy>::inParentBeforeNode(*p.computeContainerNode()) : previousVisuallyDistinctCandidate(p);
                if (p.isNull() && shadowAncestor)
                    p = PositionTemplate<Strategy>::afterNode(shadowAncestor);
            }
            const VisiblePositionTemplate<Strategy> previous = createVisiblePosition(p);

            if (previous.isNull()) {
                // The selection crosses an Editing boundary.  This is a
                // programmer error in the editing code.  Happy debugging!
                ASSERT_NOT_REACHED();
                m_base = PositionTemplate<Strategy>();
                m_extent = PositionTemplate<Strategy>();
                validate();
                return;
            }
            m_end = previous.deepEquivalent();
        }

        // The selection starts in editable content or non-editable content inside a different editable ancestor,
        // move forward until non-editable content inside the same lowest editable ancestor is reached.
        Element* startEditableAncestor = lowestEditableAncestor(m_start.computeContainerNode());
        if (startRoot || startEditableAncestor != baseEditableAncestor) {
            PositionTemplate<Strategy> p = nextVisuallyDistinctCandidate(m_start);
            Element* shadowAncestor = startRoot ? startRoot->shadowHost() : nullptr;
            if (p.isNull() && shadowAncestor)
                p = PositionTemplate<Strategy>::beforeNode(shadowAncestor);
            while (p.isNotNull() && !(lowestEditableAncestor(p.computeContainerNode()) == baseEditableAncestor && !isEditablePosition(p))) {
                Element* root = rootEditableElementOf(p);
                shadowAncestor = root ? root->shadowHost() : nullptr;
                p = isAtomicNode(p.computeContainerNode()) ? PositionTemplate<Strategy>::inParentAfterNode(*p.computeContainerNode()) : nextVisuallyDistinctCandidate(p);
                if (p.isNull() && shadowAncestor)
                    p = PositionTemplate<Strategy>::beforeNode(shadowAncestor);
            }
            const VisiblePositionTemplate<Strategy> next = createVisiblePosition(p);

            if (next.isNull()) {
                // The selection crosses an Editing boundary.  This is a
                // programmer error in the editing code.  Happy debugging!
                ASSERT_NOT_REACHED();
                m_base = PositionTemplate<Strategy>();
                m_extent = PositionTemplate<Strategy>();
                validate();
                return;
            }
            m_start = next.deepEquivalent();
        }
    }

    // Correct the extent if necessary.
    if (baseEditableAncestor != lowestEditableAncestor(m_extent.computeContainerNode()))
        m_extent = m_baseIsFirst ? m_end : m_start;
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::visiblePositionRespectingEditingBoundary(const LayoutPoint& localPoint, Node* targetNode) const
{
    return createVisiblePosition(positionRespectingEditingBoundary(localPoint, targetNode));
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> VisibleSelectionTemplate<Strategy>::positionRespectingEditingBoundary(const LayoutPoint& localPoint, Node* targetNode) const
{
    if (!targetNode->layoutObject())
        return PositionWithAffinityTemplate<Strategy>();

    LayoutPoint selectionEndPoint = localPoint;
    Element* editableElement = rootEditableElement();

    if (editableElement && !editableElement->contains(targetNode)) {
        if (!editableElement->layoutObject())
            return PositionWithAffinityTemplate<Strategy>();

        FloatPoint absolutePoint = targetNode->layoutObject()->localToAbsolute(FloatPoint(selectionEndPoint));
        selectionEndPoint = roundedLayoutPoint(editableElement->layoutObject()->absoluteToLocal(absolutePoint));
        targetNode = editableElement;
    }

    return fromPositionInDOMTree<Strategy>(targetNode->layoutObject()->positionForPoint(selectionEndPoint));
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::isContentEditable() const
{
    return isEditablePosition(start());
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::hasEditableStyle() const
{
    return isEditablePosition(start(), ContentIsEditable, DoNotUpdateStyle);
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::isContentRichlyEditable() const
{
    return isRichlyEditablePosition(toPositionInDOMTree(start()));
}

template <typename Strategy>
Element* VisibleSelectionTemplate<Strategy>::rootEditableElement() const
{
    return rootEditableElementOf(start());
}

template <typename Strategy>
Node* VisibleSelectionTemplate<Strategy>::nonBoundaryShadowTreeRootNode() const
{
    return start().anchorNode() && !start().anchorNode()->isShadowRoot() ? start().anchorNode()->nonBoundaryShadowTreeRootNode() : 0;
}

VisibleSelectionChangeObserver::VisibleSelectionChangeObserver()
{
}

VisibleSelectionChangeObserver::~VisibleSelectionChangeObserver()
{
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setChangeObserver(VisibleSelectionChangeObserver& observer)
{
    ASSERT(!m_changeObserver);
    m_changeObserver = &observer;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::clearChangeObserver()
{
    ASSERT(m_changeObserver);
    m_changeObserver = nullptr;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::didChange()
{
    if (m_changeObserver)
        m_changeObserver->didChangeVisibleSelection();
}

template <typename Strategy>
static bool isValidPosition(const PositionTemplate<Strategy>& position)
{
    if (!position.inDocument())
        return false;

    if (!position.isOffsetInAnchor())
        return true;

    if (position.offsetInContainerNode() < 0)
        return false;

    const unsigned offset = static_cast<unsigned>(position.offsetInContainerNode());
    const unsigned nodeLength = position.anchorNode()->lengthOfContents();
    return offset <= nodeLength;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::updateIfNeeded()
{
    Document* document = m_base.document();
    if (!document)
        return;
    document->updateLayoutIgnorePendingStylesheets();
    validate();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::validatePositionsIfNeeded()
{
    if (!isValidPosition(m_base) || !isValidPosition(m_extent) || !isValidPosition(m_start) || !isValidPosition(m_end))
        validate();
}

template <typename Strategy>
static bool equalSelectionsAlgorithm(const VisibleSelectionTemplate<Strategy>& selection1, const VisibleSelectionTemplate<Strategy>& selection2)
{
    if (selection1.affinity() != selection2.affinity() || selection1.isDirectional() != selection2.isDirectional())
        return false;

    if (selection1.isNone())
        return selection2.isNone();

    const VisibleSelectionTemplate<Strategy> selectionWrapper1(selection1);
    const VisibleSelectionTemplate<Strategy> selectionWrapper2(selection2);

    return selectionWrapper1.start() == selectionWrapper2.start()
        && selectionWrapper1.end() == selectionWrapper2.end()
        && selectionWrapper1.base() == selectionWrapper2.base()
        && selectionWrapper1.extent() == selectionWrapper2.extent();
}

// TODO(yosin): We should use |operator==()| instead of
// |equalSelectionsInDOMTree()|.
bool equalSelectionsInDOMTree(const VisibleSelection& selection1, const VisibleSelection& selection2)
{
    return selection1 == selection2;
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::operator==(const VisibleSelectionTemplate<Strategy>& other) const
{
    return equalSelectionsAlgorithm<Strategy>(*this, other);
}

#ifndef NDEBUG

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::debugPosition(const char* message) const
{
    fprintf(stderr, "VisibleSelection (%s) ===============\n", message);

    if (m_baseIsFirst) {
        m_start.debugPosition("start: ");
        m_base.debugPosition("base: ");
        m_end.debugPosition("end: ");
        m_extent.debugPosition("extent: ");
    } else {
        m_start.debugPosition("start: ");
        m_extent.debugPosition("extent: ");
        m_end.debugPosition("end: ");
        m_base.debugPosition("base: ");
    }

    fprintf(stderr, "isDirectional=%s\n", isDirectional() ? "true" : "false");
    fprintf(stderr, "affinity=%s\n", affinity() == TextAffinity::Downstream ? "DOWNSTREaM" : affinity() == TextAffinity::Upstream ? "UPSTREAM" : "UNKNOWN");
    fprintf(stderr, "================================\n");
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;
    String s;

    if (isNone()) {
        result.appendLiteral("<none>");
    } else {
        const int FormatBufferSize = 1024;
        char s[FormatBufferSize];
        result.appendLiteral("from ");
        start().formatForDebugger(s, FormatBufferSize);
        result.append(s);
        result.appendLiteral(" to ");
        end().formatForDebugger(s, FormatBufferSize);
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::showTreeForThis() const
{
    if (start().anchorNode()) {
        start().anchorNode()->showTreeAndMark(start().anchorNode(), "S", end().anchorNode(), "E");
        fputs("start: ", stderr);
        start().showAnchorTypeAndOffset();
        fputs("end: ", stderr);
        end().showAnchorTypeAndOffset();
    }
}

#endif

template class CORE_TEMPLATE_EXPORT VisibleSelectionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT VisibleSelectionTemplate<EditingInComposedTreeStrategy>;

} // namespace blink

#ifndef NDEBUG

void showTree(const blink::VisibleSelection& sel)
{
    sel.showTreeForThis();
}

void showTree(const blink::VisibleSelection* sel)
{
    if (sel)
        sel->showTreeForThis();
}

void showTree(const blink::VisibleSelectionInComposedTree& sel)
{
    sel.showTreeForThis();
}

void showTree(const blink::VisibleSelectionInComposedTree* sel)
{
    if (sel)
        sel->showTreeForThis();
}
#endif
