/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "core/editing/VisibleUnits.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/PositionIterator.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/iterators/BackwardsCharacterIterator.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/SimplifiedBackwardsTextIterator.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBRElement.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutView.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextBoundaries.h"

namespace blink {

using namespace HTMLNames;
using namespace WTF::Unicode;

static Node* previousLeafWithSameEditability(Node* node, EditableType editableType)
{
    bool editable = node->hasEditableStyle(editableType);
    node = previousAtomicLeafNode(*node);
    while (node) {
        if (editable == node->hasEditableStyle(editableType))
            return node;
        node = previousAtomicLeafNode(*node);
    }
    return 0;
}

static Node* nextLeafWithSameEditability(Node* node, EditableType editableType = ContentIsEditable)
{
    if (!node)
        return 0;

    bool editable = node->hasEditableStyle(editableType);
    node = nextAtomicLeafNode(*node);
    while (node) {
        if (editable == node->hasEditableStyle(editableType))
            return node;
        node = nextAtomicLeafNode(*node);
    }
    return 0;
}

// FIXME: consolidate with code in previousLinePosition.
static Position previousRootInlineBoxCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    ContainerNode* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* previousNode = previousLeafWithSameEditability(node, editableType);

    while (previousNode && (!previousNode->layoutObject() || inSameLine(VisiblePosition(firstPositionInOrBeforeNode(previousNode)), visiblePosition)))
        previousNode = previousLeafWithSameEditability(previousNode, editableType);

    while (previousNode && !previousNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(previousNode), editableType) != highestRoot)
            break;

        Position pos = isHTMLBRElement(*previousNode) ? positionBeforeNode(previousNode) :
            Position::editingPositionOf(previousNode, caretMaxOffset(previousNode));

        if (isVisuallyEquivalentCandidate(pos))
            return pos;

        previousNode = previousLeafWithSameEditability(previousNode, editableType);
    }
    return Position();
}

static Position nextRootInlineBoxCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    ContainerNode* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* nextNode = nextLeafWithSameEditability(node, editableType);
    while (nextNode && (!nextNode->layoutObject() || inSameLine(VisiblePosition(firstPositionInOrBeforeNode(nextNode)), visiblePosition)))
        nextNode = nextLeafWithSameEditability(nextNode, ContentIsEditable);

    while (nextNode && !nextNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(nextNode), editableType) != highestRoot)
            break;

        Position pos;
        pos = Position::editingPositionOf(nextNode, caretMinOffset(nextNode));

        if (isVisuallyEquivalentCandidate(pos))
            return pos;

        nextNode = nextLeafWithSameEditability(nextNode, editableType);
    }
    return Position();
}

class CachedLogicallyOrderedLeafBoxes {
public:
    CachedLogicallyOrderedLeafBoxes();

    const InlineTextBox* previousTextBox(const RootInlineBox*, const InlineTextBox*);
    const InlineTextBox* nextTextBox(const RootInlineBox*, const InlineTextBox*);

    size_t size() const { return m_leafBoxes.size(); }
    const InlineBox* firstBox() const { return m_leafBoxes[0]; }

private:
    const Vector<InlineBox*>& collectBoxes(const RootInlineBox*);
    int boxIndexInLeaves(const InlineTextBox*) const;

    const RootInlineBox* m_rootInlineBox;
    Vector<InlineBox*> m_leafBoxes;
};

CachedLogicallyOrderedLeafBoxes::CachedLogicallyOrderedLeafBoxes() : m_rootInlineBox(0) { }

const InlineTextBox* CachedLogicallyOrderedLeafBoxes::previousTextBox(const RootInlineBox* root, const InlineTextBox* box)
{
    if (!root)
        return 0;

    collectBoxes(root);

    // If box is null, root is box's previous RootInlineBox, and previousBox is the last logical box in root.
    int boxIndex = m_leafBoxes.size() - 1;
    if (box)
        boxIndex = boxIndexInLeaves(box) - 1;

    for (int i = boxIndex; i >= 0; --i) {
        if (m_leafBoxes[i]->isInlineTextBox())
            return toInlineTextBox(m_leafBoxes[i]);
    }

    return 0;
}

const InlineTextBox* CachedLogicallyOrderedLeafBoxes::nextTextBox(const RootInlineBox* root, const InlineTextBox* box)
{
    if (!root)
        return 0;

    collectBoxes(root);

    // If box is null, root is box's next RootInlineBox, and nextBox is the first logical box in root.
    // Otherwise, root is box's RootInlineBox, and nextBox is the next logical box in the same line.
    size_t nextBoxIndex = 0;
    if (box)
        nextBoxIndex = boxIndexInLeaves(box) + 1;

    for (size_t i = nextBoxIndex; i < m_leafBoxes.size(); ++i) {
        if (m_leafBoxes[i]->isInlineTextBox())
            return toInlineTextBox(m_leafBoxes[i]);
    }

    return 0;
}

const Vector<InlineBox*>& CachedLogicallyOrderedLeafBoxes::collectBoxes(const RootInlineBox* root)
{
    if (m_rootInlineBox != root) {
        m_rootInlineBox = root;
        m_leafBoxes.clear();
        root->collectLeafBoxesInLogicalOrder(m_leafBoxes);
    }
    return m_leafBoxes;
}

int CachedLogicallyOrderedLeafBoxes::boxIndexInLeaves(const InlineTextBox* box) const
{
    for (size_t i = 0; i < m_leafBoxes.size(); ++i) {
        if (box == m_leafBoxes[i])
            return i;
    }
    return 0;
}

static const InlineTextBox* logicallyPreviousBox(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& previousBoxInDifferentBlock, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    const InlineBox* startBox = textBox;

    const InlineTextBox* previousBox = leafBoxes.previousTextBox(&startBox->root(), textBox);
    if (previousBox)
        return previousBox;

    previousBox = leafBoxes.previousTextBox(startBox->root().prevRootBox(), 0);
    if (previousBox)
        return previousBox;

    while (1) {
        Node* startNode = startBox->layoutObject().nonPseudoNode();
        if (!startNode)
            break;

        Position position = previousRootInlineBoxCandidatePosition(startNode, visiblePosition, ContentIsEditable);
        if (position.isNull())
            break;

        RenderedPosition renderedPosition(position, TextAffinity::Downstream);
        RootInlineBox* previousRoot = renderedPosition.rootBox();
        if (!previousRoot)
            break;

        previousBox = leafBoxes.previousTextBox(previousRoot, 0);
        if (previousBox) {
            previousBoxInDifferentBlock = true;
            return previousBox;
        }

        if (!leafBoxes.size())
            break;
        startBox = leafBoxes.firstBox();
    }
    return 0;
}


static const InlineTextBox* logicallyNextBox(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& nextBoxInDifferentBlock, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    const InlineBox* startBox = textBox;

    const InlineTextBox* nextBox = leafBoxes.nextTextBox(&startBox->root(), textBox);
    if (nextBox)
        return nextBox;

    nextBox = leafBoxes.nextTextBox(startBox->root().nextRootBox(), 0);
    if (nextBox)
        return nextBox;

    while (1) {
        Node* startNode =startBox->layoutObject().nonPseudoNode();
        if (!startNode)
            break;

        Position position = nextRootInlineBoxCandidatePosition(startNode, visiblePosition, ContentIsEditable);
        if (position.isNull())
            break;

        RenderedPosition renderedPosition(position, TextAffinity::Downstream);
        RootInlineBox* nextRoot = renderedPosition.rootBox();
        if (!nextRoot)
            break;

        nextBox = leafBoxes.nextTextBox(nextRoot, 0);
        if (nextBox) {
            nextBoxInDifferentBlock = true;
            return nextBox;
        }

        if (!leafBoxes.size())
            break;
        startBox = leafBoxes.firstBox();
    }
    return 0;
}

static TextBreakIterator* wordBreakIteratorForMinOffsetBoundary(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    int& previousBoxLength, bool& previousBoxInDifferentBlock, Vector<UChar, 1024>& string, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    previousBoxInDifferentBlock = false;

    // FIXME: Handle the case when we don't have an inline text box.
    const InlineTextBox* previousBox = logicallyPreviousBox(visiblePosition, textBox, previousBoxInDifferentBlock, leafBoxes);

    int len = 0;
    string.clear();
    if (previousBox) {
        previousBoxLength = previousBox->len();
        previousBox->layoutObject().text().appendTo(string, previousBox->start(), previousBoxLength);
        len += previousBoxLength;
    }
    textBox->layoutObject().text().appendTo(string, textBox->start(), textBox->len());
    len += textBox->len();

    return wordBreakIterator(string.data(), len);
}

