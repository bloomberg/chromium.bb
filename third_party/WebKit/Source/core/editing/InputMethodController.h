/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef InputMethodController_h
#define InputMethodController_h

#include "core/editing/CompositionUnderline.h"
#include "wtf/Vector.h"

namespace WebCore {

class Editor;
class EditorClient;
class Frame;
class Range;
class Text;

// FIXME: We should move PlainTextOffsets to own file for using InputMethodController
// and TextIterator and unify PlainTextRange defined in AccessibilityObject.h.
class PlainTextOffsets {
public:
    PlainTextOffsets();
    PlainTextOffsets(int start, int length);
    size_t end() const { return m_end; }
    size_t start() const { return m_start; }
    bool isNull() const { return m_start == kNotFound; }
private:
    size_t m_start;
    size_t m_end;
};

class InputMethodController {
public:
    static PassOwnPtr<InputMethodController> create(Frame*);
    ~InputMethodController();

    // international text input composition
    bool hasComposition() const { return m_compositionNode; }
    void setComposition(const String&, const Vector<CompositionUnderline>&, unsigned selectionStart, unsigned selectionEnd);
    void setCompositionFromExistingText(const Vector<CompositionUnderline>&, unsigned compositionStart, unsigned compositionEnd);
    // Inserts the text that is being composed as a regular text.
    // This method does nothing if composition node is not present.
    void confirmComposition();
    // Inserts the given text string in the place of the existing composition, or replaces the selection if composition is not present.
    void confirmComposition(const String& text);
    void confirmCompositionAndResetState();
    // Deletes the existing composition text.
    void cancelComposition();
    void cancelCompositionIfSelectionIsInvalid();
    PassRefPtr<Range> compositionRange() const;

    // getting international text input composition state (for use by InlineTextBox)
    Text* compositionNode() const { return m_compositionNode.get(); }
    unsigned compositionStart() const { return m_compositionStart; }
    unsigned compositionEnd() const { return m_compositionEnd; }
    bool compositionUsesCustomUnderlines() const { return !m_customCompositionUnderlines.isEmpty(); }
    const Vector<CompositionUnderline>& customCompositionUnderlines() const { return m_customCompositionUnderlines; }

    void clear();

private:
    class SelectionOffsetsScope {
        WTF_MAKE_NONCOPYABLE(SelectionOffsetsScope);
    public:
        SelectionOffsetsScope(InputMethodController*);
        ~SelectionOffsetsScope();
    private:
        InputMethodController* m_inputMethodController;
        PlainTextOffsets m_offsets;
    };
    friend class SelectionOffsetsScope;

    Frame* m_frame;
    RefPtr<Text> m_compositionNode;
    // FIXME: We should use PlainTextOffsets m_compositionRange instead of
    // m_compositionStart/m_compositionEnd.
    unsigned m_compositionStart;
    unsigned m_compositionEnd;
    // startOffset and endOffset of CompositionUnderline are based on
    // m_compositionNode.
    Vector<CompositionUnderline> m_customCompositionUnderlines;

    explicit InputMethodController(Frame*);
    Editor& editor() const;
    EditorClient& editorClient() const;
    bool insertTextForConfirmedComposition(const String& text);
    void selectComposition() const;
    enum FinishCompositionMode { ConfirmComposition, CancelComposition };
    void finishComposition(const String&, FinishCompositionMode);
    PlainTextOffsets getSelectionOffsets() const;
    bool setSelectionOffsets(const PlainTextOffsets&);
};

} // namespace WebCore

#endif // InputMethodController_h
