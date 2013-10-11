// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_ibus.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef FocusIn
#undef FocusOut

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "chromeos/ime/input_method_manager.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/rect.h"

namespace {

const int kIBusReleaseMask = 1 << 30;

XKeyEvent* GetKeyEvent(XEvent* event) {
  DCHECK(event && (event->type == KeyPress || event->type == KeyRelease));
  return &event->xkey;
}

// Converts X (and ibus) state to event flags.
int EventFlagsFromXState(unsigned int state) {
  return (state & LockMask ? ui::EF_CAPS_LOCK_DOWN : 0) |
      (state & ControlMask ? ui::EF_CONTROL_DOWN : 0) |
      (state & ShiftMask ? ui::EF_SHIFT_DOWN : 0) |
      (state & Mod1Mask ? ui::EF_ALT_DOWN : 0) |
      (state & Button1Mask ? ui::EF_LEFT_MOUSE_BUTTON : 0) |
      (state & Button2Mask ? ui::EF_MIDDLE_MOUSE_BUTTON : 0) |
      (state & Button3Mask ? ui::EF_RIGHT_MOUSE_BUTTON : 0);
}

// Converts X state to ibus key and button state.
uint32 IBusStateFromXState(unsigned int state) {
  return (state & (LockMask | ControlMask | ShiftMask | Mod1Mask |
                   Button1Mask | Button2Mask | Button3Mask));
}

// Converts gfx::Rect to ibus::Rect.
chromeos::ibus::Rect GfxRectToIBusRect(const gfx::Rect& rect) {
  return chromeos::ibus::Rect(rect.x(), rect.y(), rect.width(), rect.height());
}

chromeos::IBusEngineHandlerInterface* GetEngine() {
  return chromeos::IBusBridge::Get()->GetEngineHandler();
}

}  // namespace

namespace ui {

// InputMethodIBus implementation -----------------------------------------
InputMethodIBus::InputMethodIBus(
    internal::InputMethodDelegate* delegate)
    : context_focused_(false),
      composing_text_(false),
      composition_changed_(false),
      suppress_next_result_(false),
      current_keyevent_id_(0),
      previous_textinput_type_(TEXT_INPUT_TYPE_NONE),
      weak_ptr_factory_(this) {
  SetDelegate(delegate);
  chromeos::IBusBridge::Get()->SetInputContextHandler(this);

  UpdateContextFocusState();
  OnInputMethodChanged();
}

InputMethodIBus::~InputMethodIBus() {
  AbandonAllPendingKeyEvents();
  context_focused_ = false;
  ConfirmCompositionText();
  // We are dead, so we need to ask the client to stop relying on us.
  OnInputMethodChanged();

  chromeos::IBusBridge::Get()->SetInputContextHandler(NULL);
}

void InputMethodIBus::OnFocus() {
  InputMethodBase::OnFocus();
  UpdateContextFocusState();
}

void InputMethodIBus::OnBlur() {
  ConfirmCompositionText();
  InputMethodBase::OnBlur();
  UpdateContextFocusState();
}

bool InputMethodIBus::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                               NativeEventResult* result) {
  return false;
}

void InputMethodIBus::ProcessKeyEventDone(uint32 id,
                                          XEvent* event,
                                          uint32 ibus_keyval,
                                          uint32 ibus_keycode,
                                          uint32 ibus_state,
                                          bool is_handled) {
  DCHECK(event);
  std::set<uint32>::iterator it = pending_key_events_.find(id);

  if (it == pending_key_events_.end())
    return;  // Abandoned key event.

  if (event->type == KeyPress) {
    if (is_handled) {
      // IME event has a priority to be handled, so that character composer
      // should be reset.
      character_composer_.Reset();
    } else {
      // If IME does not handle key event, passes keyevent to character composer
      // to be able to compose complex characters.
      is_handled = ExecuteCharacterComposer(ibus_keyval, ibus_keycode,
                                            ibus_state);
    }
  }

  if (event->type == KeyPress || event->type == KeyRelease)
    ProcessKeyEventPostIME(event, ibus_state, is_handled);

  // Do not use |it| for erasing, ProcessKeyEventPostIME may change the
  // |pending_key_events_|.
  pending_key_events_.erase(id);
}

