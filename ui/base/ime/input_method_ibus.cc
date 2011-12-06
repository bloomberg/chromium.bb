// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_ibus.h"

#include <ibus.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/utf_string_conversions.h"
#include "ui/base/events.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/rect.h"

namespace {

XKeyEvent* GetKeyEvent(XEvent* event) {
  DCHECK(event && (event->type == KeyPress || event->type == KeyRelease));
  return &event->xkey;
}

// Converts ibus key state flags to event flags.
int EventFlagsFromIBusState(guint32 state) {
  return (state & IBUS_LOCK_MASK ? ui::EF_CAPS_LOCK_DOWN : 0) |
         (state & IBUS_CONTROL_MASK ? ui::EF_CONTROL_DOWN : 0) |
         (state & IBUS_SHIFT_MASK ? ui::EF_SHIFT_DOWN : 0) |
         (state & IBUS_MOD1_MASK ? ui::EF_ALT_DOWN : 0) |
         (state & IBUS_BUTTON1_MASK ? ui::EF_LEFT_BUTTON_DOWN : 0) |
         (state & IBUS_BUTTON2_MASK ? ui::EF_MIDDLE_BUTTON_DOWN : 0) |
         (state & IBUS_BUTTON3_MASK ? ui::EF_RIGHT_BUTTON_DOWN : 0);
}

// Converts X flags to ibus key state flags.
guint32 IBusStateFromXFlags(unsigned int flags) {
  return (flags & LockMask ? IBUS_LOCK_MASK : 0U) |
      (flags & ControlMask ? IBUS_CONTROL_MASK : 0U) |
      (flags & ShiftMask ? IBUS_SHIFT_MASK : 0U) |
      (flags & Mod1Mask ? IBUS_MOD1_MASK : 0U) |
      (flags & Button1Mask ? IBUS_BUTTON1_MASK : 0U) |
      (flags & Button2Mask ? IBUS_BUTTON2_MASK : 0U) |
      (flags & Button3Mask ? IBUS_BUTTON3_MASK : 0U);
}

// Converts X flags to event flags.
guint32 EventFlagsFromXFlags(unsigned int flags) {
  return EventFlagsFromIBusState(IBusStateFromXFlags(flags));
}

void IBusKeyEventFromNativeKeyEvent(const base::NativeEvent& native_event,
                                    guint32* ibus_keyval,
                                    guint32* ibus_keycode,
                                    guint32* ibus_state) {
  DCHECK(native_event);  // A fabricated event is not supported here.
  XKeyEvent* x_key = GetKeyEvent(native_event);

  // Yes, ibus uses X11 keysym. We cannot use XLookupKeysym(), which doesn't
  // translate Shift and CapsLock states.
  KeySym keysym = NoSymbol;
  ::XLookupString(x_key, NULL, 0, &keysym, NULL);
  *ibus_keyval = keysym;
  *ibus_keycode = x_key->keycode;
  *ibus_state = IBusStateFromXFlags(x_key->state);
  if (native_event->type == KeyRelease)
    *ibus_state |= IBUS_RELEASE_MASK;
}

void ExtractCompositionTextFromIBusPreedit(IBusText* text,
                                           guint cursor_position,
                                           ui::CompositionText* composition) {
  composition->Clear();
  composition->text = UTF8ToUTF16(text->text);

  if (composition->text.empty())
    return;

  // ibus uses character index for cursor position and attribute range, but we
  // use char16 offset for them. So we need to do conversion here.
  std::vector<size_t> char16_offsets;
  size_t length = composition->text.length();
  base::i18n::UTF16CharIterator char_iterator(&composition->text);
  do {
    char16_offsets.push_back(char_iterator.array_pos());
  } while (char_iterator.Advance());

  // The text length in Unicode characters.
  guint char_length = static_cast<guint>(char16_offsets.size());
  // Make sure we can convert the value of |char_length| as well.
  char16_offsets.push_back(length);

  size_t cursor_offset =
      char16_offsets[std::min(char_length, cursor_position)];

  composition->selection = ui::Range(cursor_offset);
  if (text->attrs) {
    guint i = 0;
    while (true) {
      IBusAttribute* attr = ibus_attr_list_get(text->attrs, i++);
      if (!attr)
        break;
      if (attr->type != IBUS_ATTR_TYPE_UNDERLINE &&
          attr->type != IBUS_ATTR_TYPE_BACKGROUND) {
        continue;
      }
      guint start = std::min(char_length, attr->start_index);
      guint end = std::min(char_length, attr->end_index);
      if (start >= end)
        continue;
      ui::CompositionUnderline underline(
          char16_offsets[start], char16_offsets[end],
          SK_ColorBLACK, false /* thick */);
      if (attr->type == IBUS_ATTR_TYPE_BACKGROUND) {
        underline.thick = true;
        // If the cursor is at start or end of this underline, then we treat
        // it as the selection range as well, but make sure to set the cursor
        // position to the selection end.
        if (underline.start_offset == cursor_offset) {
          composition->selection.set_start(underline.end_offset);
          composition->selection.set_end(cursor_offset);
        } else if (underline.end_offset == cursor_offset) {
          composition->selection.set_start(underline.start_offset);
          composition->selection.set_end(cursor_offset);
        }
      } else if (attr->type == IBUS_ATTR_TYPE_UNDERLINE) {
        if (attr->value == IBUS_ATTR_UNDERLINE_DOUBLE)
          underline.thick = true;
        else if (attr->value == IBUS_ATTR_UNDERLINE_ERROR)
          underline.color = SK_ColorRED;
      }
      composition->underlines.push_back(underline);
    }
  }

  // Use a black thin underline by default.
  if (composition->underlines.empty()) {
    composition->underlines.push_back(ui::CompositionUnderline(
        0, length, SK_ColorBLACK, false /* thick */));
  }
}

}  // namespace

