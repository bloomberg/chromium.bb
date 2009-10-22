// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Mac interface forwards most of these commands to the application layer,
// and I'm not really sure what to do about most of them.

#include "config.h"

#include "Document.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "KeyboardCodes.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformString.h"
#include "RenderObject.h"
#undef LOG

#include "webkit/api/public/WebEditingAction.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebNode.h"
#include "webkit/api/public/WebRange.h"
#include "webkit/api/public/WebTextAffinity.h"
#include "webkit/api/public/WebViewClient.h"
// Can include api/src since eventually editor_client_impl will be there too.
#include "webkit/api/src/DOMUtilitiesPrivate.h"
#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/form_field_values.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/password_autocomplete_listener.h"
#include "webkit/glue/webview_impl.h"

using WebKit::WebEditingAction;
using WebKit::WebString;
using WebKit::WebTextAffinity;
using webkit_glue::FormFieldValues;

// Arbitrary depth limit for the undo stack, to keep it from using
// unbounded memory.  This is the maximum number of distinct undoable
// actions -- unbroken stretches of typed characters are coalesced
// into a single action.
static const size_t kMaximumUndoStackDepth = 1000;

// The size above which we stop triggering autofill for an input text field
// (so to avoid sending long strings through IPC).
static const size_t kMaximumTextSizeForAutofill = 1000;

EditorClientImpl::EditorClientImpl(WebViewImpl* webview)
    : webview_(webview),
      in_redo_(false),
      backspace_or_delete_pressed_(false),
      spell_check_this_field_status_(SPELLCHECK_AUTOMATIC),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          autofill_timer_(this, &EditorClientImpl::DoAutofill)) {
}

EditorClientImpl::~EditorClientImpl() {
}

void EditorClientImpl::pageDestroyed() {
  // Our lifetime is bound to the WebViewImpl.
}

bool EditorClientImpl::shouldShowDeleteInterface(WebCore::HTMLElement* elem) {
  // Normally, we don't care to show WebCore's deletion UI, so we only enable
  // it if in testing mode and the test specifically requests it by using this
  // magic class name.
  return WebKit::layoutTestMode() &&
      elem->getAttribute(WebCore::HTMLNames::classAttr) == "needsDeletionUI";
}

bool EditorClientImpl::smartInsertDeleteEnabled() {
  if (webview_->client())
    return webview_->client()->isSmartInsertDeleteEnabled();
  return true;
}

bool EditorClientImpl::isSelectTrailingWhitespaceEnabled() {
  if (webview_->client())
    return webview_->client()->isSelectTrailingWhitespaceEnabled();

#if PLATFORM(WIN_OS)
  return true;
#else
  return false;
#endif
}

bool EditorClientImpl::ShouldSpellcheckByDefault() {
  // Spellcheck should be enabled for all editable areas (such as textareas,
  // contentEditable regions, and designMode docs), except text inputs.
  const WebCore::Frame* frame = webview_->GetFocusedWebCoreFrame();
  if (!frame)
    return false;
  const WebCore::Editor* editor = frame->editor();
  if (!editor)
    return false;
  if (editor->spellCheckingEnabledInFocusedNode())
    return true;
  const WebCore::Document* document = frame->document();
  if (!document)
    return false;
  const WebCore::Node* node = document->focusedNode();
  // If |node| is NULL, we default to allowing spellchecking. This is done in
  // order to mitigate the issue when the user clicks outside the textbox, as a
  // result of which |node| becomes NULL, resulting in all the spell check
  // markers being deleted. Also, the Frame will decide not to do spellchecking
  // if the user can't edit - so returning true here will not cause any problems
  // to the Frame's behavior.
  if (!node)
    return true;
  const WebCore::RenderObject* renderer = node->renderer();
  if (!renderer)
    return false;

  return !renderer->isTextField();
}

bool EditorClientImpl::isContinuousSpellCheckingEnabled() {
  if (spell_check_this_field_status_ == SPELLCHECK_FORCED_OFF)
    return false;
  if (spell_check_this_field_status_ == SPELLCHECK_FORCED_ON)
    return true;
  return ShouldSpellcheckByDefault();
}