bool InputMethodIBus::DispatchKeyEvent(const base::NativeEvent& native_event) {
  DCHECK(native_event && (native_event->type == KeyPress ||
                          native_event->type == KeyRelease));
  DCHECK(system_toplevel_window_focused());

  uint32 ibus_keyval = 0;
  uint32 ibus_keycode = 0;
  uint32 ibus_state = 0;
  IBusKeyEventFromNativeKeyEvent(
      native_event,
      &ibus_keyval, &ibus_keycode, &ibus_state);

  // If |context_| is not usable, then we can only dispatch the key event as is.
  // We also dispatch the key event directly if the current text input type is
  // TEXT_INPUT_TYPE_PASSWORD, to bypass the input method.
  // Note: We need to send the key event to ibus even if the |context_| is not
  // enabled, so that ibus can have a chance to enable the |context_|.
  if (!context_focused_ || !GetEngine() ||
      GetTextInputType() == TEXT_INPUT_TYPE_PASSWORD ) {
    if (native_event->type == KeyPress) {
      if (ExecuteCharacterComposer(ibus_keyval, ibus_keycode, ibus_state)) {
        // Treating as PostIME event if character composer handles key event and
        // generates some IME event,
        ProcessKeyEventPostIME(native_event, ibus_state, true);
        return true;
      }
      ProcessUnfilteredKeyPressEvent(native_event, ibus_state);
    } else {
      DispatchKeyEventPostIME(native_event);
    }
    return true;
  }

  pending_key_events_.insert(current_keyevent_id_);

  // Since |native_event| might be treated as XEvent whose size is bigger than
  // XKeyEvent e.g. in CopyNativeEvent() in ui/events/event.cc, allocating
  // |event| as XKeyEvent and casting it to XEvent is unsafe. crbug.com/151884
  XEvent* event = new XEvent;
  *event = *native_event;
  GetEngine()->ProcessKeyEvent(
      ibus_keyval,
      ibus_keycode,
      ibus_state,
      base::Bind(&InputMethodIBus::ProcessKeyEventDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 current_keyevent_id_,
                 base::Owned(event),  // Pass the ownership of |event|.
                 ibus_keyval,
                 ibus_keycode,
                 ibus_state));

  ++current_keyevent_id_;

  // We don't want to suppress the result generated by this key event, but it
  // may cause problem. See comment in ResetContext() method.
  suppress_next_result_ = false;
  return true;
}

bool InputMethodIBus::DispatchFabricatedKeyEvent(const ui::KeyEvent& event) {
  // TODO(bryeung): The fabricated events should also pass through IME.
  if (event.type() == ET_KEY_PRESSED) {
    ProcessUnfilteredFabricatedKeyPressEvent(
        ET_KEY_PRESSED, event.key_code(), event.flags());
  } else {
    DispatchFabricatedKeyEventPostIME(
        ET_KEY_RELEASED,
        event.key_code(),
        event.flags());
  }
  return true;
}

void InputMethodIBus::OnTextInputTypeChanged(const TextInputClient* client) {
  if (IsTextInputClientFocused(client)) {
    ResetContext();
    UpdateContextFocusState();
    if (previous_textinput_type_ != client->GetTextInputType())
      OnInputMethodChanged();
  }
  InputMethodBase::OnTextInputTypeChanged(client);
}

void InputMethodIBus::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!context_focused_ || !IsTextInputClientFocused(client))
    return;

  // The current text input type should not be NONE if |context_| is focused.
  DCHECK(!IsTextInputTypeNone());
  const gfx::Rect rect = GetTextInputClient()->GetCaretBounds();

  gfx::Rect composition_head;
  if (!GetTextInputClient()->GetCompositionCharacterBounds(0,
                                                           &composition_head)) {
    composition_head = rect;
  }

  chromeos::IBusPanelCandidateWindowHandlerInterface* candidate_window =
      chromeos::IBusBridge::Get()->GetCandidateWindowHandler();
  if (!candidate_window)
    return;
  candidate_window->SetCursorLocation(
      GfxRectToIBusRect(rect),
      GfxRectToIBusRect(composition_head));

  gfx::Range text_range;
  gfx::Range selection_range;
  string16 surrounding_text;
  if (!GetTextInputClient()->GetTextRange(&text_range) ||
      !GetTextInputClient()->GetTextFromRange(text_range, &surrounding_text) ||
      !GetTextInputClient()->GetSelectionRange(&selection_range)) {
    previous_surrounding_text_.clear();
    previous_selection_range_ = gfx::Range::InvalidRange();
    return;
  }

  if (previous_selection_range_ == selection_range &&
      previous_surrounding_text_ == surrounding_text)
    return;

  previous_selection_range_ = selection_range;
  previous_surrounding_text_ = surrounding_text;

  if (!selection_range.IsValid()) {
    // TODO(nona): Ideally selection_range should not be invalid.
    // TODO(nona): If javascript changes the focus on page loading, even (0,0)
    //             can not be obtained. Need investigation.
    return;
  }

  // Here SetSurroundingText accepts relative position of |surrounding_text|, so
  // we have to convert |selection_range| from node coordinates to
  // |surrounding_text| coordinates.
  if (!GetEngine())
    return;
  GetEngine()->SetSurroundingText(
      UTF16ToUTF8(surrounding_text),
      selection_range.start() - text_range.start(),
      selection_range.end() - text_range.start());
}