namespace ui {

// InputMethodIBus::PendingKeyEvent implementation ------------------------
class InputMethodIBus::PendingKeyEvent {
 public:
  PendingKeyEvent(InputMethodIBus* input_method,
                  const base::NativeEvent& native_event,
                  guint32 ibus_keyval);
  ~PendingKeyEvent();

  // Abandon this pending key event. Its result will just be discarded.
  void Abandon() { input_method_ = NULL; }

  InputMethodIBus* input_method() const { return input_method_; }

  // Process this pending key event after we receive its result from the input
  // method. It just call through InputMethodIBus::ProcessKeyEventPostIME().
  void ProcessPostIME(bool handled);

 private:
  InputMethodIBus* input_method_;

  // TODO(yusukes): To support a fabricated key event (which is typically from
  // a virtual keyboard), we might have to copy event type, event flags, key
  // code, 'character_', and 'unmodified_character_'. See views::InputMethodIBus
  // for details.

  // corresponding XEvent data of a key event. It's a plain struct so we can do
  // bitwise copy.
  XKeyEvent x_event_;

  const guint32 ibus_keyval_;

  DISALLOW_COPY_AND_ASSIGN(PendingKeyEvent);
};

InputMethodIBus::PendingKeyEvent::PendingKeyEvent(
    InputMethodIBus* input_method,
    const base::NativeEvent& native_event,
    guint32 ibus_keyval)
    : input_method_(input_method),
      ibus_keyval_(ibus_keyval) {
  DCHECK(input_method_);

  // TODO(yusukes): Support non-native event (from e.g. a virtual keyboard).
  DCHECK(native_event);
  x_event_ = *GetKeyEvent(native_event);
}

InputMethodIBus::PendingKeyEvent::~PendingKeyEvent() {
  if (input_method_)
    input_method_->FinishPendingKeyEvent(this);
}

void InputMethodIBus::PendingKeyEvent::ProcessPostIME(bool handled) {
  if (!input_method_)
    return;

  if (x_event_.type == KeyPress || x_event_.type == KeyRelease) {
    input_method_->ProcessKeyEventPostIME(reinterpret_cast<XEvent*>(&x_event_),
                                          ibus_keyval_,
                                          handled);
    return;
  }

  // TODO(yusukes): Support non-native event (from e.g. a virtual keyboard).
  // See views::InputMethodIBus for details. Never forget to set 'character_'
  // and 'unmodified_character_' to support i18n VKs like a French VK!
}

// InputMethodIBus::PendingCreateICRequest implementation -----------------
class InputMethodIBus::PendingCreateICRequest {
 public:
  PendingCreateICRequest(InputMethodIBus* input_method,
                         PendingCreateICRequest** request_ptr);
  ~PendingCreateICRequest();

