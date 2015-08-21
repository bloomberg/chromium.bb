/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef VisibleUnits_h
#define VisibleUnits_h

#include "core/CoreExport.h"
#include "core/editing/EditingBoundary.h"
#include "core/editing/PositionWithAffinity.h"
#include "platform/text/TextDirection.h"

namespace blink {

class LayoutRect;
class LayoutUnit;
class LayoutObject;
class Node;
class VisiblePosition;
class IntPoint;
class InlineBox;
class LocalFrame;

enum EWordSide { RightWordIfOnBoundary = false, LeftWordIfOnBoundary = true };

struct InlineBoxPosition {
    InlineBox* inlineBox;
    int offsetInBox;

    InlineBoxPosition()
        : inlineBox(nullptr), offsetInBox(0)
    {
    }

    InlineBoxPosition(InlineBox* inlineBox, int offsetInBox)
        : inlineBox(inlineBox), offsetInBox(offsetInBox)
    {
    }
};

// Position
// mostForward/BackwardCaretPosition are used for moving back and forth between
// visually equivalent candidates.
// For example, for the text node "foo     bar" where whitespace is
// collapsible, there are two candidates that map to the VisiblePosition
// between 'b' and the space, after first space and before last space.
//
// mostBackwardCaretPosition returns the left candidate and also returs
// [boundary, 0] for any of the positions from [boundary, 0] to the first
// candidate in boundary, where
// endsOfNodeAreVisuallyDistinctPositions(boundary) is true.
//
// mostForwardCaretPosition() returns the right one and also returns the
// last position in the last atomic node in boundary for all of the positions
// in boundary after the last candidate, where
// endsOfNodeAreVisuallyDistinctPositions(boundary).
// FIXME: This function should never be called when the line box tree is dirty.
// See https://bugs.webkit.org/show_bug.cgi?id=97264
CORE_EXPORT Position mostBackwardCaretPosition(const Position &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT PositionInComposedTree mostBackwardCaretPosition(const PositionInComposedTree &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT Position mostForwardCaretPosition(const Position &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT PositionInComposedTree mostForwardCaretPosition(const PositionInComposedTree &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);

CORE_EXPORT bool isVisuallyEquivalentCandidate(const Position&);
CORE_EXPORT bool isVisuallyEquivalentCandidate(const PositionInComposedTree&);

// words
CORE_EXPORT VisiblePosition startOfWord(const VisiblePosition &, EWordSide = RightWordIfOnBoundary);
CORE_EXPORT VisiblePosition endOfWord(const VisiblePosition &, EWordSide = RightWordIfOnBoundary);
VisiblePosition previousWordPosition(const VisiblePosition &);
VisiblePosition nextWordPosition(const VisiblePosition &);
VisiblePosition rightWordPosition(const VisiblePosition&, bool skipsSpaceWhenMovingRight);
VisiblePosition leftWordPosition(const VisiblePosition&, bool skipsSpaceWhenMovingRight);

// sentences
CORE_EXPORT VisiblePosition startOfSentence(const VisiblePosition &);
CORE_EXPORT VisiblePosition endOfSentence(const VisiblePosition &);
VisiblePosition previousSentencePosition(const VisiblePosition &);
VisiblePosition nextSentencePosition(const VisiblePosition &);

// lines
VisiblePosition startOfLine(const VisiblePosition &);
VisiblePosition endOfLine(const VisiblePosition &);
CORE_EXPORT VisiblePosition previousLinePosition(const VisiblePosition&, LayoutUnit lineDirectionPoint, EditableType = ContentIsEditable);
CORE_EXPORT VisiblePosition nextLinePosition(const VisiblePosition&, LayoutUnit lineDirectionPoint, EditableType = ContentIsEditable);
CORE_EXPORT bool inSameLine(const VisiblePosition &, const VisiblePosition &);
CORE_EXPORT bool inSameLine(const PositionWithAffinity&, const PositionWithAffinity &);
CORE_EXPORT bool inSameLine(const PositionInComposedTreeWithAffinity&, const PositionInComposedTreeWithAffinity&);
bool isStartOfLine(const VisiblePosition &);
bool isEndOfLine(const VisiblePosition &);
VisiblePosition logicalStartOfLine(const VisiblePosition &);
VisiblePosition logicalEndOfLine(const VisiblePosition &);
bool isLogicalEndOfLine(const VisiblePosition &);
VisiblePosition leftBoundaryOfLine(const VisiblePosition&, TextDirection);
VisiblePosition rightBoundaryOfLine(const VisiblePosition&, TextDirection);

// paragraphs (perhaps a misnomer, can be divided by line break elements)
VisiblePosition startOfParagraph(const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
VisiblePosition endOfParagraph(const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
VisiblePosition startOfNextParagraph(const VisiblePosition&);
VisiblePosition previousParagraphPosition(const VisiblePosition &, LayoutUnit x);
VisiblePosition nextParagraphPosition(const VisiblePosition &, LayoutUnit x);
bool isStartOfParagraph(const VisiblePosition &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
bool isEndOfParagraph(const VisiblePosition &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
bool inSameParagraph(const VisiblePosition &, const VisiblePosition &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);

// blocks (true paragraphs; line break elements don't break blocks)
VisiblePosition startOfBlock(const VisiblePosition &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
VisiblePosition endOfBlock(const VisiblePosition &, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
bool inSameBlock(const VisiblePosition &, const VisiblePosition &);
bool isStartOfBlock(const VisiblePosition &);
bool isEndOfBlock(const VisiblePosition &);

// document
VisiblePosition startOfDocument(const Node*);
VisiblePosition endOfDocument(const Node*);
VisiblePosition startOfDocument(const VisiblePosition &);
VisiblePosition endOfDocument(const VisiblePosition &);
bool isStartOfDocument(const VisiblePosition &);
bool isEndOfDocument(const VisiblePosition &);

// editable content
VisiblePosition startOfEditableContent(const VisiblePosition&);
VisiblePosition endOfEditableContent(const VisiblePosition&);
bool isEndOfEditableOrNonEditableContent(const VisiblePosition&);

CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const Position&, TextAffinity);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const Position&, TextAffinity, TextDirection primaryDirection);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const PositionInComposedTree&, TextAffinity);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const PositionInComposedTree&, TextAffinity, TextDirection primaryDirection);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const VisiblePosition&);

// Rect is local to the returned layoutObject
LayoutRect localCaretRectOfPosition(const PositionWithAffinity&, LayoutObject*&);
bool hasRenderedNonAnonymousDescendantsWithHeight(LayoutObject*);

// Returns a hit-tested VisiblePosition for the given point in contents-space
// coordinates.
CORE_EXPORT VisiblePosition visiblePositionForContentsPoint(const IntPoint&, LocalFrame*);

CORE_EXPORT bool rendersInDifferentPosition(const Position&, const Position&);

} // namespace blink

#endif // VisibleUnits_h
