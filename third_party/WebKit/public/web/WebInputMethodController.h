// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInputMethodController_h
#define WebInputMethodController_h

#include "WebImeTextSpan.h"
#include "WebWidget.h"
#include "public/platform/WebTextInputInfo.h"

namespace blink {

class WebRange;
class WebString;
template <typename T>
class WebVector;

class WebInputMethodController {
 public:
  enum ConfirmCompositionBehavior {
    kDoNotKeepSelection,
    kKeepSelection,
  };

  virtual ~WebInputMethodController() {}

  // Called to inform the WebInputMethodController of a new composition text. If
  // selectionStart and selectionEnd has the same value, then it indicates the
  // input caret position. If the text is empty, then the existing composition
  // text will be canceled. |replacementRange| (when not null) is the range in
  // current text which should be replaced by |text|. Returns true if the
  // composition text was set successfully.
  virtual bool SetComposition(const WebString& text,
                              const WebVector<WebImeTextSpan>& ime_text_spans,
                              const WebRange& replacement_range,
                              int selection_start,
                              int selection_end) = 0;

  // Called to inform the controller to delete the ongoing composition if any,
  // insert |text|, and move the caret according to |relativeCaretPosition|.
  // |replacementRange| (when not null) is the range in current text which
  // should be replaced by |text|.
  virtual bool CommitText(const WebString& text,
                          const WebVector<WebImeTextSpan>& ime_text_spans,
                          const WebRange& replacement_range,
                          int relative_caret_position) = 0;

  // Called to inform the controller to confirm an ongoing composition.
  virtual bool FinishComposingText(
      ConfirmCompositionBehavior selection_behavior) = 0;

  // Returns information about the current text input of this controller. Note
  // that this query can be expensive for long fields, as it returns the
  // plain-text representation of the current editable element. Consider using
  // the lighter-weight textInputType() when appropriate.
  // WebTextInputInfo.flags won't contain Next/Previous flags.
  virtual WebTextInputInfo TextInputInfo() { return WebTextInputInfo(); }

  // This function returns a combination of
  // kWebTextInputFlagHaveNextFocusableElement and
  // kWebTextInputFlagHavePreviousFocusableElement This is splitted from
  // TextInputInfo() due to expensive operations involved.
  virtual int ComputeWebTextInputNextPreviousFlags() { return 0; }

  // Returns the type of current text input of this controller.
  virtual WebTextInputType TextInputType() { return kWebTextInputTypeNone; }

  // Fetch the current selection range of this frame.
  virtual WebRange GetSelectionOffsets() const = 0;
};

}  // namespace blink
#endif