  // Abandon this pending key event. Its result will just be discarded.
  void Abandon() {
    input_method_ = NULL;
    request_ptr_ = NULL;
  }

  // Stores the result input context to |input_method_|, or abandon it if
  // |input_method_| is NULL.
  void StoreOrAbandonInputContext(IBusInputContext* ic);

 private:
  InputMethodIBus* input_method_;
  PendingCreateICRequest** request_ptr_;
};

InputMethodIBus::PendingCreateICRequest::PendingCreateICRequest(
    InputMethodIBus* input_method,
    PendingCreateICRequest** request_ptr)
    : input_method_(input_method),
      request_ptr_(request_ptr) {
}

InputMethodIBus::PendingCreateICRequest::~PendingCreateICRequest() {
  if (request_ptr_) {
    DCHECK_EQ(*request_ptr_, this);
    *request_ptr_ = NULL;
  }
}

void InputMethodIBus::PendingCreateICRequest::StoreOrAbandonInputContext(
    IBusInputContext* ic) {
  if (input_method_) {
    input_method_->SetContext(ic);
  } else {
    // ibus_proxy_destroy() will not really release the object, we still need
    // to call g_object_unref() explicitly.
    ibus_proxy_destroy(reinterpret_cast<IBusProxy *>(ic));
    g_object_unref(ic);
  }
}

// InputMethodIBus implementation -----------------------------------------
InputMethodIBus::InputMethodIBus(
    internal::InputMethodDelegate* delegate)
    : context_(NULL),
      pending_create_ic_request_(NULL),
      context_focused_(false),
      composing_text_(false),
      composition_changed_(false),
      suppress_next_result_(false) {
  SetDelegate(delegate);
}

InputMethodIBus::~InputMethodIBus() {
  AbandonAllPendingKeyEvents();
  DestroyContext();

  // Disconnect bus signals
  g_signal_handlers_disconnect_by_func(
      GetIBus(), reinterpret_cast<gpointer>(OnIBusConnectedThunk), this);
  g_signal_handlers_disconnect_by_func(
      GetIBus(), reinterpret_cast<gpointer>(OnIBusDisconnectedThunk), this);
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

void InputMethodIBus::Init(bool focused) {
  // Initializes the connection to ibus daemon. It may happen asynchronously,
  // and as soon as the connection is established, the |context_| will be
  // created automatically.
  IBusBus* bus = GetIBus();

  // connect bus signals
  g_signal_connect(bus, "connected",
                   G_CALLBACK(OnIBusConnectedThunk), this);
  g_signal_connect(bus, "disconnected",
                   G_CALLBACK(OnIBusDisconnectedThunk), this);

  // Creates the |context_| if the connection is already established. In such
  // case, we will not get "connected" signal.
  if (ibus_bus_is_connected(bus))
    CreateContext();

  InputMethodBase::Init(focused);
}

void InputMethodIBus::DispatchKeyEvent(const base::NativeEvent& native_event) {
  DCHECK(native_event && (native_event->type == KeyPress ||
                          native_event->type == KeyRelease));
  DCHECK(system_toplevel_window_focused());

  guint32 ibus_keyval = 0;
  guint32 ibus_keycode = 0;
  guint32 ibus_state = 0;
  IBusKeyEventFromNativeKeyEvent(
      native_event, &ibus_keyval, &ibus_keycode, &ibus_state);

  // If |context_| is not usable, then we can only dispatch the key event as is.
  // We also dispatch the key event directly if the current text input type is
  // TEXT_INPUT_TYPE_PASSWORD, to bypass the input method.
  // Note: We need to send the key event to ibus even if the |context_| is not
  // enabled, so that ibus can have a chance to enable the |context_|.
  if (!context_focused_ ||
      GetTextInputType() == TEXT_INPUT_TYPE_PASSWORD) {
    if (native_event->type == KeyPress)
      ProcessUnfilteredKeyPressEvent(native_event, ibus_keyval);
    else
      DispatchKeyEventPostIME(native_event);
    return;
  }

  PendingKeyEvent* pending_key =
      new PendingKeyEvent(this, native_event, ibus_keyval);
  pending_key_events_.insert(pending_key);

  // Note:
  // 1. We currently set timeout to -1, because ibus doesn't have a mechanism to
  // associate input method results to corresponding key event, thus there is
  // actually no way to abandon results generated by a specific key event. So we
  // actually cannot abandon a specific key event and its result but accept
  // following key events and their results. So a timeout to abandon a key event
  // will not work.
  // 2. We set GCancellable to NULL, because the operation of cancelling a async
  // request also happens asynchronously, thus it's actually useless to us.
  //
  // The fundemental problem of ibus' async API is: it uses Glib's GIO API to
  // realize async communication, but in fact, GIO API is specially designed for
  // concurrent tasks, though it supports async communication as well, the API
  // is much more complicated than an ordinary message based async communication
  // API (such as Chrome's IPC).
  // Thus it's very complicated, if not impossible, to implement a client that
  // fully utilize asynchronous communication without potential problem.
  ibus_input_context_process_key_event_async(
      context_,
      ibus_keyval, ibus_keycode, ibus_state, -1, NULL,
      reinterpret_cast<GAsyncReadyCallback>(ProcessKeyEventDone),
      pending_key);

  // We don't want to suppress the result generated by this key event, but it
  // may cause problem. See comment in ResetContext() method.
  suppress_next_result_ = false;
}

void InputMethodIBus::OnTextInputTypeChanged(const TextInputClient* client) {
  if (context_ && IsTextInputClientFocused(client)) {
    ResetContext();
    UpdateContextFocusState();
  }
  InputMethodBase::OnTextInputTypeChanged(client);
}

void InputMethodIBus::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!context_focused_ || !IsTextInputClientFocused(client))
    return;