void InputMethodIBus::CancelComposition(const TextInputClient* client) {
  if (context_focused_ && IsTextInputClientFocused(client))
    ResetContext();
}

void InputMethodIBus::OnInputLocaleChanged() {
  // Not supported.
}

std::string InputMethodIBus::GetInputLocale() {
  // Not supported.
  return "";
}

base::i18n::TextDirection InputMethodIBus::GetInputTextDirection() {
  // Not supported.
  return base::i18n::UNKNOWN_DIRECTION;
}

bool InputMethodIBus::IsActive() {
  return true;
}

bool InputMethodIBus::IsCandidatePopupOpen() const {
  // TODO(yukishiino): Implement this method.
  return false;
}

void InputMethodIBus::OnWillChangeFocusedClient(TextInputClient* focused_before,
                                                TextInputClient* focused) {
  ConfirmCompositionText();
}

void InputMethodIBus::OnDidChangeFocusedClient(TextInputClient* focused_before,
                                               TextInputClient* focused) {
  // Force to update the input type since client's TextInputStateChanged()
  // function might not be called if text input types before the client loses
  // focus and after it acquires focus again are the same.
  OnTextInputTypeChanged(focused);

  UpdateContextFocusState();
  // Force to update caret bounds, in case the client thinks that the caret
  // bounds has not changed.
  OnCaretBoundsChanged(focused);
}

void InputMethodIBus::ConfirmCompositionText() {
  TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText();

  ResetContext();
}

void InputMethodIBus::ResetContext() {
  if (!context_focused_ || !GetTextInputClient())
    return;

  DCHECK(system_toplevel_window_focused());

  // Because ibus runs in asynchronous mode, the input method may still send us
  // results after sending out the reset request, so we use a flag to discard
  // all results generated by previous key events. But because ibus does not
  // have a mechanism to identify each key event and corresponding results, this
  // approach will not work for some corner cases. For example if the user types
  // very fast, then the next key event may come in before the |context_| is
  // really reset. Then we actually cannot know whether or not the next
  // result should be discard.
  suppress_next_result_ = true;

  composition_.Clear();
  result_text_.clear();
  composing_text_ = false;
  composition_changed_ = false;

  // We need to abandon all pending key events, but as above comment says, there
  // is no reliable way to abandon all results generated by these abandoned key
  // events.
  AbandonAllPendingKeyEvents();

  // This function runs asynchronously.
  // Note: some input method engines may not support reset method, such as
  // ibus-anthy. But as we control all input method engines by ourselves, we can
  // make sure that all of the engines we are using support it correctly.
  if (GetEngine())
    GetEngine()->Reset();

  character_composer_.Reset();
}

void InputMethodIBus::UpdateContextFocusState() {
  const bool old_context_focused = context_focused_;
  // Use switch here in case we are going to add more text input types.
  switch (GetTextInputType()) {
    case TEXT_INPUT_TYPE_NONE:
    case TEXT_INPUT_TYPE_PASSWORD:
      context_focused_ = false;
      break;
    default:
      context_focused_ = true;
      break;
  }
  if (!GetEngine())
    return;

  // We only focus in |context_| when the focus is in a normal textfield.
  // ibus_input_context_focus_{in|out}() run asynchronously.
  if (old_context_focused && !context_focused_) {
    GetEngine()->FocusOut();
  } else if (!old_context_focused && context_focused_) {
    GetEngine()->FocusIn();
    OnCaretBoundsChanged(GetTextInputClient());
  }
}