static TextBreakIterator* wordBreakIteratorForMaxOffsetBoundary(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& nextBoxInDifferentBlock, Vector<UChar, 1024>& string, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    nextBoxInDifferentBlock = false;

    // FIXME: Handle the case when we don't have an inline text box.
    const InlineTextBox* nextBox = logicallyNextBox(visiblePosition, textBox, nextBoxInDifferentBlock, leafBoxes);

    int len = 0;
    string.clear();
    textBox->layoutObject().text().appendTo(string, textBox->start(), textBox->len());
    len += textBox->len();
    if (nextBox) {
        nextBox->layoutObject().text().appendTo(string, nextBox->start(), nextBox->len());
        len += nextBox->len();
    }

    return wordBreakIterator(string.data(), len);
}

static bool isLogicalStartOfWord(TextBreakIterator* iter, int position, bool hardLineBreak)
{
    bool boundary = hardLineBreak ? true : iter->isBoundary(position);
    if (!boundary)
        return false;

    iter->following(position);
    // isWordTextBreak returns true after moving across a word and false after moving across a punctuation/space.
    return isWordTextBreak(iter);
}

static bool islogicalEndOfWord(TextBreakIterator* iter, int position, bool hardLineBreak)
{
    bool boundary = iter->isBoundary(position);
    return (hardLineBreak || boundary) && isWordTextBreak(iter);
}

enum CursorMovementDirection { MoveLeft, MoveRight };

static VisiblePosition visualWordPosition(const VisiblePosition& visiblePosition, CursorMovementDirection direction,
    bool skipsSpaceWhenMovingRight)
{
    if (visiblePosition.isNull())
        return VisiblePosition();

    TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
    InlineBox* previouslyVisitedBox = 0;
    VisiblePosition current = visiblePosition;
    TextBreakIterator* iter = 0;

    CachedLogicallyOrderedLeafBoxes leafBoxes;
    Vector<UChar, 1024> string;

    while (1) {
        VisiblePosition adjacentCharacterPosition = direction == MoveRight ? current.right() : current.left();
        if (adjacentCharacterPosition.deepEquivalent() == current.deepEquivalent() || adjacentCharacterPosition.isNull())
            return VisiblePosition();

        InlineBoxPosition boxPosition = computeInlineBoxPosition(adjacentCharacterPosition.deepEquivalent(), TextAffinity::Upstream);
        InlineBox* box = boxPosition.inlineBox;
        int offsetInBox = boxPosition.offsetInBox;

        if (!box)
            break;
        if (!box->isInlineTextBox()) {
            current = adjacentCharacterPosition;
            continue;
        }

        InlineTextBox* textBox = toInlineTextBox(box);
        int previousBoxLength = 0;
        bool previousBoxInDifferentBlock = false;
        bool nextBoxInDifferentBlock = false;
        bool movingIntoNewBox = previouslyVisitedBox != box;

        if (offsetInBox == box->caretMinOffset()) {
            iter = wordBreakIteratorForMinOffsetBoundary(visiblePosition, textBox, previousBoxLength, previousBoxInDifferentBlock, string, leafBoxes);
        } else if (offsetInBox == box->caretMaxOffset()) {
            iter = wordBreakIteratorForMaxOffsetBoundary(visiblePosition, textBox, nextBoxInDifferentBlock, string, leafBoxes);
        } else if (movingIntoNewBox) {
            iter = wordBreakIterator(textBox->layoutObject().text(), textBox->start(), textBox->len());
            previouslyVisitedBox = box;
        }

        if (!iter)
            break;

        iter->first();
        int offsetInIterator = offsetInBox - textBox->start() + previousBoxLength;

        bool isWordBreak;
        bool boxHasSameDirectionalityAsBlock = box->direction() == blockDirection;
        bool movingBackward = (direction == MoveLeft && box->direction() == LTR) || (direction == MoveRight && box->direction() == RTL);
        if ((skipsSpaceWhenMovingRight && boxHasSameDirectionalityAsBlock)
            || (!skipsSpaceWhenMovingRight && movingBackward)) {
            bool logicalStartInLayoutObject = offsetInBox == static_cast<int>(textBox->start()) && previousBoxInDifferentBlock;
            isWordBreak = isLogicalStartOfWord(iter, offsetInIterator, logicalStartInLayoutObject);
        } else {
            bool logicalEndInLayoutObject = offsetInBox == static_cast<int>(textBox->start() + textBox->len()) && nextBoxInDifferentBlock;
            isWordBreak = islogicalEndOfWord(iter, offsetInIterator, logicalEndInLayoutObject);
        }

        if (isWordBreak)
            return adjacentCharacterPosition;

        current = adjacentCharacterPosition;
    }
    return VisiblePosition();
}

VisiblePosition leftWordPosition(const VisiblePosition& visiblePosition, bool skipsSpaceWhenMovingRight)
{
    VisiblePosition leftWordBreak = visualWordPosition(visiblePosition, MoveLeft, skipsSpaceWhenMovingRight);
    leftWordBreak = visiblePosition.honorEditingBoundaryAtOrBefore(leftWordBreak);

    // FIXME: How should we handle a non-editable position?
    if (leftWordBreak.isNull() && isEditablePosition(visiblePosition.deepEquivalent())) {
        TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
        leftWordBreak = blockDirection == LTR ? startOfEditableContent(visiblePosition) : endOfEditableContent(visiblePosition);
    }
    return leftWordBreak;
}

VisiblePosition rightWordPosition(const VisiblePosition& visiblePosition, bool skipsSpaceWhenMovingRight)
{
    VisiblePosition rightWordBreak = visualWordPosition(visiblePosition, MoveRight, skipsSpaceWhenMovingRight);
    rightWordBreak = visiblePosition.honorEditingBoundaryAtOrBefore(rightWordBreak);

    // FIXME: How should we handle a non-editable position?
    if (rightWordBreak.isNull() && isEditablePosition(visiblePosition.deepEquivalent())) {
        TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
        rightWordBreak = blockDirection == LTR ? endOfEditableContent(visiblePosition) : startOfEditableContent(visiblePosition);
    }
    return rightWordBreak;
}

template <typename Strategy>
static ContainerNode* nonShadowBoundaryParentNode(Node* node)
{
    ContainerNode* parent = Strategy::parent(*node);
    return parent && !parent->isShadowRoot() ? parent : nullptr;
}

template <typename Strategy>
static Node* parentEditingBoundary(const PositionAlgorithm<Strategy>& position)
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

enum BoundarySearchContextAvailability { DontHaveMoreContext, MayHaveMoreContext };

typedef unsigned (*BoundarySearchFunction)(const UChar*, unsigned length, unsigned offset, BoundarySearchContextAvailability, bool& needMoreContext);

static VisiblePosition previousBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction)
{
    Position pos = c.deepEquivalent();
    Node* boundary = parentEditingBoundary(pos);
    if (!boundary)
        return VisiblePosition();

    Document& d = boundary->document();
    Position start = Position::editingPositionOf(boundary, 0).parentAnchoredEquivalent();
    Position end = pos.parentAnchoredEquivalent();

    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;

    TrackExceptionState exceptionState;
    if (requiresContextForWordBoundary(c.characterBefore())) {
        RefPtrWillBeRawPtr<Range> forwardsScanRange(d.createRange());
        forwardsScanRange->setEndAfter(boundary, exceptionState);
        forwardsScanRange->setStart(end.anchorNode(), end.offsetInContainerNode(), exceptionState);
        TextIterator forwardsIterator(forwardsScanRange->startPosition(), forwardsScanRange->endPosition());
        while (!forwardsIterator.atEnd()) {
            Vector<UChar, 1024> characters;
            forwardsIterator.text().appendTextTo(characters);
            int i = endOfFirstWordBoundaryContext(characters.data(), characters.size());
            string.append(characters.data(), i);
            suffixLength += i;
            if (static_cast<unsigned>(i) < characters.size())
                break;
            forwardsIterator.advance();
        }
    }

    ASSERT(!exceptionState.hadException());
    if (exceptionState.hadException())
        return VisiblePosition();

    SimplifiedBackwardsTextIterator it(start, end);
    unsigned next = 0;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        bool inTextSecurityMode = it.node() && it.node()->layoutObject() && it.node()->layoutObject()->style()->textSecurity() != TSNONE;
        // iterate to get chunks until the searchFunction returns a non-zero value.
        if (!inTextSecurityMode) {
            it.prependTextTo(string);
        } else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            Vector<UChar, 1024> iteratorString;
            iteratorString.fill('x', it.length());
            string.prepend(iteratorString.data(), iteratorString.size());
        }
        next = searchFunction(string.data(), string.size(), string.size() - suffixLength, MayHaveMoreContext, needMoreContext);
        if (next)
            break;
        it.advance();
    }
    if (needMoreContext) {
        // The last search returned the beginning of the buffer and asked for more context,
        // but there is no earlier text. Force a search with what's available.
        next = searchFunction(string.data(), string.size(), string.size() - suffixLength, DontHaveMoreContext, needMoreContext);
        ASSERT(!needMoreContext);
    }

    if (!next)
        return VisiblePosition(it.atEnd() ? it.startPosition() : pos);

    Node* node = it.startContainer();
    if (node->isTextNode() && static_cast<int>(next) <= node->maxCharacterOffset()) {
        // The next variable contains a usable index into a text node
        return VisiblePosition(Position(node, next));
    }

    // Use the character iterator to translate the next value into a DOM position.
    BackwardsCharacterIterator charIt(start, end);
    charIt.advance(string.size() - suffixLength - next);
    // FIXME: charIt can get out of shadow host.
    return VisiblePosition(charIt.endPosition());
}