void EditorClientImpl::toggleContinuousSpellChecking() {
  if (isContinuousSpellCheckingEnabled())
    spell_check_this_field_status_ = SPELLCHECK_FORCED_OFF;
  else
    spell_check_this_field_status_ = SPELLCHECK_FORCED_ON;
}

bool EditorClientImpl::isGrammarCheckingEnabled() {
  return false;
}

void EditorClientImpl::toggleGrammarChecking() {
  notImplemented();
}

int EditorClientImpl::spellCheckerDocumentTag() {
  ASSERT_NOT_REACHED();
  return 0;
}

bool EditorClientImpl::isEditable() {
  return false;
}

bool EditorClientImpl::shouldBeginEditing(WebCore::Range* range) {
  if (webview_->client()) {
    return webview_->client()->shouldBeginEditing(
        webkit_glue::RangeToWebRange(range));
  }
  return true;
}

bool EditorClientImpl::shouldEndEditing(WebCore::Range* range) {
  if (webview_->client()) {
    return webview_->client()->shouldEndEditing(
        webkit_glue::RangeToWebRange(range));
  }
  return true;
}

bool EditorClientImpl::shouldInsertNode(WebCore::Node* node,
                                        WebCore::Range* range,
                                        WebCore::EditorInsertAction action) {
  if (webview_->client()) {
    return webview_->client()->shouldInsertNode(
        webkit_glue::NodeToWebNode(node),
        webkit_glue::RangeToWebRange(range),
        static_cast<WebEditingAction>(action));
  }
  return true;
}

bool EditorClientImpl::shouldInsertText(const WebCore::String& text,
                                        WebCore::Range* range,
                                        WebCore::EditorInsertAction action) {
  if (webview_->client()) {
    return webview_->client()->shouldInsertText(
        webkit_glue::StringToWebString(text),
        webkit_glue::RangeToWebRange(range),
        static_cast<WebEditingAction>(action));
  }
  return true;
}


bool EditorClientImpl::shouldDeleteRange(WebCore::Range* range) {
  if (webview_->client()) {
    return webview_->client()->shouldDeleteRange(
        webkit_glue::RangeToWebRange(range));
  }
  return true;
}

bool EditorClientImpl::shouldChangeSelectedRange(WebCore::Range* from_range,
                                                 WebCore::Range* to_range,
                                                 WebCore::EAffinity affinity,
                                                 bool still_selecting) {
  if (webview_->client()) {
    return webview_->client()->shouldChangeSelectedRange(
        webkit_glue::RangeToWebRange(from_range),
        webkit_glue::RangeToWebRange(to_range),
        static_cast<WebTextAffinity>(affinity),
        still_selecting);
  }
  return true;
}

bool EditorClientImpl::shouldApplyStyle(WebCore::CSSStyleDeclaration* style,
                                        WebCore::Range* range) {
  if (webview_->client()) {
    // TODO(darin): Pass a reference to the CSSStyleDeclaration somehow.
    return webview_->client()->shouldApplyStyle(
        WebString(),
        webkit_glue::RangeToWebRange(range));
  }
  return true;
}

bool EditorClientImpl::shouldMoveRangeAfterDelete(
    WebCore::Range* /*range*/,
    WebCore::Range* /*rangeToBeReplaced*/) {
  return true;
}

void EditorClientImpl::didBeginEditing() {
  if (webview_->client())
    webview_->client()->didBeginEditing();
}

void EditorClientImpl::respondToChangedSelection() {
  if (webview_->client()) {
    WebCore::Frame* frame = webview_->GetFocusedWebCoreFrame();
    if (frame)
      webview_->client()->didChangeSelection(!frame->selection()->isRange());
  }
}

void EditorClientImpl::respondToChangedContents() {
  if (webview_->client())
    webview_->client()->didChangeContents();
}

void EditorClientImpl::didEndEditing() {
  if (webview_->client())
    webview_->client()->didEndEditing();
}

void EditorClientImpl::didWriteSelectionToPasteboard() {
}

void EditorClientImpl::didSetSelectionTypesForPasteboard() {
}

void EditorClientImpl::registerCommandForUndo(
    PassRefPtr<WebCore::EditCommand> command) {
  if (undo_stack_.size() == kMaximumUndoStackDepth)
    undo_stack_.removeFirst();  // drop oldest item off the far end
  if (!in_redo_)
    redo_stack_.clear();
  undo_stack_.append(command);
}