void InputMethodIBus::ProcessKeyEventPostIME(
    const base::NativeEvent& native_event,
    uint32 ibus_state,
    bool handled) {
  TextInputClient* client = GetTextInputClient();

  if (!client) {
    // As ibus works asynchronously, there is a chance that the focused client
    // loses focus before this method gets called.
    DispatchKeyEventPostIME(native_event);
    return;
  }

  if (native_event->type == KeyPress && handled)
    ProcessFilteredKeyPressEvent(native_event);

  // In case the focus was changed by the key event. The |context_| should have
  // been reset when the focused window changed.
  if (client != GetTextInputClient())
    return;

  if (HasInputMethodResult())
    ProcessInputMethodResult(native_event, handled);

  // In case the focus was changed when sending input method results to the
  // focused window.
  if (client != GetTextInputClient())
    return;

  if (native_event->type == KeyPress && !handled)
    ProcessUnfilteredKeyPressEvent(native_event, ibus_state);
  else if (native_event->type == KeyRelease)
    DispatchKeyEventPostIME(native_event);
}

void InputMethodIBus::IBusKeyEventFromNativeKeyEvent(
    const base::NativeEvent& native_event,
    uint32* ibus_keyval,
    uint32* ibus_keycode,
    uint32* ibus_state) {
  DCHECK(native_event);  // A fabricated event is not supported here.
  XKeyEvent* x_key = GetKeyEvent(native_event);

  // Yes, ibus uses X11 keysym. We cannot use XLookupKeysym(), which doesn't
  // translate Shift and CapsLock states.
  KeySym keysym = NoSymbol;
  ::XLookupString(x_key, NULL, 0, &keysym, NULL);
  *ibus_keyval = keysym;
  *ibus_keycode = x_key->keycode;
  *ibus_state = IBusStateFromXState(x_key->state);
  if (native_event->type == KeyRelease)
    *ibus_state |= kIBusReleaseMask;
}

void InputMethodIBus::ProcessFilteredKeyPressEvent(
    const base::NativeEvent& native_event) {
  if (NeedInsertChar())
    DispatchKeyEventPostIME(native_event);
  else
    DispatchFabricatedKeyEventPostIME(
        ET_KEY_PRESSED,
        VKEY_PROCESSKEY,
        EventFlagsFromXState(GetKeyEvent(native_event)->state));
}

void InputMethodIBus::ProcessUnfilteredKeyPressEvent(
    const base::NativeEvent& native_event,
    uint32 ibus_state) {
  // For a fabricated event, ProcessUnfilteredFabricatedKeyPressEvent should be
  // called instead.
  DCHECK(native_event);

  TextInputClient* client = GetTextInputClient();
  DispatchKeyEventPostIME(native_event);

  // We shouldn't dispatch the character anymore if the key event dispatch
  // caused focus change. For example, in the following scenario,
  // 1. visit a web page which has a <textarea>.
  // 2. click Omnibox.
  // 3. enable Korean IME, press A, then press Tab to move the focus to the web
  //    page.
  // We should return here not to send the Tab key event to RWHV.
  if (client != GetTextInputClient())
    return;

  const uint32 event_flags = EventFlagsFromXState(ibus_state);

  // If a key event was not filtered by |context_| and |character_composer_|,
  // then it means the key event didn't generate any result text. So we need
  // to send corresponding character to the focused text input client.
  client = GetTextInputClient();

  uint16 ch = 0;
  if (!(event_flags & ui::EF_CONTROL_DOWN))
    ch = ui::GetCharacterFromXEvent(native_event);
  if (!ch) {
    ch = ui::GetCharacterFromKeyCode(
        ui::KeyboardCodeFromNative(native_event), event_flags);
  }

  if (client && ch)
    client->InsertChar(ch, event_flags);
}

void InputMethodIBus::ProcessUnfilteredFabricatedKeyPressEvent(
    EventType type,
    KeyboardCode key_code,
    int event_flags) {
  TextInputClient* client = GetTextInputClient();
  DispatchFabricatedKeyEventPostIME(type, key_code, event_flags);

  if (client != GetTextInputClient())
    return;

  client = GetTextInputClient();
  const uint16 ch = ui::GetCharacterFromKeyCode(key_code, event_flags);
  if (client && ch)
    client->InsertChar(ch, event_flags);
}

void InputMethodIBus::ProcessInputMethodResult(
    const base::NativeEvent& native_event,
    bool handled) {
  TextInputClient* client = GetTextInputClient();
  DCHECK(client);

  if (result_text_.length()) {
    if (handled && NeedInsertChar()) {
      const uint32 state =
          EventFlagsFromXState(GetKeyEvent(native_event)->state);
      for (string16::const_iterator i = result_text_.begin();
           i != result_text_.end(); ++i) {
        client->InsertChar(*i, state);
      }
    } else {
      client->InsertText(result_text_);
      composing_text_ = false;
    }
  }

  if (composition_changed_ && !IsTextInputTypeNone()) {
    if (composition_.text.length()) {
      composing_text_ = true;
      client->SetCompositionText(composition_);
    } else if (result_text_.empty()) {
      client->ClearCompositionText();
    }
  }

  // We should not clear composition text here, as it may belong to the next
  // composition session.
  result_text_.clear();
  composition_changed_ = false;
}