static VisiblePosition nextBoundary(const VisiblePosition& c, BoundarySearchFunction searchFunction)
{
    Position pos = c.deepEquivalent();
    Node* boundary = parentEditingBoundary(pos);
    if (!boundary)
        return VisiblePosition();

    Document& d = boundary->document();
    Position start(pos.parentAnchoredEquivalent());

    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;

    if (requiresContextForWordBoundary(c.characterAfter())) {
        SimplifiedBackwardsTextIterator backwardsIterator(Position::firstPositionInNode(&d), start);
        while (!backwardsIterator.atEnd()) {
            Vector<UChar, 1024> characters;
            backwardsIterator.prependTextTo(characters);
            int length = characters.size();
            int i = startOfLastWordBoundaryContext(characters.data(), length);
            string.prepend(characters.data() + i, length - i);
            prefixLength += length - i;
            if (i > 0)
                break;
            backwardsIterator.advance();
        }
    }

    Position searchStart = Position::editingPositionOf(start.anchorNode(), start.offsetInContainerNode());
    RangeBoundaryPoint searchEndPoint(boundary);
    searchEndPoint.setToEndOfNode(*boundary);
    Position searchEnd = searchEndPoint.toPosition();
    TextIterator it(searchStart, searchEnd, TextIteratorEmitsCharactersBetweenAllVisiblePositions);
    const unsigned invalidOffset = static_cast<unsigned>(-1);
    unsigned next = invalidOffset;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        // Keep asking the iterator for chunks until the search function
        // returns an end value not equal to the length of the string passed to it.
        bool inTextSecurityMode = it.node() && it.node()->layoutObject() && it.node()->layoutObject()->style()->textSecurity() != TSNONE;
        if (!inTextSecurityMode) {
            it.text().appendTextTo(string);
        } else {
            // Treat bullets used in the text security mode as regular characters when looking for boundaries
            Vector<UChar, 1024> iteratorString;
            iteratorString.fill('x', it.length());
            string.append(iteratorString.data(), iteratorString.size());
        }
        next = searchFunction(string.data(), string.size(), prefixLength, MayHaveMoreContext, needMoreContext);
        if (next != string.size())
            break;
        it.advance();
    }
    if (needMoreContext) {
        // The last search returned the end of the buffer and asked for more context,
        // but there is no further text. Force a search with what's available.
        next = searchFunction(string.data(), string.size(), prefixLength, DontHaveMoreContext, needMoreContext);
        ASSERT(!needMoreContext);
    }

    if (it.atEnd() && next == string.size()) {
        pos = it.startPositionInCurrentContainer();
    } else if (next != invalidOffset && next != prefixLength) {
        // Use the character iterator to translate the next value into a DOM position.
        CharacterIterator charIt(searchStart, searchEnd, TextIteratorEmitsCharactersBetweenAllVisiblePositions);
        charIt.advance(next - prefixLength - 1);
        pos = charIt.endPosition();

        if (charIt.characterAt(0) == '\n') {
            // FIXME: workaround for collapsed range (where only start position is correct) emitted for some emitted newlines (see rdar://5192593)
            VisiblePosition visPos = VisiblePosition(pos);
            if (visPos.deepEquivalent() == VisiblePosition(charIt.startPosition()).deepEquivalent()) {
                charIt.advance(1);
                pos = charIt.startPosition();
            }
        }
    }

    // generate VisiblePosition, use TextAffinity::Upstream affinity if possible
    return VisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
}

// ---------

static unsigned startWordBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    ASSERT(offset);
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(characters, offset)) {
        needMoreContext = true;
        return 0;
    }
    needMoreContext = false;
    int start, end;
    U16_BACK_1(characters, 0, offset);
    findWordBoundary(characters, length, offset, &start, &end);
    return start;
}

VisiblePosition startOfWord(const VisiblePosition &c, EWordSide side)
{
    // FIXME: This returns a null VP for c at the start of the document
    // and side == LeftWordIfOnBoundary
    VisiblePosition p = c;
    if (side == RightWordIfOnBoundary) {
        // at paragraph end, the startofWord is the current position
        if (isEndOfParagraph(c))
            return c;

        p = c.next();
        if (p.isNull())
            return c;
    }
    return previousBoundary(p, startWordBoundary);
}

static unsigned endWordBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    ASSERT(offset <= length);
    if (mayHaveMoreContext && endOfFirstWordBoundaryContext(characters + offset, length - offset) == static_cast<int>(length - offset)) {
        needMoreContext = true;
        return length;
    }
    needMoreContext = false;
    return findWordEndBoundary(characters, length, offset);
}

VisiblePosition endOfWord(const VisiblePosition &c, EWordSide side)
{
    VisiblePosition p = c;
    if (side == LeftWordIfOnBoundary) {
        if (isStartOfParagraph(c))
            return c;

        p = c.previous();
        if (p.isNull())
            return c;
    } else if (isEndOfParagraph(c)) {
        return c;
    }

    return nextBoundary(p, endWordBoundary);
}

static unsigned previousWordPositionBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(characters, offset)) {
        needMoreContext = true;
        return 0;
    }
    needMoreContext = false;
    return findNextWordFromIndex(characters, length, offset, false);
}

VisiblePosition previousWordPosition(const VisiblePosition &c)
{
    VisiblePosition prev = previousBoundary(c, previousWordPositionBoundary);
    return c.honorEditingBoundaryAtOrBefore(prev);
}

static unsigned nextWordPositionBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    if (mayHaveMoreContext && endOfFirstWordBoundaryContext(characters + offset, length - offset) == static_cast<int>(length - offset)) {
        needMoreContext = true;
        return length;
    }
    needMoreContext = false;
    return findNextWordFromIndex(characters, length, offset, true);
}

VisiblePosition nextWordPosition(const VisiblePosition &c)
{
    VisiblePosition next = nextBoundary(c, nextWordPositionBoundary);
    return c.honorEditingBoundaryAtOrAfter(next);
}

// ---------

enum LineEndpointComputationMode { UseLogicalOrdering, UseInlineBoxOrdering };
template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> startPositionForLine(const PositionWithAffinityTemplate<Strategy>& c, LineEndpointComputationMode mode)
{
    if (c.isNull())
        return PositionWithAffinityTemplate<Strategy>();

    RootInlineBox* rootBox = RenderedPosition(c.position(), c.affinity()).rootBox();
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        PositionAlgorithm<Strategy> p = c.position();
        if (p.anchorNode()->layoutObject() && p.anchorNode()->layoutObject()->isLayoutBlock() && !p.computeEditingOffset())
            return c;

        return PositionWithAffinityTemplate<Strategy>();
    }

    Node* startNode;
    InlineBox* startBox;
    if (mode == UseLogicalOrdering) {
        startNode = rootBox->getLogicalStartBoxWithNode(startBox);
        if (!startNode)
            return PositionWithAffinityTemplate<Strategy>();
    } else {
        // Generated content (e.g. list markers and CSS :before and :after pseudoelements) have no corresponding DOM element,
        // and so cannot be represented by a VisiblePosition. Use whatever follows instead.
        startBox = rootBox->firstLeafChild();
        while (true) {
            if (!startBox)
                return PositionWithAffinityTemplate<Strategy>();

            startNode = startBox->layoutObject().nonPseudoNode();
            if (startNode)
                break;

            startBox = startBox->nextLeafChild();
        }
    }

    return PositionWithAffinityTemplate<Strategy>(startNode->isTextNode() ? PositionAlgorithm<Strategy>(toText(startNode), toInlineTextBox(startBox)->start()) : PositionAlgorithm<Strategy>::beforeNode(startNode));
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> startOfLine(const PositionWithAffinityTemplate<Strategy>& c, LineEndpointComputationMode mode)
{
    // TODO: this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    PositionWithAffinityTemplate<Strategy> visPos = startPositionForLine(c, mode);

    if (mode == UseLogicalOrdering) {
        if (ContainerNode* editableRoot = highestEditableRoot(c.position())) {
            if (!editableRoot->contains(visPos.position().computeContainerNode()))
                return PositionWithAffinityTemplate<Strategy>(PositionAlgorithm<Strategy>::firstPositionInNode(editableRoot));
        }
    }

    return honorEditingBoundaryAtOrBeforeOf(visPos, c.position());
}

