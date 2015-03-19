/*
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef TextIterator_h
#define TextIterator_h

#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/editing/FindOptions.h"
#include "core/editing/iterators/FullyClippedStateStack.h"
#include "core/editing/iterators/TextIteratorFlags.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class InlineTextBox;
class LayoutText;
class LayoutTextFragment;

String plainText(const Range*, TextIteratorBehaviorFlags = TextIteratorDefaultBehavior);
String plainText(const Position& start, const Position& end, TextIteratorBehaviorFlags = TextIteratorDefaultBehavior);
PassRefPtrWillBeRawPtr<Range> findPlainText(const Range*, const String&, FindOptions);
void findPlainText(const Position& inputStart, const Position& inputEnd, const String&, FindOptions, Position& resultStart, Position& resultEnd);

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.  The text comes back in
// chunks so as to optimize for performance of the iteration.

class CORE_EXPORT TextIterator {
    STACK_ALLOCATED();
public:
    explicit TextIterator(const Range*, TextIteratorBehaviorFlags = TextIteratorDefaultBehavior);
    // [start, end] indicates the document range that the iteration should take place within (both ends inclusive).
    TextIterator(const Position& start, const Position& end, TextIteratorBehaviorFlags = TextIteratorDefaultBehavior);
    ~TextIterator();

    bool atEnd() const { return !m_positionNode || m_shouldStop; }
    void advance();
    bool isInsideReplacedElement() const;

    int length() const { return m_textLength; }
    UChar characterAt(unsigned index) const;
    String substring(unsigned position, unsigned length) const;
    void appendTextToStringBuilder(StringBuilder&, unsigned position = 0, unsigned maxLength = UINT_MAX) const;

    template<typename BufferType>
    void appendTextTo(BufferType& output, unsigned position = 0)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(position <= static_cast<unsigned>(length()));
        unsigned lengthToAppend = length() - position;
        if (!lengthToAppend)
            return;
        if (m_singleCharacterBuffer) {
            ASSERT(!position);
            ASSERT(length() == 1);
            output.append(&m_singleCharacterBuffer, 1);
        } else {
            string().appendTo(output, startOffset() + position, lengthToAppend);
        }
    }

    PassRefPtrWillBeRawPtr<Range> createRange() const;
    Node* node() const;

    Document* ownerDocument() const;
    Node* startContainer() const;
    Node* endContainer() const;
    int startOffset() const;
    int endOffset() const;
    Position startPosition() const;
    Position endPosition() const;

    bool breaksAtReplacedElement() { return m_breaksAtReplacedElement; }

    // Computes the length of the given range using a text iterator. The default
    // iteration behavior is to always emit object replacement characters for
    // replaced elements. When |forSelectionPreservation| is set to true, it
    // also emits spaces for other non-text nodes using the
    // |TextIteratorEmitsCharactersBetweenAllVisiblePosition| mode.
    static int rangeLength(const Range*, bool forSelectionPreservation = false);
    static int rangeLength(const Position& start, const Position& end, bool forSelectionPreservation = false);
    static PassRefPtrWillBeRawPtr<Range> subrange(Range* entireRange, int characterOffset, int characterCount);
    static void subrange(Position& start, Position& end, int characterOffset, int characterCount);

    static bool shouldEmitTabBeforeNode(Node*);
    static bool shouldEmitNewlineBeforeNode(Node&);
    static bool shouldEmitNewlineAfterNode(Node&);
    static bool shouldEmitNewlineForNode(Node*, bool emitsOriginalText);

    static bool supportsAltText(Node*);

private:
    enum IterationProgress {
        HandledNone,
        HandledOpenShadowRoots,
        HandledClosedShadowRoot,
        HandledNode,
        HandledChildren
    };

    void initialize(const Position& start, const Position& end);

    void flushPositionOffsets() const;
    int positionStartOffset() const { return m_positionStartOffset; }
    const String& string() const { return m_text; }
    void exitNode();
    bool shouldRepresentNodeOffsetZero();
    bool shouldEmitSpaceBeforeAndAfterNode(Node*);
    void representNodeOffsetZero();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void handleTextBox();
    void handleTextNodeFirstLetter(LayoutTextFragment*);
    bool hasVisibleTextNode(LayoutText*);
    void emitCharacter(UChar, Node* textNode, Node* offsetBaseNode, int textStartOffset, int textEndOffset);
    void emitText(Node* textNode, LayoutText* renderer, int textStartOffset, int textEndOffset);

    // Current position, not necessarily of the text being returned, but position
    // as we walk through the DOM tree.
    RawPtrWillBeMember<Node> m_node;
    int m_offset;
    IterationProgress m_iterationProgress;
    FullyClippedStateStack m_fullyClippedStack;
    int m_shadowDepth;

    // The range.
    RawPtrWillBeMember<Node> m_startContainer;
    int m_startOffset;
    RawPtrWillBeMember<Node> m_endContainer;
    int m_endOffset;
    RawPtrWillBeMember<Node> m_pastEndNode;

    // The current text and its position, in the form to be returned from the iterator.
    RawPtrWillBeMember<Node> m_positionNode;
    mutable RawPtrWillBeMember<Node> m_positionOffsetBaseNode;
    mutable int m_positionStartOffset;
    mutable int m_positionEndOffset;
    int m_textLength;
    String m_text;

    // Used when there is still some pending text from the current node; when these
    // are false and 0, we go back to normal iterating.
    bool m_needsAnotherNewline;
    InlineTextBox* m_textBox;
    // Used when iteration over :first-letter text to save pointer to
    // remaining text box.
    InlineTextBox* m_remainingTextBox;
    // Used to point to LayoutText object for :first-letter.
    RawPtrWillBeMember<LayoutText> m_firstLetterText;

    // Used to do the whitespace collapsing logic.
    RawPtrWillBeMember<Text> m_lastTextNode;
    bool m_lastTextNodeEndedWithCollapsedSpace;
    UChar m_lastCharacter;

    // Used for whitespace characters that aren't in the DOM, so we can point at them.
    // If non-zero, overrides m_text.
    UChar m_singleCharacterBuffer;

    // Used when text boxes are out of order (Hebrew/Arabic w/ embeded LTR text)
    Vector<InlineTextBox*> m_sortedTextBoxes;
    size_t m_sortedTextBoxesPosition;

    // Used when deciding whether to emit a "positioning" (e.g. newline) before any other content
    bool m_hasEmitted;

    // Used by selection preservation code.  There should be one character emitted between every VisiblePosition
    // in the Range used to create the TextIterator.
    // FIXME <rdar://problem/6028818>: This functionality should eventually be phased out when we rewrite
    // moveParagraphs to not clone/destroy moved content.
    bool m_emitsCharactersBetweenAllVisiblePositions;
    bool m_entersTextControls;

    // Used in pasting inside password field.
    bool m_emitsOriginalText;
    // Used when deciding text fragment created by :first-letter should be looked into.
    bool m_handledFirstLetter;
    // Used when the visibility of the style should not affect text gathering.
    bool m_ignoresStyleVisibility;
    // Used when the iteration should stop if form controls are reached.
    bool m_stopsOnFormControls;
    // Used when m_stopsOnFormControls is set to determine if the iterator should keep advancing.
    bool m_shouldStop;

    bool m_emitsImageAltText;

    bool m_entersOpenShadowRoots;

    bool m_emitsObjectReplacementCharacter;

    bool m_breaksAtReplacedElement;
};

} // namespace blink

#endif // TextIterator_h