  // The current text input type should not be NONE if |context_| is focused.
  DCHECK(!IsTextInputTypeNone());
  const gfx::Rect rect = GetTextInputClient()->GetCaretBounds();

  // This function runs asynchronously.
  ibus_input_context_set_cursor_location(
      context_, rect.x(), rect.y(), rect.width(), rect.height());
}

void InputMethodIBus::CancelComposition(const TextInputClient* client) {
  if (context_focused_ && IsTextInputClientFocused(client))
    ResetContext();
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
  if (context_focused_)
    OnCaretBoundsChanged(focused);
}

void InputMethodIBus::CreateContext() {
  DCHECK(!context_);
  DCHECK(GetIBus());
  DCHECK(ibus_bus_is_connected(GetIBus()));
  DCHECK(!pending_create_ic_request_);

  // Creates the input context asynchronously.
  pending_create_ic_request_ = new PendingCreateICRequest(
      this, &pending_create_ic_request_);
  ibus_bus_create_input_context_async(
      GetIBus(), "chrome", -1, NULL,
      reinterpret_cast<GAsyncReadyCallback>(CreateInputContextDone),
      pending_create_ic_request_);
}

void InputMethodIBus::SetContext(IBusInputContext* ic) {
  DCHECK(ic);
  DCHECK(!context_);
  context_ = ic;

  // connect input context signals
  g_signal_connect(ic, "commit-text",
                   G_CALLBACK(OnCommitTextThunk), this);
  g_signal_connect(ic, "forward-key-event",
                   G_CALLBACK(OnForwardKeyEventThunk), this);
  g_signal_connect(ic, "update-preedit-text",
                   G_CALLBACK(OnUpdatePreeditTextThunk), this);
  g_signal_connect(ic, "show-preedit-text",
                   G_CALLBACK(OnShowPreeditTextThunk), this);
  g_signal_connect(ic, "hide-preedit-text",
                   G_CALLBACK(OnHidePreeditTextThunk), this);
  g_signal_connect(ic, "destroy",
                   G_CALLBACK(OnDestroyThunk), this);

  // TODO(suzhe): support surrounding text.
  guint32 caps = IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS;
  ibus_input_context_set_capabilities(ic, caps);

  UpdateContextFocusState();
  OnInputMethodChanged();
}