static PositionWithAffinity startOfLine(const PositionWithAffinity& currentPosition)
{
    return startOfLine(currentPosition, UseInlineBoxOrdering);
}

static PositionInComposedTreeWithAffinity startOfLine(const PositionInComposedTreeWithAffinity& currentPosition)
{
    return startOfLine(currentPosition, UseInlineBoxOrdering);
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition startOfLine(const VisiblePosition& currentPosition)
{
    return VisiblePosition(startOfLine(currentPosition.toPositionWithAffinity(), UseInlineBoxOrdering));
}

VisiblePosition logicalStartOfLine(const VisiblePosition& currentPosition)
{
    return VisiblePosition(startOfLine(currentPosition.toPositionWithAffinity(), UseLogicalOrdering));
}

static VisiblePosition endPositionForLine(const VisiblePosition& c, LineEndpointComputationMode mode)
{
    if (c.isNull())
        return VisiblePosition();

    RootInlineBox* rootBox = RenderedPosition(c).rootBox();
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        Position p = c.deepEquivalent();
        if (p.anchorNode()->layoutObject() && p.anchorNode()->layoutObject()->isLayoutBlock() && !p.computeEditingOffset())
            return c;
        return VisiblePosition();
    }

    Node* endNode;
    InlineBox* endBox;
    if (mode == UseLogicalOrdering) {
        endNode = rootBox->getLogicalEndBoxWithNode(endBox);
        if (!endNode)
            return VisiblePosition();
    } else {
        // Generated content (e.g. list markers and CSS :before and :after pseudoelements) have no corresponding DOM element,
        // and so cannot be represented by a VisiblePosition. Use whatever precedes instead.
        endBox = rootBox->lastLeafChild();
        while (true) {
            if (!endBox)
                return VisiblePosition();

            endNode = endBox->layoutObject().nonPseudoNode();
            if (endNode)
                break;

            endBox = endBox->prevLeafChild();
        }
    }

    Position pos;
    if (isHTMLBRElement(*endNode)) {
        pos = positionBeforeNode(endNode);
    } else if (endBox->isInlineTextBox() && endNode->isTextNode()) {
        InlineTextBox* endTextBox = toInlineTextBox(endBox);
        int endOffset = endTextBox->start();
        if (!endTextBox->isLineBreak())
            endOffset += endTextBox->len();
        pos = Position(toText(endNode), endOffset);
    } else {
        pos = positionAfterNode(endNode);
    }

    return VisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
}

static bool inSameLogicalLine(const VisiblePosition& a, const VisiblePosition& b)
{
    return a.isNotNull() && logicalStartOfLine(a).deepEquivalent() == logicalStartOfLine(b).deepEquivalent();
}

static VisiblePosition endOfLine(const VisiblePosition& c, LineEndpointComputationMode mode)
{
    // TODO: this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    VisiblePosition visPos = endPositionForLine(c, mode);

    if (mode == UseLogicalOrdering) {
        // Make sure the end of line is at the same line as the given input position. For a wrapping line, the logical end
        // position for the not-last-2-lines might incorrectly hand back the logical beginning of the next line.
        // For example, <div contenteditable dir="rtl" style="line-break:before-white-space">abcdefg abcdefg abcdefg
        // a abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg abcdefg </div>
        // In this case, use the previous position of the computed logical end position.
        if (!inSameLogicalLine(c, visPos))
            visPos = visPos.previous();

        if (ContainerNode* editableRoot = highestEditableRoot(c.deepEquivalent())) {
            if (!editableRoot->contains(visPos.deepEquivalent().computeContainerNode()))
                return VisiblePosition(lastPositionInNode(editableRoot));
        }

        return c.honorEditingBoundaryAtOrAfter(visPos);
    }

    // Make sure the end of line is at the same line as the given input position. Else use the previous position to
    // obtain end of line. This condition happens when the input position is before the space character at the end
    // of a soft-wrapped non-editable line. In this scenario, endPositionForLine would incorrectly hand back a position
    // in the next line instead. This fix is to account for the discrepancy between lines with webkit-line-break:after-white-space style
    // versus lines without that style, which would break before a space by default.
    if (!inSameLine(c, visPos)) {
        visPos = c.previous();
        if (visPos.isNull())
            return VisiblePosition();
        visPos = endPositionForLine(visPos, UseInlineBoxOrdering);
    }

    return c.honorEditingBoundaryAtOrAfter(visPos);
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition endOfLine(const VisiblePosition& currentPosition)
{
    return endOfLine(currentPosition, UseInlineBoxOrdering);
}

VisiblePosition logicalEndOfLine(const VisiblePosition& currentPosition)
{
    return endOfLine(currentPosition, UseLogicalOrdering);
}

template <typename Strategy>
bool inSameLineAlgorithm(const PositionWithAffinityTemplate<Strategy>& position1, const PositionWithAffinityTemplate<Strategy>& position2)
{
    if (position1.isNull() || position2.isNull())
        return false;
    PositionWithAffinityTemplate<Strategy> startOfLine1 = startOfLine(position1);
    PositionWithAffinityTemplate<Strategy> startOfLine2 = startOfLine(position2);
    if (startOfLine1 == startOfLine2)
        return true;
    PositionAlgorithm<Strategy> canonicalized1 = canonicalPositionOf(startOfLine1.position());
    if (canonicalized1 == startOfLine2.position())
        return true;
    return canonicalized1 == canonicalPositionOf(startOfLine2.position());
}

bool inSameLine(const PositionWithAffinity& a, const PositionWithAffinity& b)
{
    return inSameLineAlgorithm<EditingStrategy>(a, b);
}

bool inSameLine(const PositionInComposedTreeWithAffinity& position1, const PositionInComposedTreeWithAffinity& position2)
{
    return inSameLineAlgorithm<EditingInComposedTreeStrategy>(position1, position2);
}

bool inSameLine(const VisiblePosition& position1, const VisiblePosition& position2)
{
    return inSameLine(position1.toPositionWithAffinity(), position2.toPositionWithAffinity());
}

bool isStartOfLine(const VisiblePosition &p)
{
    return p.isNotNull() && p.deepEquivalent() == startOfLine(p).deepEquivalent();
}

bool isEndOfLine(const VisiblePosition &p)
{
    return p.isNotNull() && p.deepEquivalent() == endOfLine(p).deepEquivalent();
}

bool isLogicalEndOfLine(const VisiblePosition &p)
{
    return p.isNotNull() && p.deepEquivalent() == logicalEndOfLine(p).deepEquivalent();
}

static inline LayoutPoint absoluteLineDirectionPointToLocalPointInBlock(RootInlineBox* root, LayoutUnit lineDirectionPoint)
{
    ASSERT(root);
    LayoutBlockFlow& containingBlock = root->block();
    FloatPoint absoluteBlockPoint = containingBlock.localToAbsolute(FloatPoint());
    if (containingBlock.hasOverflowClip())
        absoluteBlockPoint -= containingBlock.scrolledContentOffset();

    if (root->block().isHorizontalWritingMode())
        return LayoutPoint(lineDirectionPoint - absoluteBlockPoint.x(), root->blockDirectionPointInLine());

    return LayoutPoint(root->blockDirectionPointInLine(), lineDirectionPoint - absoluteBlockPoint.y());
}

VisiblePosition previousLinePosition(const VisiblePosition &visiblePosition, LayoutUnit lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.anchorNode();

    if (!node)
        return VisiblePosition();

    node->document().updateLayoutIgnorePendingStylesheets();

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return VisiblePosition();

    RootInlineBox* root = 0;
    InlineBox* box = computeInlineBoxPosition(visiblePosition).inlineBox;
    if (box) {
        root = box->root().prevRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight() || !root->firstLeafChild())
            root = 0;
    }

    if (!root) {
        Position position = previousRootInlineBoxCandidatePosition(node, visiblePosition, editableType);
        if (position.isNotNull()) {
            RenderedPosition renderedPosition((VisiblePosition(position)));
            root = renderedPosition.rootBox();
            if (!root)
                return VisiblePosition(position);
        }
    }

    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        LayoutPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(root, lineDirectionPoint);
        LayoutObject& layoutObject = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->layoutObject();
        Node* node = layoutObject.node();
        if (node && editingIgnoresContent(node))
            return VisiblePosition(positionInParentBeforeNode(*node));
        return VisiblePosition(layoutObject.positionForPoint(pointInLine));
    }

    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    Element* rootElement = node->hasEditableStyle(editableType) ? node->rootEditableElement(editableType) : node->document().documentElement();
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition(firstPositionInNode(rootElement));
}

