// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__
#define WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__

#include "DOMWindow.h"
#include "EditorClient.h"
#include "Timer.h"
#include <wtf/Deque.h>

namespace WebCore {
class Frame;
class HTMLInputElement;
class Node;
class PlatformKeyboardEvent;
}

class WebViewImpl;

class EditorClientImpl : public WebCore::EditorClient {
 public:
  EditorClientImpl(WebViewImpl* webview);

  virtual ~EditorClientImpl();
  virtual void pageDestroyed();

  virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*);
  virtual bool smartInsertDeleteEnabled();
  virtual bool isSelectTrailingWhitespaceEnabled();
  virtual bool isContinuousSpellCheckingEnabled();
  virtual void toggleContinuousSpellChecking();
  virtual bool isGrammarCheckingEnabled();
  virtual void toggleGrammarChecking();
  virtual int spellCheckerDocumentTag();

  virtual bool isEditable();

  virtual bool shouldBeginEditing(WebCore::Range* range);
  virtual bool shouldEndEditing(WebCore::Range* range);
  virtual bool shouldInsertNode(WebCore::Node* node, WebCore::Range* range,
                                WebCore::EditorInsertAction action);
  virtual bool shouldInsertText(const WebCore::String& text, WebCore::Range* range,
                                WebCore::EditorInsertAction action);
  virtual bool shouldDeleteRange(WebCore::Range* range);
  virtual bool shouldChangeSelectedRange(WebCore::Range* fromRange,
                                         WebCore::Range* toRange,
                                         WebCore::EAffinity affinity,
                                         bool stillSelecting);
  virtual bool shouldApplyStyle(WebCore::CSSStyleDeclaration* style,
                                WebCore::Range* range);
  virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*);

  virtual void didBeginEditing();
  virtual void respondToChangedContents();
  virtual void respondToChangedSelection();
  virtual void didEndEditing();
  virtual void didWriteSelectionToPasteboard();
  virtual void didSetSelectionTypesForPasteboard();

  virtual void registerCommandForUndo(PassRefPtr<WebCore::EditCommand>);
  virtual void registerCommandForRedo(PassRefPtr<WebCore::EditCommand>);
  virtual void clearUndoRedoOperations();

  virtual bool canUndo() const;
  virtual bool canRedo() const;

  virtual void undo();
  virtual void redo();

  virtual const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
  virtual bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);
  virtual void handleKeyboardEvent(WebCore::KeyboardEvent*);
  virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*);

  virtual void textFieldDidBeginEditing(WebCore::Element*);
  virtual void textFieldDidEndEditing(WebCore::Element* element);
  virtual void textDidChangeInTextField(WebCore::Element*);
  virtual bool doTextFieldCommandFromEvent(WebCore::Element*,WebCore::KeyboardEvent*);
  virtual void textWillBeDeletedInTextField(WebCore::Element*);
  virtual void textDidChangeInTextArea(WebCore::Element*);

  virtual void ignoreWordInSpellDocument(const WebCore::String&);
  virtual void learnWord(const WebCore::String&);
  virtual void checkSpellingOfString(const UChar*, int length,
                                     int* misspellingLocation,
                                     int* misspellingLength);
  virtual void checkGrammarOfString(const UChar*, int length,
                                    WTF::Vector<WebCore::GrammarDetail>&,
                                    int* badGrammarLocation,
                                    int* badGrammarLength);
  virtual WebCore::String getAutoCorrectSuggestionForMisspelledWord(
      const WebCore::String& misspelledWord);
  virtual void updateSpellingUIWithGrammarString(const WebCore::String&,
                                                 const WebCore::GrammarDetail& detail);
  virtual void updateSpellingUIWithMisspelledWord(const WebCore::String&);
  virtual void showSpellingUI(bool show);
  virtual bool spellingUIIsShowing();
  virtual void getGuessesForWord(const WebCore::String&,
                                 WTF::Vector<WebCore::String>& guesses);
  virtual void setInputMethodState(bool enabled);

  // Shows the form autofill popup for |node| if it is an HTMLInputElement and
  // it is empty.  This is called when you press the up or down arrow in a
  // text-field or when clicking an already focused text-field.
  // Returns true if the autofill popup has been scheduled to be shown, false
  // otherwise.
  virtual bool ShowFormAutofillForNode(WebCore::Node* node);

  // Notification that the text changed in |text_field| due to acceptance of
  // a suggestion provided by an autofill popup.  Having a separate callback
  // in this case is a simple way to break the cycle that would otherwise occur
  // if textDidChangeInTextField was called.
  virtual void OnAutofillSuggestionAccepted(
      WebCore::HTMLInputElement* text_field);

 private:
  void ModifySelection(WebCore::Frame* frame,
                       WebCore::KeyboardEvent* event);

  // Triggers autofill for |input_element| if applicable.  This can be form
  // autofill (via a popup-menu) or password autofill depending on
  // |input_element|.  If |form_autofill_only| is true, password autofill is not
  // triggered.
  // |autofill_on_empty_value| indicates whether the autofill should be shown
  // when the text-field is empty.
  // If |requires_caret_at_end| is true, the autofill popup is only shown if the
  // caret is located at the end of the entered text in |input_element|.
  // Returns true if the autofill popup has been scheduled to be shown, false
  // otherwise.
  bool Autofill(WebCore::HTMLInputElement* input_element,
                bool form_autofill_only,
                bool autofill_on_empty_value,
                bool requires_caret_at_end);

 private:
  // Called to process the autofill described by autofill_args_.
  // This method is invoked asynchronously if the caret position is not
  // reflecting the last text change yet, and we need it to decide whether or
  // not to show the autofill popup.
  void DoAutofill(WebCore::Timer<EditorClientImpl>*);

  void CancelPendingAutofill();

  // Returns whether or not the focused control needs spell-checking.
  // Currently, this function just retrieves the focused node and determines
  // whether or not it is a <textarea> element or an element whose
  // contenteditable attribute is true.
  // TODO(hbono): Bug 740540: This code just implements the default behavior
  // proposed in this issue. We should also retrieve "spellcheck" attributes
  // for text fields and create a flag to over-write the default behavior.
  bool ShouldSpellcheckByDefault();

  WebViewImpl* webview_;
  bool in_redo_;

  typedef Deque< RefPtr<WebCore::EditCommand> > EditCommandStack;
  EditCommandStack undo_stack_;
  EditCommandStack redo_stack_;

  // Whether the last entered key was a backspace.
  bool backspace_or_delete_pressed_;

  // This flag is set to false if spell check for this editor is manually
  // turned off. The default setting is SPELLCHECK_AUTOMATIC.
  enum {
    SPELLCHECK_AUTOMATIC,
    SPELLCHECK_FORCED_ON,
    SPELLCHECK_FORCED_OFF
  };
  int spell_check_this_field_status_;

  // Used to delay autofill processing.
  WebCore::Timer<EditorClientImpl> autofill_timer_;

  struct AutofillArgs {
    RefPtr<WebCore::HTMLInputElement> input_element;
    bool autofill_form_only;
    bool autofill_on_empty_value;
    bool require_caret_at_end;
    bool backspace_or_delete_pressed;
  };
  OwnPtr<AutofillArgs> autofill_args_;
};

#endif  // WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__