void InputMethodIBus::DestroyContext() {
  if (pending_create_ic_request_) {
    DCHECK(!context_);
    // |pending_create_ic_request_| will be deleted in CreateInputContextDone().
    pending_create_ic_request_->Abandon();
    pending_create_ic_request_ = NULL;
  } else if (context_) {
    // ibus_proxy_destroy() will not really release the resource of |context_|
    // object. We still need to handle "destroy" signal and call
    // g_object_unref() there.
    ibus_proxy_destroy(reinterpret_cast<IBusProxy *>(context_));
    DCHECK(!context_);
  }
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
  ibus_input_context_reset(context_);

  character_composer_.Reset();
}

void InputMethodIBus::UpdateContextFocusState() {
  if (!context_) {
    context_focused_ = false;
    return;
  }

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

  // We only focus in |context_| when the focus is in a normal textfield.
  // ibus_input_context_focus_{in|out}() run asynchronously.
  if (old_context_focused && !context_focused_)
    ibus_input_context_focus_out(context_);
  else if (!old_context_focused && context_focused_)
    ibus_input_context_focus_in(context_);
}

void InputMethodIBus::ProcessKeyEventPostIME(
    const base::NativeEvent& native_event,
    guint32 ibus_keyval,
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
    ProcessUnfilteredKeyPressEvent(native_event, ibus_keyval);
  else if (native_event->type == KeyRelease)
    DispatchKeyEventPostIME(native_event);
}

void InputMethodIBus::ProcessFilteredKeyPressEvent(
    const base::NativeEvent& native_event) {
  if (NeedInsertChar())
    DispatchKeyEventPostIME(native_event);
  else
    DispatchFabricatedKeyEventPostIME(
        ET_KEY_PRESSED,
        VKEY_PROCESSKEY,
        EventFlagsFromXFlags(GetKeyEvent(native_event)->state));
}

void InputMethodIBus::ProcessUnfilteredKeyPressEvent(
    const base::NativeEvent& native_event,
    guint32 ibus_keyval) {
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

  // Process compose and dead keys
  if (character_composer_.FilterKeyPress(ibus_keyval)) {
    string16 composed = character_composer_.composed_character();
    if (!composed.empty()) {
      client = GetTextInputClient();
      if (client)
        client->InsertText(composed);
    }
    return;
  }

  // If a key event was not filtered by |context_| and |character_composer_|,
  // then it means the key event didn't generate any result text. So we need
  // to send corresponding character to the focused text input client.
  client = GetTextInputClient();

  const uint32 state =
      EventFlagsFromXFlags(GetKeyEvent(native_event)->state);
  uint16 ch = ui::DefaultSymbolFromXEvent(native_event);
  if (!ch) {
    ch = ui::GetCharacterFromKeyCode(
        ui::KeyboardCodeFromNative(native_event), state);
  }

  if (client && ch)
    client->InsertChar(ch, state);
}