VisiblePosition nextLinePosition(const VisiblePosition &visiblePosition, LayoutUnit lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.anchorNode();

    if (!node)
        return VisiblePosition();

    node->document().updateLayoutIgnorePendingStylesheets();

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return VisiblePosition();

    RootInlineBox* root = 0;
    InlineBox* box = computeInlineBoxPosition(visiblePosition).inlineBox;
    if (box) {
        root = box->root().nextRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight() || !root->firstLeafChild())
            root = 0;
    }

    if (!root) {
        // FIXME: We need do the same in previousLinePosition.
        Node* child = NodeTraversal::childAt(*node, p.computeEditingOffset());
        node = child ? child : &NodeTraversal::lastWithinOrSelf(*node);
        Position position = nextRootInlineBoxCandidatePosition(node, visiblePosition, editableType);
        if (position.isNotNull()) {
            RenderedPosition renderedPosition((VisiblePosition(position)));
            root = renderedPosition.rootBox();
            if (!root)
                return VisiblePosition(position);
        }
    }

    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        LayoutPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(root, lineDirectionPoint);
        LayoutObject& layoutObject = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->layoutObject();
        Node* node = layoutObject.node();
        if (node && editingIgnoresContent(node))
            return VisiblePosition(positionInParentBeforeNode(*node));
        return VisiblePosition(layoutObject.positionForPoint(pointInLine));
    }

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    Element* rootElement = node->hasEditableStyle(editableType) ? node->rootEditableElement(editableType) : node->document().documentElement();
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition(lastPositionInNode(rootElement));
}

// ---------

static unsigned startSentenceBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    // FIXME: The following function can return -1; we don't handle that.
    return iterator->preceding(length);
}

VisiblePosition startOfSentence(const VisiblePosition &c)
{
    return previousBoundary(c, startSentenceBoundary);
}

static unsigned endSentenceBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    return iterator->next();
}

// FIXME: This includes the space after the punctuation that marks the end of the sentence.
VisiblePosition endOfSentence(const VisiblePosition &c)
{
    return nextBoundary(c, endSentenceBoundary);
}

static unsigned previousSentencePositionBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    // FIXME: This is identical to startSentenceBoundary. I'm pretty sure that's not right.
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    // FIXME: The following function can return -1; we don't handle that.
    return iterator->preceding(length);
}

VisiblePosition previousSentencePosition(const VisiblePosition &c)
{
    VisiblePosition prev = previousBoundary(c, previousSentencePositionBoundary);
    return c.honorEditingBoundaryAtOrBefore(prev);
}

static unsigned nextSentencePositionBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    // FIXME: This is identical to endSentenceBoundary. This isn't right, it needs to
    // move to the equivlant position in the following sentence.
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    return iterator->following(0);
}

VisiblePosition nextSentencePosition(const VisiblePosition &c)
{
    VisiblePosition next = nextBoundary(c, nextSentencePositionBoundary);
    return c.honorEditingBoundaryAtOrAfter(next);
}