bool InputMethodIBus::NeedInsertChar() const {
  return GetTextInputClient() &&
      (IsTextInputTypeNone() ||
       (!composing_text_ && result_text_.length() == 1));
}

bool InputMethodIBus::HasInputMethodResult() const {
  return result_text_.length() || composition_changed_;
}

void InputMethodIBus::AbandonAllPendingKeyEvents() {
  pending_key_events_.clear();
}

void InputMethodIBus::CommitText(const std::string& text) {
  if (suppress_next_result_ || text.empty())
    return;

  // We need to receive input method result even if the text input type is
  // TEXT_INPUT_TYPE_NONE, to make sure we can always send correct
  // character for each key event to the focused text input client.
  if (!GetTextInputClient())
    return;

  const string16 utf16_text = UTF8ToUTF16(text);
  if (utf16_text.empty())
    return;

  // Append the text to the buffer, because commit signal might be fired
  // multiple times when processing a key event.
  result_text_.append(utf16_text);

  // If we are not handling key event, do not bother sending text result if the
  // focused text input client does not support text input.
  if (pending_key_events_.empty() && !IsTextInputTypeNone()) {
    GetTextInputClient()->InsertText(utf16_text);
    result_text_.clear();
  }
}

void InputMethodIBus::ForwardKeyEvent(uint32 keyval,
                                      uint32 keycode,
                                      uint32 state) {
  KeyboardCode ui_key_code = KeyboardCodeFromXKeysym(keyval);
  if (!ui_key_code)
    return;

  const EventType event_type =
      (state & kIBusReleaseMask) ? ET_KEY_RELEASED : ET_KEY_PRESSED;
  const int event_flags = EventFlagsFromXState(state);

  // It is not clear when the input method will forward us a fake key event.
  // If there is a pending key event, then we may already received some input
  // method results, so we dispatch this fake key event directly rather than
  // calling ProcessKeyEventPostIME(), which will clear pending input method
  // results.
  if (event_type == ET_KEY_PRESSED) {
    ProcessUnfilteredFabricatedKeyPressEvent(event_type, ui_key_code,
                                             event_flags);
  } else {
    DispatchFabricatedKeyEventPostIME(event_type, ui_key_code, event_flags);
  }
}

void InputMethodIBus::ShowPreeditText() {
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  composing_text_ = true;
}

void InputMethodIBus::UpdatePreeditText(const chromeos::IBusText& text,
                                        uint32 cursor_pos,
                                        bool visible) {
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  if (!CanComposeInline()) {
    chromeos::IBusPanelCandidateWindowHandlerInterface* candidate_window =
        chromeos::IBusBridge::Get()->GetCandidateWindowHandler();
    if (candidate_window)
      candidate_window->UpdatePreeditText(text.text(), cursor_pos, visible);
  }

  // |visible| argument is very confusing. For example, what's the correct
  // behavior when:
  // 1. OnUpdatePreeditText() is called with a text and visible == false, then
  // 2. OnShowPreeditText() is called afterwards.
  //
  // If it's only for clearing the current preedit text, then why not just use
  // OnHidePreeditText()?
  if (!visible) {
    HidePreeditText();
    return;
  }

  ExtractCompositionText(text, cursor_pos, &composition_);

  composition_changed_ = true;

  // In case OnShowPreeditText() is not called.
  if (composition_.text.length())
    composing_text_ = true;

  // If we receive a composition text without pending key event, then we need to
  // send it to the focused text input client directly.
  if (pending_key_events_.empty()) {
    GetTextInputClient()->SetCompositionText(composition_);
    composition_changed_ = false;
    composition_.Clear();
  }
}

void InputMethodIBus::HidePreeditText() {
  if (composition_.text.empty() || IsTextInputTypeNone())
    return;

  // Intentionally leaves |composing_text_| unchanged.
  composition_changed_ = true;
  composition_.Clear();

  if (pending_key_events_.empty()) {
    TextInputClient* client = GetTextInputClient();
    if (client && client->HasCompositionText())
      client->ClearCompositionText();
    composition_changed_ = false;
  }
}