void EditorClientImpl::registerCommandForRedo(
    PassRefPtr<WebCore::EditCommand> command) {
  redo_stack_.append(command);
}

void EditorClientImpl::clearUndoRedoOperations() {
  undo_stack_.clear();
  redo_stack_.clear();
}

bool EditorClientImpl::canUndo() const {
  return !undo_stack_.isEmpty();
}

bool EditorClientImpl::canRedo() const {
  return !redo_stack_.isEmpty();
}

void EditorClientImpl::undo() {
  if (canUndo()) {
    EditCommandStack::iterator back = --undo_stack_.end();
    RefPtr<WebCore::EditCommand> command(*back);
    undo_stack_.remove(back);
    command->unapply();
    // unapply will call us back to push this command onto the redo stack.
  }
}

void EditorClientImpl::redo() {
  if (canRedo()) {
    EditCommandStack::iterator back = --redo_stack_.end();
    RefPtr<WebCore::EditCommand> command(*back);
    redo_stack_.remove(back);

    ASSERT(!in_redo_);
    in_redo_ = true;
    command->reapply();
    // reapply will call us back to push this command onto the undo stack.
    in_redo_ = false;
  }
}

// The below code was adapted from the WebKit file webview.cpp
// provided by Apple, Inc, and is subject to the following copyright
// notice and disclaimer.

/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
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

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;
static const unsigned MetaKey = 1 << 3;
#if PLATFORM(DARWIN)
// Aliases for the generic key defintions to make kbd shortcuts definitions more
// readable on OS X.
static const unsigned OptionKey  = AltKey;
static const unsigned CommandKey = MetaKey;
#endif

// Keys with special meaning. These will be delegated to the editor using
// the execCommand() method
struct KeyDownEntry {
  unsigned virtualKey;
  unsigned modifiers;
  const char* name;
};

struct KeyPressEntry {
  unsigned charCode;
  unsigned modifiers;
  const char* name;
};