VisiblePosition startOfParagraph(const VisiblePosition& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    Position p = c.deepEquivalent();
    Node* startNode = p.anchorNode();

    if (!startNode)
        return VisiblePosition();

    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return VisiblePosition(positionBeforeNode(startNode));

    Element* startBlock = enclosingBlock(startNode);

    Node* node = startNode;
    ContainerNode* highestRoot = highestEditableRoot(p);
    int offset = p.computeEditingOffset();
    PositionAnchorType type = p.anchorType();

    Node* n = startNode;
    bool startNodeIsEditable = startNode->hasEditableStyle();
    while (n) {
        if (boundaryCrossingRule == CannotCrossEditingBoundary && !nodeIsUserSelectAll(n) && n->hasEditableStyle() != startNodeIsEditable)
            break;
        if (boundaryCrossingRule == CanSkipOverEditingBoundary) {
            while (n && n->hasEditableStyle() != startNodeIsEditable)
                n = NodeTraversal::previousPostOrder(*n, startBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }
        LayoutObject* r = n->layoutObject();
        if (!r) {
            n = NodeTraversal::previousPostOrder(*n, startBlock);
            continue;
        }
        const ComputedStyle& style = r->styleRef();
        if (style.visibility() != VISIBLE) {
            n = NodeTraversal::previousPostOrder(*n, startBlock);
            continue;
        }

        if (r->isBR() || isBlock(n))
            break;

        if (r->isText() && toLayoutText(r)->resolvedTextLength()) {
            ASSERT_WITH_SECURITY_IMPLICATION(n->isTextNode());
            type = PositionAnchorType::OffsetInAnchor;
            if (style.preserveNewline()) {
                LayoutText* text = toLayoutText(r);
                int i = text->textLength();
                int o = offset;
                if (n == startNode && o < i)
                    i = max(0, o);
                while (--i >= 0) {
                    if ((*text)[i] == '\n')
                        return VisiblePosition(Position(toText(n), i + 1));
                }
            }
            node = n;
            offset = 0;
            n = NodeTraversal::previousPostOrder(*n, startBlock);
        } else if (editingIgnoresContent(n) || isRenderedTableElement(n)) {
            node = n;
            type = PositionAnchorType::BeforeAnchor;
            n = n->previousSibling() ? n->previousSibling() : NodeTraversal::previousPostOrder(*n, startBlock);
        } else {
            n = NodeTraversal::previousPostOrder(*n, startBlock);
        }
    }

    if (type == PositionAnchorType::OffsetInAnchor)
        return VisiblePosition(Position(node, offset));

    return VisiblePosition(Position(node, type));
}

VisiblePosition endOfParagraph(const VisiblePosition &c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    if (c.isNull())
        return VisiblePosition();

    Position p = c.deepEquivalent();
    Node* startNode = p.anchorNode();

    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return VisiblePosition(positionAfterNode(startNode));

    Element* startBlock = enclosingBlock(startNode);
    Element* stayInsideBlock = startBlock;

    Node* node = startNode;
    ContainerNode* highestRoot = highestEditableRoot(p);
    int offset = p.computeEditingOffset();
    PositionAnchorType type = p.anchorType();

    Node* n = startNode;
    bool startNodeIsEditable = startNode->hasEditableStyle();
    while (n) {
        if (boundaryCrossingRule == CannotCrossEditingBoundary && !nodeIsUserSelectAll(n) && n->hasEditableStyle() != startNodeIsEditable)
            break;
        if (boundaryCrossingRule == CanSkipOverEditingBoundary) {
            while (n && n->hasEditableStyle() != startNodeIsEditable)
                n = NodeTraversal::next(*n, stayInsideBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }

        LayoutObject* r = n->layoutObject();
        if (!r) {
            n = NodeTraversal::next(*n, stayInsideBlock);
            continue;
        }
        const ComputedStyle& style = r->styleRef();
        if (style.visibility() != VISIBLE) {
            n = NodeTraversal::next(*n, stayInsideBlock);
            continue;
        }

        if (r->isBR() || isBlock(n))
            break;

        // FIXME: We avoid returning a position where the layoutObject can't accept the caret.
        if (r->isText() && toLayoutText(r)->resolvedTextLength()) {
            ASSERT_WITH_SECURITY_IMPLICATION(n->isTextNode());
            int length = toLayoutText(r)->textLength();
            type = PositionAnchorType::OffsetInAnchor;
            if (style.preserveNewline()) {
                LayoutText* text = toLayoutText(r);
                int o = n == startNode ? offset : 0;
                for (int i = o; i < length; ++i) {
                    if ((*text)[i] == '\n')
                        return VisiblePosition(Position(toText(n), i));
                }
            }
            node = n;
            offset = r->caretMaxOffset();
            n = NodeTraversal::next(*n, stayInsideBlock);
        } else if (editingIgnoresContent(n) || isRenderedTableElement(n)) {
            node = n;
            type = PositionAnchorType::AfterAnchor;
            n = NodeTraversal::nextSkippingChildren(*n, stayInsideBlock);
        } else {
            n = NodeTraversal::next(*n, stayInsideBlock);
        }
    }

    if (type == PositionAnchorType::OffsetInAnchor)
        return VisiblePosition(Position(node, offset));

    return VisiblePosition(Position(node, type));
}

// FIXME: isStartOfParagraph(startOfNextParagraph(pos)) is not always true
VisiblePosition startOfNextParagraph(const VisiblePosition& visiblePosition)
{
    VisiblePosition paragraphEnd(endOfParagraph(visiblePosition, CanSkipOverEditingBoundary));
    VisiblePosition afterParagraphEnd(paragraphEnd.next(CannotCrossEditingBoundary));
    // The position after the last position in the last cell of a table
    // is not the start of the next paragraph.
    if (isFirstPositionAfterTable(afterParagraphEnd))
        return afterParagraphEnd.next(CannotCrossEditingBoundary);
    return afterParagraphEnd;
}

bool inSameParagraph(const VisiblePosition &a, const VisiblePosition &b, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return a.isNotNull() && startOfParagraph(a, boundaryCrossingRule).deepEquivalent() == startOfParagraph(b, boundaryCrossingRule).deepEquivalent();
}

bool isStartOfParagraph(const VisiblePosition &pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return pos.isNotNull() && pos.deepEquivalent() == startOfParagraph(pos, boundaryCrossingRule).deepEquivalent();
}

bool isEndOfParagraph(const VisiblePosition &pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return pos.isNotNull() && pos.deepEquivalent() == endOfParagraph(pos, boundaryCrossingRule).deepEquivalent();
}

VisiblePosition previousParagraphPosition(const VisiblePosition& p, LayoutUnit x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = previousLinePosition(pos, x);
        if (n.isNull() || n.deepEquivalent() == pos.deepEquivalent())
            break;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

VisiblePosition nextParagraphPosition(const VisiblePosition& p, LayoutUnit x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = nextLinePosition(pos, x);
        if (n.isNull() || n.deepEquivalent() == pos.deepEquivalent())
            break;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

// ---------

VisiblePosition startOfBlock(const VisiblePosition& visiblePosition, EditingBoundaryCrossingRule rule)
{
    Position position = visiblePosition.deepEquivalent();
    Element* startBlock = position.computeContainerNode() ? enclosingBlock(position.computeContainerNode(), rule) : 0;
    return startBlock ? VisiblePosition(firstPositionInNode(startBlock)) : VisiblePosition();
}

VisiblePosition endOfBlock(const VisiblePosition& visiblePosition, EditingBoundaryCrossingRule rule)
{
    Position position = visiblePosition.deepEquivalent();
    Element* endBlock = position.computeContainerNode() ? enclosingBlock(position.computeContainerNode(), rule) : 0;
    return endBlock ? VisiblePosition(lastPositionInNode(endBlock)) : VisiblePosition();
}

bool inSameBlock(const VisiblePosition &a, const VisiblePosition &b)
{
    return !a.isNull() && enclosingBlock(a.deepEquivalent().computeContainerNode()) == enclosingBlock(b.deepEquivalent().computeContainerNode());
}

bool isStartOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos.deepEquivalent() == startOfBlock(pos, CanCrossEditingBoundary).deepEquivalent();
}

bool isEndOfBlock(const VisiblePosition &pos)
{
    return pos.isNotNull() && pos.deepEquivalent() == endOfBlock(pos, CanCrossEditingBoundary).deepEquivalent();
}

// ---------

VisiblePosition startOfDocument(const Node* node)
{
    if (!node || !node->document().documentElement())
        return VisiblePosition();

    return VisiblePosition(firstPositionInNode(node->document().documentElement()));
}

VisiblePosition startOfDocument(const VisiblePosition &c)
{
    return startOfDocument(c.deepEquivalent().anchorNode());
}

VisiblePosition endOfDocument(const Node* node)
{
    if (!node || !node->document().documentElement())
        return VisiblePosition();

    Element* doc = node->document().documentElement();
    return VisiblePosition(lastPositionInNode(doc));
}

VisiblePosition endOfDocument(const VisiblePosition &c)
{
    return endOfDocument(c.deepEquivalent().anchorNode());
}

bool isStartOfDocument(const VisiblePosition &p)
{
    return p.isNotNull() && p.previous(CanCrossEditingBoundary).isNull();
}

bool isEndOfDocument(const VisiblePosition &p)
{
    return p.isNotNull() && p.next(CanCrossEditingBoundary).isNull();
}

// ---------

VisiblePosition startOfEditableContent(const VisiblePosition& visiblePosition)
{
    ContainerNode* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return VisiblePosition();

    return VisiblePosition(firstPositionInNode(highestRoot));
}

VisiblePosition endOfEditableContent(const VisiblePosition& visiblePosition)
{
    ContainerNode* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return VisiblePosition();

    return VisiblePosition(lastPositionInNode(highestRoot));
}

bool isEndOfEditableOrNonEditableContent(const VisiblePosition &p)
{
    return p.isNotNull() && p.next().isNull();
}

VisiblePosition leftBoundaryOfLine(const VisiblePosition& c, TextDirection direction)
{
    return direction == LTR ? logicalStartOfLine(c) : logicalEndOfLine(c);
}

VisiblePosition rightBoundaryOfLine(const VisiblePosition& c, TextDirection direction)
{
    return direction == LTR ? logicalEndOfLine(c) : logicalStartOfLine(c);
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
static InlineBoxPosition computeInlineBoxPositionAlgorithm(const PositionAlgorithm<Strategy>& position, TextAffinity affinity, TextDirection primaryDirection)
{
    InlineBox* inlineBox = nullptr;
    int caretOffset = position.computeEditingOffset();
    Node* const anchorNode = position.anchorNode();
    LayoutObject* layoutObject = anchorNode->isShadowRoot() ? toShadowRoot(anchorNode)->host()->layoutObject() : anchorNode->layoutObject();

    if (!layoutObject->isText()) {
        inlineBox = 0;
        if (canHaveChildrenForEditing(anchorNode) && layoutObject->isLayoutBlockFlow() && hasRenderedNonAnonymousDescendantsWithHeight(layoutObject)) {
            // Try a visually equivalent position with possibly opposite
            // editability. This helps in case |this| is in an editable block
            // but surrounded by non-editable positions. It acts to negate the
            // logic at the beginning of LayoutObject::createVisiblePosition().
            PositionAlgorithm<Strategy> equivalent = downstreamIgnoringEditingBoundaries(position);
            if (equivalent == position) {
                equivalent = upstreamIgnoringEditingBoundaries(position);
                if (equivalent == position || downstreamIgnoringEditingBoundaries(equivalent) == position)
                    return InlineBoxPosition(inlineBox, caretOffset);
            }

            return computeInlineBoxPosition(equivalent, TextAffinity::Upstream, primaryDirection);
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
static InlineBoxPosition computeInlineBoxPositionAlgorithm(const PositionAlgorithm<Strategy>& position, TextAffinity affinity)
{
    return computeInlineBoxPositionAlgorithm<Strategy>(position, affinity, primaryDirectionOf(*position.anchorNode()));
}

InlineBoxPosition computeInlineBoxPosition(const Position& position, TextAffinity affinity)
{
    return computeInlineBoxPositionAlgorithm<EditingStrategy>(position, affinity);
}

InlineBoxPosition computeInlineBoxPosition(const PositionInComposedTree& position, TextAffinity affinity)
{
    return computeInlineBoxPositionAlgorithm<EditingInComposedTreeStrategy>(position, affinity);
}

InlineBoxPosition computeInlineBoxPosition(const VisiblePosition& position)
{
    return computeInlineBoxPosition(position.deepEquivalent(), position.affinity());
}

InlineBoxPosition computeInlineBoxPosition(const Position& position, TextAffinity affinity, TextDirection primaryDirection)
{
    return computeInlineBoxPositionAlgorithm<EditingStrategy>(position, affinity, primaryDirection);
}

InlineBoxPosition computeInlineBoxPosition(const PositionInComposedTree& position, TextAffinity affinity, TextDirection primaryDirection)
{
    return computeInlineBoxPositionAlgorithm<EditingInComposedTreeStrategy>(position, affinity, primaryDirection);
}

LayoutRect localCaretRectOfPosition(const PositionWithAffinity& position, LayoutObject*& layoutObject)
{
    if (position.position().isNull()) {
        layoutObject = nullptr;
        return LayoutRect();
    }
    Node* node = position.position().anchorNode();

    layoutObject = node->layoutObject();
    if (!layoutObject)
        return LayoutRect();

    InlineBoxPosition boxPosition = computeInlineBoxPosition(position.position(), position.affinity());

    if (boxPosition.inlineBox)
        layoutObject = &boxPosition.inlineBox->layoutObject();

    return layoutObject->localCaretRect(boxPosition.inlineBox, boxPosition.offsetInBox);
}

static int boundingBoxLogicalHeight(LayoutObject *o, const IntRect &rect)
{
    return o->style()->isHorizontalWritingMode() ? rect.height() : rect.width();
}

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

VisiblePosition visiblePositionForContentsPoint(const IntPoint& contentsPoint, LocalFrame* frame)
{
    HitTestRequest request = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping;
    HitTestResult result(request, contentsPoint);
    frame->document()->layoutView()->hitTest(result);

    if (Node* node = result.innerNode())
        return frame->selection().selection().visiblePositionRespectingEditingBoundary(result.localPoint(), node);
    return VisiblePosition();
}

static Node* nextRenderedEditable(Node* node)
{
    for (node = nextAtomicLeafNode(*node); node; node = nextAtomicLeafNode(*node)) {
        LayoutObject* layoutObject = node->layoutObject();
        if (!layoutObject)
            continue;
        if (!node->hasEditableStyle())
            continue;
        // TODO(yosin) We should have a function to check |InlineBox| types
        // in layout rather than using |InlineBox| here. See also
        // |previousRenderedEditable()| which has a same condition.
        if ((layoutObject->isBox() && toLayoutBox(layoutObject)->inlineBoxWrapper()) || (layoutObject->isText() && toLayoutText(layoutObject)->firstTextBox()))
            return node;
    }
    return 0;
}

static Node* previousRenderedEditable(Node* node)
{
    for (node = previousAtomicLeafNode(*node); node; node = previousAtomicLeafNode(*node)) {
        LayoutObject* layoutObject = node->layoutObject();
        if (!layoutObject)
            continue;
        if (!node->hasEditableStyle())
            continue;
        // TODO(yosin) We should have a function to check |InlineBox| types
        // in layout rather than using |InlineBox| here. See also
        // |nextRenderedEditable()| which has a same condition.
        if ((layoutObject->isBox() && toLayoutBox(layoutObject)->inlineBoxWrapper()) || (layoutObject->isText() && toLayoutText(layoutObject)->firstTextBox()))
            return node;
    }
    return 0;
}

static int renderedOffsetOf(const Position& position)
{
    const int offset = position.computeEditingOffset();
    Node* const anchorNode = position.anchorNode();
    if (!anchorNode->isTextNode() || !anchorNode->layoutObject())
        return offset;

    // TODO(yosin) We should have a function to compute offset in |LayoutText|
    // to avoid using |InlineBox| in "editing/".
    int result = 0;
    LayoutText* textLayoutObject = toLayoutText(anchorNode->layoutObject());
    for (InlineTextBox *box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
        int start = box->start();
        int end = box->start() + box->len();
        if (offset < start)
            return result;
        if (offset <= end) {
            result += offset - start;
            return result;
        }
        result += box->len();
    }
    return result;
}

// TODO(yosin) We should move |rendersInDifferentPosition()to "EditingUtilities.cpp"
// with |renderedOffsetOf()|.
bool rendersInDifferentPosition(const Position& position1, const Position& position2)
{
    if (position1.isNull() || position2.isNull())
        return false;

    LayoutObject* layoutObject = position1.anchorNode()->layoutObject();
    if (!layoutObject)
        return false;

    LayoutObject* posLayoutObject = position2.anchorNode()->layoutObject();
    if (!posLayoutObject)
        return false;

    if (layoutObject->style()->visibility() != VISIBLE
        || posLayoutObject->style()->visibility() != VISIBLE)
        return false;

    if (position1.anchorNode() == position2.anchorNode()) {
        if (isHTMLBRElement(*position1.anchorNode()))
            return false;

        if (position1.computeEditingOffset() == position2.computeEditingOffset())
            return false;

        if (!position1.anchorNode()->isTextNode() && !position2.anchorNode()->isTextNode())
            return true;
    }

    if (isHTMLBRElement(*position1.anchorNode()) && isVisuallyEquivalentCandidate(position2))
        return true;

    if (isHTMLBRElement(*position2.anchorNode()) && isVisuallyEquivalentCandidate(position1))
        return true;

    if (!inSameContainingBlockFlowElement(position1.anchorNode(), position2.anchorNode()))
        return true;

    if (position1.anchorNode()->isTextNode() && !inRenderedText(position1))
        return false;

    if (position2.anchorNode()->isTextNode() && !inRenderedText(position2))
        return false;

    const int renderedOffset1 = renderedOffsetOf(position1);
    const int renderedOffset2 = renderedOffsetOf(position2);

    if (layoutObject == posLayoutObject && renderedOffset1 == renderedOffset2)
        return false;

    InlineBoxPosition boxPosition1 = computeInlineBoxPosition(position1, TextAffinity::Downstream);
    InlineBoxPosition boxPosition2 = computeInlineBoxPosition(position2, TextAffinity::Downstream);

    WTF_LOG(Editing, "layoutObject1:   %p [%p]\n", layoutObject, boxPosition1.inlineBox);
    WTF_LOG(Editing, "renderedOffset1: %d\n", renderedOffset1);
    WTF_LOG(Editing, "layoutObject2:   %p [%p]\n", posLayoutObject, boxPosition2.inlineBox);
    WTF_LOG(Editing, "renderedOffset2: %d\n", renderedOffset2);
    WTF_LOG(Editing, "node1 min/max:   %d:%d\n", caretMinOffset(position1.anchorNode()), caretMaxOffset(position1.anchorNode()));
    WTF_LOG(Editing, "node2 min/max:   %d:%d\n", caretMinOffset(position2.anchorNode()), caretMaxOffset(position2.anchorNode()));
    WTF_LOG(Editing, "----------------------------------------------------------------------\n");

    if (!boxPosition1.inlineBox || !boxPosition2.inlineBox) {
        return false;
    }

    if (boxPosition1.inlineBox->root() != boxPosition2.inlineBox->root()) {
        return true;
    }

    if (nextRenderedEditable(position1.anchorNode()) == position2.anchorNode()
        && renderedOffset1 == caretMaxOffset(position1.anchorNode()) && !renderedOffset2) {
        return false;
    }

    if (previousRenderedEditable(position1.anchorNode()) == position2.anchorNode()
        && !renderedOffset1 && renderedOffset2 == caretMaxOffset(position2.anchorNode())) {
        return false;
    }

    return true;
}

// Whether or not [node, 0] and [node, lastOffsetForEditing(node)] are their own VisiblePositions.
// If true, adjacent candidates are visually distinct.
// FIXME: Disregard nodes with layoutObjects that have no height, as we do in isCandidate.
// FIXME: Share code with isCandidate, if possible.
static bool endsOfNodeAreVisuallyDistinctPositions(Node* node)
{
    if (!node || !node->layoutObject())
        return false;

    if (!node->layoutObject()->isInline())
        return true;

    // Don't include inline tables.
    if (isHTMLTableElement(*node))
        return false;

    // A Marquee elements are moving so we should assume their ends are always
    // visibily distinct.
    if (isHTMLMarqueeElement(*node))
        return true;

    // There is a VisiblePosition inside an empty inline-block container.
    return node->layoutObject()->isReplaced() && canHaveChildrenForEditing(node) && toLayoutBox(node->layoutObject())->size().height() != 0 && !node->hasChildren();
}

template <typename Strategy>
static Node* enclosingVisualBoundary(Node* node)
{
    while (node && !endsOfNodeAreVisuallyDistinctPositions(node))
        node = Strategy::parent(*node);

    return node;
}

// upstream() and downstream() want to return positions that are either in a
// text node or at just before a non-text node.  This method checks for that.
template <typename Strategy>
static bool isStreamer(const PositionIteratorAlgorithm<Strategy>& pos)
{
    if (!pos.node())
        return true;

    if (isAtomicNode(pos.node()))
        return true;

    return pos.atStartOfNode();
}

template <typename Strategy>
static PositionAlgorithm<Strategy> mostForwardCaretPosition(const PositionAlgorithm<Strategy>& position, EditingBoundaryCrossingRule rule)
{
    TRACE_EVENT0("blink", "Position::upstream");

    Node* startNode = position.anchorNode();
    if (!startNode)
        return PositionAlgorithm<Strategy>();

    // iterate backward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary<Strategy>(startNode);
    // FIXME: PositionIterator should respect Before and After positions.
    PositionIteratorAlgorithm<Strategy> lastVisible(position.isAfterAnchor() ? PositionAlgorithm<Strategy>::editingPositionOf(position.anchorNode(), Strategy::caretMaxOffset(*position.anchorNode())) : position);
    PositionIteratorAlgorithm<Strategy> currentPos = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    Node* lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPos.atStart(); currentPos.decrement()) {
        Node* currentNode = currentPos.node();
        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }
            lastNode = currentNode;
        }

        // If we've moved to a position that is visually distinct, return the last saved position. There
        // is code below that terminates early if we're *about* to move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentNode != boundary)
            return lastVisible.deprecatedComputePosition();

        // skip position in non-laid out or invisible node
        LayoutObject* layoutObject = currentNode->layoutObject();
        if (!layoutObject || layoutObject->style()->visibility() != VISIBLE)
            continue;

        if (rule == CanCrossEditingBoundary && boundaryCrossed) {
            lastVisible = currentPos;
            break;
        }

        // track last visible streamer position
        if (isStreamer<Strategy>(currentPos))
            lastVisible = currentPos;

        // Don't move past a position that is visually distinct.  We could rely on code above to terminate and
        // return lastVisible on the next iteration, but we terminate early to avoid doing a nodeIndex() call.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentPos.atStartOfNode())
            return lastVisible.deprecatedComputePosition();

        // Return position after tables and nodes which have content that can be ignored.
        if (Strategy::editingIgnoresContent(currentNode) || isRenderedHTMLTableElement(currentNode)) {
            if (currentPos.atEndOfNode())
                return PositionAlgorithm<Strategy>::afterNode(currentNode);
            continue;
        }

        // return current position if it is in laid out text
        if (layoutObject->isText() && toLayoutText(layoutObject)->firstTextBox()) {
            if (currentNode != startNode) {
                // This assertion fires in layout tests in the case-transform.html test because
                // of a mix-up between offsets in the text in the DOM tree with text in the
                // layout tree which can have a different length due to case transformation.
                // Until we resolve that, disable this so we can run the layout tests!
                // ASSERT(currentOffset >= layoutObject->caretMaxOffset());
                return PositionAlgorithm<Strategy>(currentNode, layoutObject->caretMaxOffset());
            }

            unsigned textOffset = currentPos.offsetInLeafNode();
            LayoutText* textLayoutObject = toLayoutText(layoutObject);
            InlineTextBox* lastTextBox = textLayoutObject->lastTextBox();
            for (InlineTextBox* box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset <= box->start() + box->len()) {
                    if (textOffset > box->start())
                        return currentPos.computePosition();
                    continue;
                }

                if (box == lastTextBox || textOffset != box->start() + box->len() + 1)
                    continue;

                // The text continues on the next line only if the last text box is not on this line and
                // none of the boxes on this line have a larger start offset.

                bool continuesOnNextLine = true;
                InlineBox* otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->nextLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (otherBox->layoutObject() == textLayoutObject && toInlineTextBox(otherBox)->start() > textOffset))
                        continuesOnNextLine = false;
                }

                otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->prevLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (otherBox->layoutObject() == textLayoutObject && toInlineTextBox(otherBox)->start() > textOffset))
                        continuesOnNextLine = false;
                }

                if (continuesOnNextLine)
                    return currentPos.computePosition();
            }
        }
    }
    return lastVisible.deprecatedComputePosition();
}