void InputMethodIBus::ProcessUnfilteredFabricatedKeyPressEvent(
    EventType type,
    KeyboardCode key_code,
    int flags,
    guint32 ibus_keyval) {
  TextInputClient* client = GetTextInputClient();
  DispatchFabricatedKeyEventPostIME(type, key_code, flags);

  if (client != GetTextInputClient())
    return;

  if (character_composer_.FilterKeyPress(ibus_keyval)) {
    string16 composed = character_composer_.composed_character();
    if (!composed.empty()) {
      client = GetTextInputClient();
      if (client)
        client->InsertText(composed);
    }
    return;
  }

  client = GetTextInputClient();
  const uint16 ch = ui::GetCharacterFromKeyCode(key_code, flags);
  if (client && ch)
    client->InsertChar(ch, flags);
}

void InputMethodIBus::ProcessInputMethodResult(
    const base::NativeEvent& native_event,
    bool handled) {
  TextInputClient* client = GetTextInputClient();
  DCHECK(client);

  if (result_text_.length()) {
    if (handled && NeedInsertChar()) {
      const uint32 state =
          EventFlagsFromXFlags(GetKeyEvent(native_event)->state);
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

void InputMethodIBus::SendFakeProcessKeyEvent(bool pressed) const {
  DispatchFabricatedKeyEventPostIME(pressed ? ET_KEY_PRESSED : ET_KEY_RELEASED,
                                    VKEY_PROCESSKEY,
                                    0);
}

void InputMethodIBus::FinishPendingKeyEvent(PendingKeyEvent* pending_key) {
  DCHECK(pending_key_events_.count(pending_key));

  // |pending_key| will be deleted in ProcessKeyEventDone().
  pending_key_events_.erase(pending_key);
}

void InputMethodIBus::AbandonAllPendingKeyEvents() {
  for (std::set<PendingKeyEvent*>::iterator i = pending_key_events_.begin();
       i != pending_key_events_.end(); ++i) {
    // The object will be deleted in ProcessKeyEventDone().
    (*i)->Abandon();
  }
  pending_key_events_.clear();
}

void InputMethodIBus::OnCommitText(
    IBusInputContext* context, IBusText* text) {
  DCHECK_EQ(context_, context);
  if (suppress_next_result_ || !text || !text->text)
    return;

  // We need to receive input method result even if the text input type is
  // TEXT_INPUT_TYPE_NONE, to make sure we can always send correct
  // character for each key event to the focused text input client.
  if (!GetTextInputClient())
    return;

  string16 utf16_text(UTF8ToUTF16(text->text));

  // Append the text to the buffer, because commit signal might be fired
  // multiple times when processing a key event.
  result_text_.append(utf16_text);

  // If we are not handling key event, do not bother sending text result if the
  // focused text input client does not support text input.
  if (pending_key_events_.empty() && !IsTextInputTypeNone()) {
    SendFakeProcessKeyEvent(true);
    GetTextInputClient()->InsertText(utf16_text);
    SendFakeProcessKeyEvent(false);
    result_text_.clear();
  }
}

void InputMethodIBus::OnForwardKeyEvent(IBusInputContext* context,
                                        guint keyval,
                                        guint keycode,
                                        guint state) {
  DCHECK_EQ(context_, context);

  KeyboardCode ui_key_code = KeyboardCodeFromXKeysym(keyval);
  if (!ui_key_code)
    return;

  const EventType event =
      (state & IBUS_RELEASE_MASK) ? ET_KEY_RELEASED : ET_KEY_PRESSED;
  const int flags = EventFlagsFromIBusState(state);

  // It is not clear when the input method will forward us a fake key event.
  // If there is a pending key event, then we may already received some input
  // method results, so we dispatch this fake key event directly rather than
  // calling ProcessKeyEventPostIME(), which will clear pending input method
  // results.
  if (event == ET_KEY_PRESSED)
    ProcessUnfilteredFabricatedKeyPressEvent(event, ui_key_code, flags, keyval);
  else
    DispatchFabricatedKeyEventPostIME(event, ui_key_code, flags);
}

void InputMethodIBus::OnShowPreeditText(IBusInputContext* context) {
  DCHECK_EQ(context_, context);
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  composing_text_ = true;
}

void InputMethodIBus::OnUpdatePreeditText(IBusInputContext* context,
                                          IBusText* text,
                                          guint cursor_pos,
                                          gboolean visible) {
  DCHECK_EQ(context_, context);
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  // |visible| argument is very confusing. For example, what's the correct
  // behavior when:
  // 1. OnUpdatePreeditText() is called with a text and visible == false, then
  // 2. OnShowPreeditText() is called afterwards.
  //
  // If it's only for clearing the current preedit text, then why not just use
  // OnHidePreeditText()?
  if (!visible) {
    OnHidePreeditText(context);
    return;
  }

  ExtractCompositionTextFromIBusPreedit(text, cursor_pos, &composition_);

  composition_changed_ = true;

  // In case OnShowPreeditText() is not called.
  if (composition_.text.length())
    composing_text_ = true;

  // If we receive a composition text without pending key event, then we need to
  // send it to the focused text input client directly.
  if (pending_key_events_.empty()) {
    SendFakeProcessKeyEvent(true);
    GetTextInputClient()->SetCompositionText(composition_);
    SendFakeProcessKeyEvent(false);
    composition_changed_ = false;
    composition_.Clear();
  }
}

void InputMethodIBus::OnHidePreeditText(IBusInputContext* context) {
  DCHECK_EQ(context_, context);
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

void InputMethodIBus::OnDestroy(IBusInputContext* context) {
  DCHECK_EQ(context_, context);
  g_object_unref(context_);
  context_ = NULL;
  context_focused_ = false;

  ConfirmCompositionText();

  // We are dead, so we need to ask the client to stop relying on us.
  // We cannot do it in DestroyContext(), because OnDestroy() may be called
  // automatically.
  OnInputMethodChanged();
}

void InputMethodIBus::OnIBusConnected(IBusBus* bus) {
  DCHECK_EQ(GetIBus(), bus);
  DCHECK(ibus_bus_is_connected(bus));

  DestroyContext();
  CreateContext();
}

void InputMethodIBus::OnIBusDisconnected(IBusBus* bus) {
  DCHECK_EQ(GetIBus(), bus);

  // TODO(suzhe): Make sure if we really do not need to handle this signal.
  // And I'm actually wondering if ibus-daemon will release the resource of the
  // |context_| correctly when the connection is lost.
}

// static
IBusBus* InputMethodIBus::GetIBus() {
  // Everything happens in UI thread, so we do not need to care about
  // synchronization issue.
  static IBusBus* ibus = NULL;

  if (!ibus) {
    ibus_init();
    ibus = ibus_bus_new();
    DCHECK(ibus);
  }
  return ibus;
}

// static
void InputMethodIBus::ProcessKeyEventDone(IBusInputContext* context,
                                          GAsyncResult* res,
                                          PendingKeyEvent* data) {
  DCHECK(data);
  DCHECK(!data->input_method() ||
         data->input_method()->context_ == context);

  gboolean handled = ibus_input_context_process_key_event_async_finish(
      context, res, NULL);
  data->ProcessPostIME(handled);
  delete data;
}

// static
void InputMethodIBus::CreateInputContextDone(IBusBus* bus,
                                             GAsyncResult* res,
                                             PendingCreateICRequest* data) {
  DCHECK_EQ(GetIBus(), bus);
  DCHECK(data);
  IBusInputContext* ic =
      ibus_bus_create_input_context_async_finish(bus, res, NULL);
  if (ic)
    data->StoreOrAbandonInputContext(ic);
  delete data;
}

}  // namespace ui
