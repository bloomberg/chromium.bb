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

#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/dom/SynchronousMutationObserver.h"
#include "core/editing/CompositionUnderline.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/PlainTextRange.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTextInputInfo.h"
#include "public/platform/WebTextInputType.h"
#include "wtf/Vector.h"

namespace blink {

class Editor;
class LocalFrame;
class Range;

class CORE_EXPORT InputMethodController final
    : public GarbageCollectedFinalized<InputMethodController>,
      public SynchronousMutationObserver {
  WTF_MAKE_NONCOPYABLE(InputMethodController);
  USING_GARBAGE_COLLECTED_MIXIN(InputMethodController);

 public:
  enum ConfirmCompositionBehavior {
    DoNotKeepSelection,
    KeepSelection,
  };

  static InputMethodController* create(LocalFrame&);
  virtual ~InputMethodController();
  DECLARE_TRACE();

  // international text input composition
  bool hasComposition() const;
  void setComposition(const String& text,
                      const Vector<CompositionUnderline>& underlines,
                      int selectionStart,
                      int selectionEnd);
  void setCompositionFromExistingText(const Vector<CompositionUnderline>& text,
                                      unsigned compositionStart,
                                      unsigned compositionEnd);

  // Deletes ongoing composing text if any, inserts specified text, and
  // changes the selection according to relativeCaretPosition, which is
  // relative to the end of the inserting text.
  bool commitText(const String& text,
                  const Vector<CompositionUnderline>& underlines,
                  int relativeCaretPosition);

  // Inserts ongoing composing text; changes the selection to the end of
  // the inserting text if DoNotKeepSelection, or holds the selection if
  // KeepSelection.
  bool finishComposingText(ConfirmCompositionBehavior);

  // Deletes the existing composition text.
  void cancelComposition();

  EphemeralRange compositionEphemeralRange() const;
  Range* compositionRange() const;

  void clear();
  void documentAttached(Document*);

  PlainTextRange getSelectionOffsets() const;
  // Returns true if setting selection to specified offsets, otherwise false.
  bool setEditableSelectionOffsets(
      const PlainTextRange&,
      FrameSelection::SetSelectionOptions = FrameSelection::CloseTyping);
  void extendSelectionAndDelete(int before, int after);
  PlainTextRange createRangeForSelection(int start,
                                         int end,
                                         size_t textLength) const;
  void deleteSurroundingText(int before, int after);
  void deleteSurroundingTextInCodePoints(int before, int after);
  WebTextInputInfo textInputInfo() const;
  WebTextInputType textInputType() const;

  // Call this when we will change focus.
  void willChangeFocus();

 private:
  Document& document() const;
  bool isAvailable() const;

  Member<LocalFrame> m_frame;
  Member<Range> m_compositionRange;
  bool m_hasComposition;

  explicit InputMethodController(LocalFrame&);

  Editor& editor() const;
  LocalFrame& frame() const {
    DCHECK(m_frame);
    return *m_frame;
  }

  String composingText() const;
  void selectComposition() const;

  EphemeralRange ephemeralRangeForOffsets(const PlainTextRange&) const;

  // Returns true if selection offsets were successfully set.
  bool setSelectionOffsets(
      const PlainTextRange&,
      FrameSelection::SetSelectionOptions = FrameSelection::CloseTyping);

  void addCompositionUnderlines(const Vector<CompositionUnderline>& underlines,
                                ContainerNode* baseElement,
                                unsigned offsetInPlainChars);

  bool insertText(const String&);
  bool insertTextAndMoveCaret(const String&,
                              int relativeCaretPosition,
                              const Vector<CompositionUnderline>& underlines);

  // Inserts the given text string in the place of the existing composition.
  // Returns true if did replace.
  bool replaceComposition(const String& text);
  // Inserts the given text string in the place of the existing composition
  // and moves caret. Returns true if did replace and moved caret successfully.
  bool replaceCompositionAndMoveCaret(
      const String&,
      int relativeCaretPosition,
      const Vector<CompositionUnderline>& underlines);

  // Returns true if moved caret successfully.
  bool moveCaret(int newCaretPosition);

  PlainTextRange createSelectionRangeForSetComposition(int selectionStart,
                                                       int selectionEnd,
                                                       size_t textLength) const;
  int textInputFlags() const;
  WebTextInputMode inputModeOfFocusedElement() const;

  // Implements |SynchronousMutationObserver|.
  void contextDestroyed(Document*) final;
};

}  // namespace blink

#endif  // InputMethodController_h