Position mostForwardCaretPosition(const Position& position, EditingBoundaryCrossingRule rule)
{
    return mostForwardCaretPosition<EditingStrategy>(position, rule);
}

PositionInComposedTree mostForwardCaretPosition(const PositionInComposedTree& position, EditingBoundaryCrossingRule rule)
{
    return mostForwardCaretPosition<EditingInComposedTreeStrategy>(position, rule);
}

template <typename Strategy>
PositionAlgorithm<Strategy> mostBackwardCaretPosition(const PositionAlgorithm<Strategy>& position, EditingBoundaryCrossingRule rule)
{
    TRACE_EVENT0("blink", "Position::downstream");

    Node* startNode = position.anchorNode();
    if (!startNode)
        return PositionAlgorithm<Strategy>();

    // iterate forward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary<Strategy>(startNode);
    // FIXME: PositionIterator should respect Before and After positions.
    PositionIteratorAlgorithm<Strategy> lastVisible(position.isAfterAnchor() ? PositionAlgorithm<Strategy>::editingPositionOf(position.anchorNode(), Strategy::caretMaxOffset(*position.anchorNode())) : position);
    PositionIteratorAlgorithm<Strategy> currentPos = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    Node* lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPos.atEnd(); currentPos.increment()) {
        Node* currentNode = currentPos.node();
        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }

            lastNode = currentNode;
        }

        // stop before going above the body, up into the head
        // return the last visible streamer position
        if (isHTMLBodyElement(*currentNode) && currentPos.atEndOfNode())
            break;

        // Do not move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentNode != boundary)
            return lastVisible.deprecatedComputePosition();
        // Do not move past a visually disinct position.
        // Note: The first position after the last in a node whose ends are visually distinct
        // positions will be [boundary->parentNode(), originalBlock->nodeIndex() + 1].
        if (boundary && Strategy::parent(*boundary) == currentNode)
            return lastVisible.deprecatedComputePosition();

        // skip position in non-laid out or invisible node
        LayoutObject* layoutObject = currentNode->layoutObject();
        if (!layoutObject || layoutObject->style()->visibility() != VISIBLE)
            continue;

        if (rule == CanCrossEditingBoundary && boundaryCrossed) {
            lastVisible = currentPos;
            break;
        }

        // track last visible streamer position
        if (isStreamer<Strategy>(currentPos))
            lastVisible = currentPos;

        // Return position before tables and nodes which have content that can be ignored.
        if (Strategy::editingIgnoresContent(currentNode) || isRenderedHTMLTableElement(currentNode)) {
            if (currentPos.offsetInLeafNode() <= layoutObject->caretMinOffset())
                return PositionAlgorithm<Strategy>::editingPositionOf(currentNode, layoutObject->caretMinOffset());
            continue;
        }

        // return current position if it is in laid out text
        if (layoutObject->isText() && toLayoutText(layoutObject)->firstTextBox()) {
            if (currentNode != startNode) {
                ASSERT(currentPos.atStartOfNode());
                return PositionAlgorithm<Strategy>(currentNode, layoutObject->caretMinOffset());
            }

            unsigned textOffset = currentPos.offsetInLeafNode();
            LayoutText* textLayoutObject = toLayoutText(layoutObject);
            InlineTextBox* lastTextBox = textLayoutObject->lastTextBox();
            for (InlineTextBox* box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset <= box->end()) {
                    if (textOffset >= box->start())
                        return currentPos.computePosition();
                    continue;
                }

                if (box == lastTextBox || textOffset != box->start() + box->len())
                    continue;

                // The text continues on the next line only if the last text box is not on this line and
                // none of the boxes on this line have a larger start offset.

                bool continuesOnNextLine = true;
                InlineBox* otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->nextLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (otherBox->layoutObject() == textLayoutObject && toInlineTextBox(otherBox)->start() >= textOffset))
                        continuesOnNextLine = false;
                }

                otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->prevLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (otherBox->layoutObject() == textLayoutObject && toInlineTextBox(otherBox)->start() >= textOffset))
                        continuesOnNextLine = false;
                }

                if (continuesOnNextLine)
                    return currentPos.computePosition();
            }
        }
    }

    return lastVisible.deprecatedComputePosition();
}