void InputMethodIBus::DeleteSurroundingText(int32 offset, uint32 length) {
  if (!composition_.text.empty())
    return;  // do nothing if there is ongoing composition.
  if (offset < 0 && static_cast<uint32>(-1 * offset) != length)
    return;  // only preceding text can be deletable.
  if (GetTextInputClient())
    GetTextInputClient()->ExtendSelectionAndDelete(length, 0U);
}

bool InputMethodIBus::ExecuteCharacterComposer(uint32 ibus_keyval,
                                               uint32 ibus_keycode,
                                               uint32 ibus_state) {
  bool consumed = character_composer_.FilterKeyPress(
      ibus_keyval,
      ibus_keycode,
      EventFlagsFromXState(ibus_state));

  suppress_next_result_ = false;
  chromeos::IBusText preedit;
  preedit.set_text(
      UTF16ToUTF8(character_composer_.preedit_string()));
  UpdatePreeditText(preedit, preedit.text().size(),
                    !preedit.text().empty());
   std::string commit_text =
      UTF16ToUTF8(character_composer_.composed_character());
  if (!commit_text.empty()) {
    CommitText(commit_text);
  }
  return consumed;
}

void InputMethodIBus::ExtractCompositionText(
    const chromeos::IBusText& text,
    uint32 cursor_position,
    CompositionText* out_composition) const {
  out_composition->Clear();
  out_composition->text = UTF8ToUTF16(text.text());

  if (out_composition->text.empty())
    return;

  // ibus uses character index for cursor position and attribute range, but we
  // use char16 offset for them. So we need to do conversion here.
  std::vector<size_t> char16_offsets;
  size_t length = out_composition->text.length();
  base::i18n::UTF16CharIterator char_iterator(&out_composition->text);
  do {
    char16_offsets.push_back(char_iterator.array_pos());
  } while (char_iterator.Advance());

  // The text length in Unicode characters.
  uint32 char_length = static_cast<uint32>(char16_offsets.size());
  // Make sure we can convert the value of |char_length| as well.
  char16_offsets.push_back(length);

  size_t cursor_offset =
      char16_offsets[std::min(char_length, cursor_position)];

  out_composition->selection = gfx::Range(cursor_offset);

  const std::vector<chromeos::IBusText::UnderlineAttribute>&
      underline_attributes = text.underline_attributes();
  const std::vector<chromeos::IBusText::SelectionAttribute>&
      selection_attributes = text.selection_attributes();

  if (!underline_attributes.empty()) {
    for (size_t i = 0; i < underline_attributes.size(); ++i) {
      const uint32 start = underline_attributes[i].start_index;
      const uint32 end = underline_attributes[i].end_index;
      if (start >= end)
        continue;
      CompositionUnderline underline(
          char16_offsets[start], char16_offsets[end],
          SK_ColorBLACK, false /* thick */);
      if (underline_attributes[i].type ==
          chromeos::IBusText::IBUS_TEXT_UNDERLINE_DOUBLE)
        underline.thick = true;
      else if (underline_attributes[i].type ==
               chromeos::IBusText::IBUS_TEXT_UNDERLINE_ERROR)
        underline.color = SK_ColorRED;
      out_composition->underlines.push_back(underline);
    }
  }

  if (!selection_attributes.empty()) {
    LOG_IF(ERROR, selection_attributes.size() != 1)
        << "Chrome does not support multiple selection";
    for (uint32 i = 0; i < selection_attributes.size(); ++i) {
      const uint32 start = selection_attributes[i].start_index;
      const uint32 end = selection_attributes[i].end_index;
      if (start >= end)
        continue;
      CompositionUnderline underline(
          char16_offsets[start], char16_offsets[end],
          SK_ColorBLACK, true /* thick */);
      out_composition->underlines.push_back(underline);
      // If the cursor is at start or end of this underline, then we treat
      // it as the selection range as well, but make sure to set the cursor
      // position to the selection end.
      if (underline.start_offset == cursor_offset) {
        out_composition->selection.set_start(underline.end_offset);
        out_composition->selection.set_end(cursor_offset);
      } else if (underline.end_offset == cursor_offset) {
        out_composition->selection.set_start(underline.start_offset);
        out_composition->selection.set_end(cursor_offset);
      }
    }
  }

  // Use a black thin underline by default.
  if (out_composition->underlines.empty()) {
    out_composition->underlines.push_back(CompositionUnderline(
        0, length, SK_ColorBLACK, false /* thick */));
  }
}

}  // namespace ui