static const KeyDownEntry keyDownEntries[] = {
  { WebCore::VKEY_LEFT,   0,                  "MoveLeft"                      },
  { WebCore::VKEY_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"    },
#if PLATFORM(DARWIN)
  { WebCore::VKEY_LEFT,   OptionKey,          "MoveWordLeft"                  },
  { WebCore::VKEY_LEFT,   OptionKey | ShiftKey,
        "MoveWordLeftAndModifySelection"                                      },
#else
  { WebCore::VKEY_LEFT,   CtrlKey,            "MoveWordLeft"                  },
  { WebCore::VKEY_LEFT,   CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection"},
#endif
  { WebCore::VKEY_RIGHT,  0,                  "MoveRight"                     },
  { WebCore::VKEY_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"   },
#if PLATFORM(DARWIN)
  { WebCore::VKEY_RIGHT,  OptionKey,          "MoveWordRight"                 },
  { WebCore::VKEY_RIGHT,  OptionKey | ShiftKey,
        "MoveWordRightAndModifySelection"                                     },
#else
  { WebCore::VKEY_RIGHT,  CtrlKey,            "MoveWordRight"                 },
  { WebCore::VKEY_RIGHT,  CtrlKey | ShiftKey,
        "MoveWordRightAndModifySelection"                                     },
#endif
  { WebCore::VKEY_UP,     0,                  "MoveUp"                        },
  { WebCore::VKEY_UP,     ShiftKey,           "MoveUpAndModifySelection"      },
  { WebCore::VKEY_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection"  },
  { WebCore::VKEY_DOWN,   0,                  "MoveDown"                      },
  { WebCore::VKEY_DOWN,   ShiftKey,           "MoveDownAndModifySelection"    },
  { WebCore::VKEY_NEXT,   ShiftKey,           "MovePageDownAndModifySelection"},
  { WebCore::VKEY_PRIOR,  0,                  "MovePageUp"                    },
  { WebCore::VKEY_NEXT,   0,                  "MovePageDown"                  },
  { WebCore::VKEY_HOME,   0,                  "MoveToBeginningOfLine"         },
  { WebCore::VKEY_HOME,   ShiftKey,
        "MoveToBeginningOfLineAndModifySelection"                             },
#if PLATFORM(DARWIN)
  { WebCore::VKEY_LEFT,   CommandKey,         "MoveToBeginningOfLine"         },
  { WebCore::VKEY_LEFT,   CommandKey | ShiftKey,
        "MoveToBeginningOfLineAndModifySelection"                             },
#endif
#if PLATFORM(DARWIN)
  { WebCore::VKEY_UP,     CommandKey,         "MoveToBeginningOfDocument"     },
  { WebCore::VKEY_UP,     CommandKey | ShiftKey,
        "MoveToBeginningOfDocumentAndModifySelection"                         },
#else
  { WebCore::VKEY_HOME,   CtrlKey,            "MoveToBeginningOfDocument"     },
  { WebCore::VKEY_HOME,   CtrlKey | ShiftKey,
        "MoveToBeginningOfDocumentAndModifySelection"                         },
#endif
  { WebCore::VKEY_END,    0,                  "MoveToEndOfLine"               },
  { WebCore::VKEY_END,    ShiftKey,
        "MoveToEndOfLineAndModifySelection"                                   },
#if PLATFORM(DARWIN)
  { WebCore::VKEY_DOWN,   CommandKey,         "MoveToEndOfDocument"           },
  { WebCore::VKEY_DOWN,   CommandKey | ShiftKey,
        "MoveToEndOfDocumentAndModifySelection"                               },
#else
  { WebCore::VKEY_END,    CtrlKey,            "MoveToEndOfDocument"           },
  { WebCore::VKEY_END,    CtrlKey | ShiftKey,
        "MoveToEndOfDocumentAndModifySelection"                               },
#endif
#if PLATFORM(DARWIN)
  { WebCore::VKEY_RIGHT,  CommandKey,            "MoveToEndOfLine"            },
  { WebCore::VKEY_RIGHT,  CommandKey | ShiftKey,
        "MoveToEndOfLineAndModifySelection"                                   },
#endif
  { WebCore::VKEY_BACK,   0,                  "DeleteBackward"                },
  { WebCore::VKEY_BACK,   ShiftKey,           "DeleteBackward"                },
  { WebCore::VKEY_DELETE, 0,                  "DeleteForward"                 },
#if PLATFORM(DARWIN)
  { WebCore::VKEY_BACK,   OptionKey,          "DeleteWordBackward"            },
  { WebCore::VKEY_DELETE, OptionKey,          "DeleteWordForward"             },
#else
  { WebCore::VKEY_BACK,   CtrlKey,            "DeleteWordBackward"            },
  { WebCore::VKEY_DELETE, CtrlKey,            "DeleteWordForward"             },
#endif
  { 'B',                  CtrlKey,            "ToggleBold"                    },
  { 'I',                  CtrlKey,            "ToggleItalic"                  },
  { 'U',                  CtrlKey,            "ToggleUnderline"               },
  { WebCore::VKEY_ESCAPE, 0,                  "Cancel"                        },
  { WebCore::VKEY_OEM_PERIOD, CtrlKey,        "Cancel"                        },
  { WebCore::VKEY_TAB,    0,                  "InsertTab"                     },
  { WebCore::VKEY_TAB,    ShiftKey,           "InsertBacktab"                 },
  { WebCore::VKEY_RETURN, 0,                  "InsertNewline"                 },
  { WebCore::VKEY_RETURN, CtrlKey,            "InsertNewline"                 },
  { WebCore::VKEY_RETURN, AltKey,             "InsertNewline"                 },
  { WebCore::VKEY_RETURN, AltKey | ShiftKey,  "InsertNewline"                 },
  { WebCore::VKEY_RETURN, ShiftKey,           "InsertLineBreak"               },
  { WebCore::VKEY_INSERT, CtrlKey,            "Copy"                          },
  { WebCore::VKEY_INSERT, ShiftKey,           "Paste"                         },
  { WebCore::VKEY_DELETE, ShiftKey,           "Cut"                           },
#if PLATFORM(DARWIN)
  { 'C',                  CommandKey,         "Copy"                          },
  { 'V',                  CommandKey,         "Paste"                         },
  { 'V',                  CommandKey | ShiftKey,
        "PasteAndMatchStyle"                                                  },
  { 'X',                  CommandKey,         "Cut"                           },
  { 'A',                  CommandKey,         "SelectAll"                     },
  { 'Z',                  CommandKey,         "Undo"                          },
  { 'Z',                  CommandKey | ShiftKey,
        "Redo"                                                                },
  { 'Y',                  CommandKey,         "Redo"                          },
#else
  { 'C',                  CtrlKey,            "Copy"                          },
  { 'V',                  CtrlKey,            "Paste"                         },
  { 'V',                  CtrlKey | ShiftKey, "PasteAndMatchStyle"            },
  { 'X',                  CtrlKey,            "Cut"                           },
  { 'A',                  CtrlKey,            "SelectAll"                     },
  { 'Z',                  CtrlKey,            "Undo"                          },
  { 'Z',                  CtrlKey | ShiftKey, "Redo"                          },
  { 'Y',                  CtrlKey,            "Redo"                          },
#endif
};

static const KeyPressEntry keyPressEntries[] = {
  { '\t',   0,                  "InsertTab"                                   },
  { '\t',   ShiftKey,           "InsertBacktab"                               },
  { '\r',   0,                  "InsertNewline"                               },
  { '\r',   CtrlKey,            "InsertNewline"                               },
  { '\r',   ShiftKey,           "InsertLineBreak"                             },
  { '\r',   AltKey,             "InsertNewline"                               },
  { '\r',   AltKey | ShiftKey,  "InsertNewline"                               },
};

const char* EditorClientImpl::interpretKeyEvent(
  const WebCore::KeyboardEvent* evt) {
  const WebCore::PlatformKeyboardEvent* keyEvent = evt->keyEvent();
  if (!keyEvent)
    return "";

  static HashMap<int, const char*>* keyDownCommandsMap = 0;
  static HashMap<int, const char*>* keyPressCommandsMap = 0;

  if (!keyDownCommandsMap) {
    keyDownCommandsMap = new HashMap<int, const char*>;
    keyPressCommandsMap = new HashMap<int, const char*>;

    for (unsigned i = 0; i < arraysize(keyDownEntries); i++) {
      keyDownCommandsMap->set(
        keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey,
        keyDownEntries[i].name);
    }

    for (unsigned i = 0; i < arraysize(keyPressEntries); i++) {
      keyPressCommandsMap->set(
        keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode,
        keyPressEntries[i].name);
    }
  }

  unsigned modifiers = 0;
  if (keyEvent->shiftKey())
    modifiers |= ShiftKey;
  if (keyEvent->altKey())
    modifiers |= AltKey;
  if (keyEvent->ctrlKey())
    modifiers |= CtrlKey;
  if (keyEvent->metaKey())
    modifiers |= MetaKey;

  if (keyEvent->type() == WebCore::PlatformKeyboardEvent::RawKeyDown) {
    int mapKey = modifiers << 16 | evt->keyCode();
    return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
  }

  int mapKey = modifiers << 16 | evt->charCode();
  return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

bool EditorClientImpl::handleEditingKeyboardEvent(
    WebCore::KeyboardEvent* evt) {
  const WebCore::PlatformKeyboardEvent* keyEvent = evt->keyEvent();
  // do not treat this as text input if it's a system key event
  if (!keyEvent || keyEvent->isSystemKey())
      return false;

  WebCore::Frame* frame = evt->target()->toNode()->document()->frame();
  if (!frame)
    return false;

  WebCore::String command_name = interpretKeyEvent(evt);
  WebCore::Editor::Command command = frame->editor()->command(command_name);

  if (keyEvent->type() == WebCore::PlatformKeyboardEvent::RawKeyDown) {
    // WebKit doesn't have enough information about mode to decide how
    // commands that just insert text if executed via Editor should be treated,
    // so we leave it upon WebCore to either handle them immediately
    // (e.g. Tab that changes focus) or let a keypress event be generated
    // (e.g. Tab that inserts a Tab character, or Enter).
    if (command.isTextInsertion() || command_name.isEmpty())
      return false;
    if (command.execute(evt)) {
      if (webview_->client()) {
        webview_->client()->didExecuteCommand(
            webkit_glue::StringToWebString(command_name));
      }
      return true;
    }
    return false;
  }

  if (command.execute(evt)) {
    if (webview_->client()) {
      webview_->client()->didExecuteCommand(
          webkit_glue::StringToWebString(command_name));
    }
    return true;
  }

  // Here we need to filter key events.
  // On Gtk/Linux, it emits key events with ASCII text and ctrl on for ctrl-<x>.
  // In Webkit, EditorClient::handleKeyboardEvent in
  // WebKit/gtk/WebCoreSupport/EditorClientGtk.cpp drop such events.
  // On Mac, it emits key events with ASCII text and meta on for Command-<x>.
  // These key events should not emit text insert event.
  // Alt key would be used to insert alternative character, so we should let
  // through. Also note that Ctrl-Alt combination equals to AltGr key which is
  // also used to insert alternative character.
  // http://code.google.com/p/chromium/issues/detail?id=10846
  // Windows sets both alt and meta are on when "Alt" key pressed.
  // http://code.google.com/p/chromium/issues/detail?id=2215
  // Also, we should not rely on an assumption that keyboards don't
  // send ASCII characters when pressing a control key on Windows,
  // which may be configured to do it so by user.
  // See also http://en.wikipedia.org/wiki/Keyboard_Layout
  // TODO(ukai): investigate more detail for various keyboard layout.
  if (evt->keyEvent()->text().length() == 1) {
    UChar ch = evt->keyEvent()->text()[0U];

    // Don't insert null or control characters as they can result in
    // unexpected behaviour
    if (ch < ' ')
      return false;
#if !PLATFORM(WIN_OS)
    // Don't insert ASCII character if ctrl w/o alt or meta is on.
    // On Mac, we should ignore events when meta is on (Command-<x>).
    if (ch < 0x80) {
      if (evt->keyEvent()->ctrlKey() && !evt->keyEvent()->altKey())
        return false;
#if PLATFORM(DARWIN)
      if (evt->keyEvent()->metaKey())
        return false;
#endif
    }
#endif
  }

  if (!frame->editor()->canEdit())
    return false;

  return frame->editor()->insertText(evt->keyEvent()->text(), evt);
}

//
// End of code block subject to Apple, Inc. copyright
//

void EditorClientImpl::handleKeyboardEvent(WebCore::KeyboardEvent* evt) {
  if (evt->keyCode() == WebCore::VKEY_DOWN ||
      evt->keyCode() == WebCore::VKEY_UP) {
    ASSERT(evt->target()->toNode());
    ShowFormAutofillForNode(evt->target()->toNode());
  }

  // Give the embedder a chance to handle the keyboard event.
  if ((webview_->client() &&
       webview_->client()->handleCurrentKeyboardEvent()) ||
      handleEditingKeyboardEvent(evt))
    evt->setDefaultHandled();
}

void EditorClientImpl::handleInputMethodKeydown(
    WebCore::KeyboardEvent* keyEvent) {
  // We handle IME within chrome.
}

void EditorClientImpl::textFieldDidBeginEditing(WebCore::Element*) {
}

void EditorClientImpl::textFieldDidEndEditing(WebCore::Element* element) {
  // Notification that focus was lost.  Be careful with this, it's also sent
  // when the page is being closed.

  // Cancel any pending DoAutofill call.
  autofill_args_.clear();
  autofill_timer_.stop();

  // Hide any showing popup.
  webview_->HideAutoCompletePopup();

  if (!webview_->client())
    return;  // The page is getting closed, don't fill the password.

  // Notify any password-listener of the focus change.
  WebCore::HTMLInputElement* input_element =
      WebKit::elementToHTMLInputElement(element);
  if (!input_element)
    return;

  WebFrameImpl* webframe =
      WebFrameImpl::FromFrame(input_element->document()->frame());
  webkit_glue::PasswordAutocompleteListener* listener =
      webframe->GetPasswordListener(input_element);
  if (!listener)
    return;

  listener->OnBlur(input_element, input_element->value());
}

void EditorClientImpl::textDidChangeInTextField(WebCore::Element* element) {
  ASSERT(element->hasLocalName(WebCore::HTMLNames::inputTag));
  // Note that we only show the autofill popup in this case if the caret is at
  // the end.  This matches FireFox and Safari but not IE.
  Autofill(static_cast<WebCore::HTMLInputElement*>(element),
           false, false, true);
}

bool EditorClientImpl::ShowFormAutofillForNode(WebCore::Node* node) {
  WebCore::HTMLInputElement* input_element =
      WebKit::nodeToHTMLInputElement(node);
  if (input_element)
    return Autofill(input_element, true, true, false);
  return false;
}

bool EditorClientImpl::Autofill(WebCore::HTMLInputElement* input_element,
                                bool autofill_form_only,
                                bool autofill_on_empty_value,
                                bool require_caret_at_end) {
  // Cancel any pending DoAutofill call.
  autofill_args_.clear();
  autofill_timer_.stop();

  // Let's try to trigger autofill for that field, if applicable.
  if (!input_element->isEnabledFormControl() || !input_element->isTextField() ||
      input_element->isPasswordField() || !input_element->autoComplete()) {
    return false;
  }

  string16 name = FormFieldValues::GetNameForInputElement(input_element);
  if (name.empty())  // If the field has no name, then we won't have values.
    return false;

  // Don't attempt to autofill with values that are too large.
  if (input_element->value().length() > kMaximumTextSizeForAutofill)
    return false;

  autofill_args_ = new AutofillArgs();
  autofill_args_->input_element = input_element;
  autofill_args_->autofill_form_only = autofill_form_only;
  autofill_args_->autofill_on_empty_value = autofill_on_empty_value;
  autofill_args_->require_caret_at_end = require_caret_at_end;
  autofill_args_->backspace_or_delete_pressed = backspace_or_delete_pressed_;

  if (!require_caret_at_end) {
    DoAutofill(NULL);
  } else {
    // We post a task for doing the autofill as the caret position is not set
    // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976)
    // and we need it to determine whether or not to trigger autofill.
    autofill_timer_.startOneShot(0.0);
  }
  return true;
}

void EditorClientImpl::DoAutofill(WebCore::Timer<EditorClientImpl>* timer) {
  OwnPtr<AutofillArgs> args(autofill_args_.release());
  WebCore::HTMLInputElement* input_element = args->input_element.get();

  const WebCore::String& value = input_element->value();

  // Enforce autofill_on_empty_value and caret_at_end.
  bool is_caret_at_end = args->require_caret_at_end ?
      input_element->selectionStart() == input_element->selectionEnd() &&
      input_element->selectionEnd() == static_cast<int>(value.length()) :
      true;  // When |require_caret_at_end| is false, just pretend we are at
             // the end.
  if ((!args->autofill_on_empty_value && value.isEmpty()) || !is_caret_at_end) {
    webview_->HideAutoCompletePopup();
    return;
  }

  // First let's see if there is a password listener for that element.
  // We won't trigger form autofill in that case, as having both behavior on
  // a node would be confusing.
  WebFrameImpl* webframe =
      WebFrameImpl::FromFrame(input_element->document()->frame());
  webkit_glue::PasswordAutocompleteListener* listener =
      webframe->GetPasswordListener(input_element);
  if (listener) {
    if (args->autofill_form_only)
      return;

    listener->OnInlineAutocompleteNeeded(
        input_element, value, args->backspace_or_delete_pressed, true);
    return;
  }

  // Then trigger form autofill.
  string16 name = FormFieldValues::GetNameForInputElement(input_element);
  ASSERT(static_cast<int>(name.length()) > 0);

  if (webview_->client()) {
    webview_->client()->queryAutofillSuggestions(
        webkit_glue::NodeToWebNode(input_element), name,
        webkit_glue::StringToWebString(value));
  }
}

void EditorClientImpl::CancelPendingAutofill() {
  autofill_args_.clear();
  autofill_timer_.stop();
}

void EditorClientImpl::OnAutofillSuggestionAccepted(
    WebCore::HTMLInputElement* text_field) {
  WebFrameImpl* webframe =
      WebFrameImpl::FromFrame(text_field->document()->frame());
  webkit_glue::PasswordAutocompleteListener* listener =
      webframe->GetPasswordListener(text_field);
  // Password listeners need to autocomplete other fields that depend on the
  // input element with autofill suggestions.
  if (listener) {
    listener->OnInlineAutocompleteNeeded(
        text_field, text_field->value(), false, false);
  }
}

bool EditorClientImpl::doTextFieldCommandFromEvent(
    WebCore::Element* element,
    WebCore::KeyboardEvent* event) {
  // Remember if backspace was pressed for the autofill.  It is not clear how to
  // find if backspace was pressed from textFieldDidBeginEditing and
  // textDidChangeInTextField as when these methods are called the value of the
  // input element already contains the type character.
  backspace_or_delete_pressed_ = (event->keyCode() == WebCore::VKEY_BACK) ||
                                 (event->keyCode() == WebCore::VKEY_DELETE);

  // The Mac code appears to use this method as a hook to implement special
  // keyboard commands specific to Safari's auto-fill implementation.  We
  // just return false to allow the default action.
  return false;
}

void EditorClientImpl::textWillBeDeletedInTextField(WebCore::Element*) {
}

void EditorClientImpl::textDidChangeInTextArea(WebCore::Element*) {
}

void EditorClientImpl::ignoreWordInSpellDocument(const WebCore::String&) {
  notImplemented();
}

void EditorClientImpl::learnWord(const WebCore::String&) {
  notImplemented();
}

void EditorClientImpl::checkSpellingOfString(const UChar* text, int length,
                                             int* misspellingLocation,
                                             int* misspellingLength) {
  // SpellCheckWord will write (0, 0) into the output vars, which is what our
  // caller expects if the word is spelled correctly.
  int spell_location = -1;
  int spell_length = 0;

  // Check to see if the provided text is spelled correctly.
  if (isContinuousSpellCheckingEnabled() && webview_->client()) {
    webview_->client()->spellCheck(
        WebString(text, length), spell_location, spell_length);
  } else {
    spell_location = 0;
    spell_length = 0;
  }

  // Note: the Mac code checks if the pointers are NULL before writing to them,
  // so we do too.
  if (misspellingLocation)
    *misspellingLocation = spell_location;
  if (misspellingLength)
    *misspellingLength = spell_length;
}

WebCore::String EditorClientImpl::getAutoCorrectSuggestionForMisspelledWord(
    const WebCore::String& misspelledWord) {
  if (!(isContinuousSpellCheckingEnabled() && webview_->client()))
    return WebCore::String();

  // Do not autocorrect words with capital letters in it except the
  // first letter. This will remove cases changing "IMB" to "IBM".
  for (size_t i = 1; i < misspelledWord.length(); i++) {
    if (u_isupper(static_cast<UChar32>(misspelledWord[i])))
      return WebCore::String();
  }

  return webkit_glue::WebStringToString(webview_->client()->autoCorrectWord(
      webkit_glue::StringToWebString(misspelledWord)));
}

void EditorClientImpl::checkGrammarOfString(const UChar*, int length,
    WTF::Vector<WebCore::GrammarDetail>&, int* badGrammarLocation,
    int* badGrammarLength) {
  notImplemented();
  if (badGrammarLocation)
    *badGrammarLocation = 0;
  if (badGrammarLength)
    *badGrammarLength = 0;
}

void EditorClientImpl::updateSpellingUIWithGrammarString(const WebCore::String&,
    const WebCore::GrammarDetail& detail) {
  notImplemented();
}

void EditorClientImpl::updateSpellingUIWithMisspelledWord(
    const WebCore::String& misspelled_word) {
  if (webview_->client()) {
    webview_->client()->updateSpellingUIWithMisspelledWord(
        webkit_glue::StringToWebString(misspelled_word));
  }
}

void EditorClientImpl::showSpellingUI(bool show) {
  if (webview_->client())
    webview_->client()->showSpellingUI(show);
}

bool EditorClientImpl::spellingUIIsShowing() {
  if (webview_->client())
    return webview_->client()->isShowingSpellingUI();
  return false;
}

void EditorClientImpl::getGuessesForWord(const WebCore::String&,
    WTF::Vector<WebCore::String>& guesses) {
  notImplemented();
}

void EditorClientImpl::setInputMethodState(bool enabled) {
  if (webview_->client())
    webview_->client()->setInputMethodEnabled(enabled);
}