Position mostBackwardCaretPosition(const Position& position, EditingBoundaryCrossingRule rule)
{
    return mostBackwardCaretPosition<EditingStrategy>(position, rule);
}

PositionInComposedTree mostBackwardCaretPosition(const PositionInComposedTree& position, EditingBoundaryCrossingRule rule)
{
    return mostBackwardCaretPosition<EditingInComposedTreeStrategy>(position, rule);
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
static bool isVisuallyEquivalentCandidateAlgorithm(const PositionAlgorithm<Strategy>& position)
{
    Node* const anchorNode = position.anchorNode();
    if (!anchorNode)
        return false;

    LayoutObject* layoutObject = anchorNode->layoutObject();
    if (!layoutObject)
        return false;

    if (layoutObject->style()->visibility() != VISIBLE)
        return false;

    if (layoutObject->isBR()) {
        // TODO(leviw) The condition should be
        // m_anchorType == PositionAnchorType::BeforeAnchor, but for now we
        // still need to support legacy positions.
        if (position.isAfterAnchor())
            return false;
        return !position.computeEditingOffset() && !nodeIsUserSelectNone(Strategy::parent(*anchorNode));
    }

    if (layoutObject->isText())
        return !nodeIsUserSelectNone(anchorNode) && inRenderedText(position);

    if (layoutObject->isSVG()) {
        // We don't consider SVG elements are contenteditable except for
        // associated |layoutObject| returns |isText()| true,
        // e.g. |LayoutSVGInlineText|.
        return false;
    }

    if (isRenderedHTMLTableElement(anchorNode) || Strategy::editingIgnoresContent(anchorNode))
        return (position.atFirstEditingPositionForNode() || position.atLastEditingPositionForNode()) && !nodeIsUserSelectNone(Strategy::parent(*anchorNode));

    if (isHTMLHtmlElement(*anchorNode))
        return false;

    if (layoutObject->isLayoutBlockFlow() || layoutObject->isFlexibleBox() || layoutObject->isLayoutGrid()) {
        if (toLayoutBlock(layoutObject)->logicalHeight() || isHTMLBodyElement(*anchorNode)) {
            if (!hasRenderedNonAnonymousDescendantsWithHeight(layoutObject))
                return position.atFirstEditingPositionForNode() && !nodeIsUserSelectNone(anchorNode);
            return anchorNode->hasEditableStyle() && !nodeIsUserSelectNone(anchorNode) && atEditingBoundary(position);
        }
    } else {
        LocalFrame* frame = anchorNode->document().frame();
        bool caretBrowsing = frame->settings() && frame->settings()->caretBrowsingEnabled();
        return (caretBrowsing || anchorNode->hasEditableStyle()) && !nodeIsUserSelectNone(anchorNode) && atEditingBoundary(position);
    }

    return false;
}

bool isVisuallyEquivalentCandidate(const Position& position)
{
    return isVisuallyEquivalentCandidateAlgorithm<EditingStrategy>(position);
}

bool isVisuallyEquivalentCandidate(const PositionInComposedTree& position)
{
    return isVisuallyEquivalentCandidateAlgorithm<EditingInComposedTreeStrategy>(position);
}

} // namespace blink
