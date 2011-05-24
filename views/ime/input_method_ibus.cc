// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/ime/input_method_ibus.h"

#include <ibus.h>

#include <cstring>
#include <set>

#if defined(TOUCH_UI)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include "base/command_line.h"
#include "base/basictypes.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/utf_string_conversions.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "views/events/event.h"
#include "views/widget/widget.h"

#if defined(TOUCH_UI)
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#endif

namespace {

// A global flag to switch the InputMethod implementation to InputMethodIBus
bool inputmethod_ibus_enabled = false;

// Converts ibus key state flags to Views event flags.
int ViewsFlagsFromIBusState(guint32 state) {
  return (state & IBUS_LOCK_MASK ? ui::EF_CAPS_LOCK_DOWN : 0) |
         (state & IBUS_CONTROL_MASK ? ui::EF_CONTROL_DOWN : 0) |
         (state & IBUS_SHIFT_MASK ? ui::EF_SHIFT_DOWN : 0) |
         (state & IBUS_MOD1_MASK ? ui::EF_ALT_DOWN : 0) |
         (state & IBUS_BUTTON1_MASK ? ui::EF_LEFT_BUTTON_DOWN : 0) |
         (state & IBUS_BUTTON2_MASK ? ui::EF_MIDDLE_BUTTON_DOWN : 0) |
         (state & IBUS_BUTTON3_MASK ? ui::EF_RIGHT_BUTTON_DOWN : 0);
}

// Converts Views event flags to ibus key state flags.
guint32 IBusStateFromViewsFlags(int flags) {
  return (flags & ui::EF_CAPS_LOCK_DOWN ? IBUS_LOCK_MASK : 0) |
         (flags & ui::EF_CONTROL_DOWN ? IBUS_CONTROL_MASK : 0) |
         (flags & ui::EF_SHIFT_DOWN ? IBUS_SHIFT_MASK : 0) |
         (flags & ui::EF_ALT_DOWN ? IBUS_MOD1_MASK : 0) |
         (flags & ui::EF_LEFT_BUTTON_DOWN ? IBUS_BUTTON1_MASK : 0) |
         (flags & ui::EF_MIDDLE_BUTTON_DOWN ? IBUS_BUTTON2_MASK : 0) |
         (flags & ui::EF_RIGHT_BUTTON_DOWN ? IBUS_BUTTON3_MASK : 0);
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
    while(true) {
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

// A switch to enable InputMethodIBus
const char kEnableInputMethodIBusSwitch[] = "enable-inputmethod-ibus";

}  // namespace

namespace views {

// InputMethodIBus::PendingKeyEvent implementation ----------------------------
class InputMethodIBus::PendingKeyEvent {
 public:
  PendingKeyEvent(InputMethodIBus* input_method, const KeyEvent& key);
  ~PendingKeyEvent();

  // Abandon this pending key event. Its result will just be discarded.
  void abandon() { input_method_ = NULL; }

  InputMethodIBus* input_method() const { return input_method_; }

  // Process this pending key event after we receive its result from the input
  // method. It just call through InputMethodIBus::ProcessKeyEventPostIME().
  void ProcessPostIME(bool handled);

 private:
  InputMethodIBus* input_method_;

  // Complete information of a views::KeyEvent. Sadly, views::KeyEvent doesn't
  // support copy.
  ui::EventType type_;
  int flags_;
  ui::KeyboardCode key_code_;

#if defined(TOUCH_UI)
  // corresponding XEvent data of a views::KeyEvent. It's a plain struct so we
  // can do bitwise copy.
  XKeyEvent x_event_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PendingKeyEvent);
};

InputMethodIBus::PendingKeyEvent::PendingKeyEvent(InputMethodIBus* input_method,
                                                  const KeyEvent& key)
    : input_method_(input_method),
      type_(key.type()),
      flags_(key.flags()),
      key_code_(key.key_code()) {
  DCHECK(input_method_);
#if defined(TOUCH_UI)
  if (key.native_event_2())
    x_event_ = *reinterpret_cast<XKeyEvent*>(key.native_event_2());
  else
    memset(&x_event_, 0, sizeof(x_event_));
#endif
}

InputMethodIBus::PendingKeyEvent::~PendingKeyEvent() {
  if (input_method_)
    input_method_->FinishPendingKeyEvent(this);
}

void InputMethodIBus::PendingKeyEvent::ProcessPostIME(bool handled) {
  if (!input_method_)
    return;

#if defined(TOUCH_UI)
  if (x_event_.type == KeyPress || x_event_.type == KeyRelease) {
    Event::FromNativeEvent2 from_native;
    KeyEvent key(reinterpret_cast<XEvent*>(&x_event_), from_native);
    input_method_->ProcessKeyEventPostIME(key, handled);
    return;
  }
#endif
  KeyEvent key(type_, key_code_, flags_);
  input_method_->ProcessKeyEventPostIME(key, handled);
}

// InputMethodIBus::PendingCreateICRequest implementation ---------------------
class InputMethodIBus::PendingCreateICRequest {
 public:
  PendingCreateICRequest(InputMethodIBus* input_method,
                         PendingCreateICRequest** request_ptr,
                         bool fake);
  ~PendingCreateICRequest();

  // Abandon this pending key event. Its result will just be discarded.
  void abandon() {
    input_method_ = NULL;
    request_ptr_ = NULL;
  }

  // Stores the result input context to |input_method_|, or abandon it if
  // |input_method_| is NULL.
  void StoreOrAbandonInputContext(IBusInputContext* ic);

 private:
  InputMethodIBus* input_method_;
  PendingCreateICRequest** request_ptr_;
  bool fake_;
};

InputMethodIBus::PendingCreateICRequest::PendingCreateICRequest(
    InputMethodIBus* input_method,
    PendingCreateICRequest** request_ptr,
    bool fake)
    : input_method_(input_method),
      request_ptr_(request_ptr),
      fake_(fake) {
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
    input_method_->SetContext(ic, fake_);
  } else {
    // ibus_proxy_destroy() will not really release the object, we still need
    // to call g_object_unref() explicitly.
    ibus_proxy_destroy(reinterpret_cast<IBusProxy *>(ic));
    g_object_unref(ic);
  }
}

// InputMethodIBus implementation ---------------------------------------------
InputMethodIBus::InputMethodIBus(internal::InputMethodDelegate* delegate)
    : context_(NULL),
      fake_context_(NULL),
      pending_create_ic_request_(NULL),
      pending_create_fake_ic_request_(NULL),
      context_focused_(false),
      composing_text_(false),
      composition_changed_(false),
      suppress_next_result_(false) {
  set_delegate(delegate);
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
  DCHECK(!widget_focused());
  InputMethodBase::OnFocus();
  UpdateContextFocusState();
}

void InputMethodIBus::OnBlur() {
  DCHECK(widget_focused());
  ConfirmCompositionText();
  InputMethodBase::OnBlur();
  UpdateContextFocusState();
}

void InputMethodIBus::Init(Widget* widget) {
  InputMethodBase::Init(widget);

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
}

void InputMethodIBus::DispatchKeyEvent(const KeyEvent& key) {
  DCHECK(key.type() == ui::ET_KEY_PRESSED || key.type() == ui::ET_KEY_RELEASED);
  DCHECK(widget_focused());

  // If |context_| is not usable and |fake_context_| is not created yet, then we
  // can only dispatch the key event as is. We also dispatch the key event
  // directly if the current text input type is ui::TEXT_INPUT_TYPE_PASSWORD,
  // to bypass the input method.
  // Note: We need to send the key event to ibus even if the |context_| is not
  // enabled, so that ibus can have a chance to enable the |context_|.
  if (!(context_focused_ || fake_context_) ||
      GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD) {
    if (key.type() == ui::ET_KEY_PRESSED)
      ProcessUnfilteredKeyPressEvent(key);
    else
      DispatchKeyEventPostIME(key);
    return;
  }

  guint32 ibus_keyval = 0;
  guint32 ibus_keycode = 0;
  guint32 ibus_state = 0;
  PendingKeyEvent* pending_key =
      NewPendingKeyEvent(key, &ibus_keyval, &ibus_keycode, &ibus_state);

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
      context_focused_ ? context_ : fake_context_,
      ibus_keyval, ibus_keycode, ibus_state, -1, NULL,
      reinterpret_cast<GAsyncReadyCallback>(ProcessKeyEventDone),
      pending_key);

  // We don't want to suppress the result generated by this key event, but it
  // may cause problem. See comment in ResetContext() method.
  suppress_next_result_ = false;
}

void InputMethodIBus::OnTextInputTypeChanged(View* view) {
  if (context_ && IsViewFocused(view)) {
    ResetContext();
    UpdateContextFocusState();
  }
}

void InputMethodIBus::OnCaretBoundsChanged(View* view) {
  if (!context_focused_ || !IsViewFocused(view))
    return;

  // The current text input type should not be NONE if |context_| is focused.
  DCHECK(!IsTextInputTypeNone());

  gfx::Rect rect = GetTextInputClient()->GetCaretBounds();
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  // We need to convert the origin and end points separately, in case the View
  // is scaled.
  View::ConvertPointToScreen(view, &origin);
  View::ConvertPointToScreen(view, &end);

  // This function runs asynchronously.
  ibus_input_context_set_cursor_location(
      context_, origin.x(), origin.y(),
      end.x() - origin.x(), end.y() - origin.y());
}

void InputMethodIBus::CancelComposition(View* view) {
  if (context_focused_ && IsViewFocused(view))
    ResetContext();
}

std::string InputMethodIBus::GetInputLocale() {
  // Not supported.
  return std::string("");
}

base::i18n::TextDirection InputMethodIBus::GetInputTextDirection() {
  // Not supported.
  return base::i18n::UNKNOWN_DIRECTION;
}

bool InputMethodIBus::IsActive() {
  return context_ != NULL;
}

// static
bool InputMethodIBus::IsInputMethodIBusEnabled() {
#if defined(TOUCH_UI)
  return true;
#else
  return inputmethod_ibus_enabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableInputMethodIBusSwitch);
#endif
}

// static
void InputMethodIBus::SetEnableInputMethodIBus(bool enabled) {
  inputmethod_ibus_enabled = enabled;
}

void InputMethodIBus::FocusedViewWillChange() {
  ConfirmCompositionText();
}

void InputMethodIBus::FocusedViewDidChange() {
  UpdateContextFocusState();

  // Force to update caret bounds, in case the View thinks that the caret
  // bounds has not changed.
  if (context_focused_)
    OnCaretBoundsChanged(focused_view());
}

void InputMethodIBus::CreateContext() {
  DCHECK(!context_);
  DCHECK(!fake_context_);
  DCHECK(GetIBus());
  DCHECK(ibus_bus_is_connected(GetIBus()));
  DCHECK(!pending_create_ic_request_);
  DCHECK(!pending_create_fake_ic_request_);

  // Creates the input context asynchronously.
  pending_create_ic_request_ = new PendingCreateICRequest(
      this, &pending_create_ic_request_, false /* fake */);
  ibus_bus_create_input_context_async(
      GetIBus(), "chrome", -1, NULL,
      reinterpret_cast<GAsyncReadyCallback>(CreateInputContextDone),
      pending_create_ic_request_);

  // Creates the fake input context asynchronously. ibus will match the "fake"
  // prefix in the application name to do magic thing.
  pending_create_fake_ic_request_ = new PendingCreateICRequest(
      this, &pending_create_fake_ic_request_, true /* fake */);
  ibus_bus_create_input_context_async(
      GetIBus(), "fake-chrome", -1, NULL,
      reinterpret_cast<GAsyncReadyCallback>(CreateInputContextDone),
      pending_create_fake_ic_request_);
}

void InputMethodIBus::SetContext(IBusInputContext* ic, bool fake) {
  DCHECK(ic);
  if (fake) {
    DCHECK(!fake_context_);
    fake_context_ = ic;

    // We only need to care about "destroy" signal of the fake context,
    // as it will not generate any input method result.
    g_signal_connect(ic, "destroy", G_CALLBACK(OnFakeDestroyThunk), this);

    ibus_input_context_set_capabilities(ic, IBUS_CAP_FOCUS);
    UpdateFakeContextFocusState();
    return;
  }

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
    pending_create_ic_request_->abandon();
    pending_create_ic_request_ = NULL;
  } else if (context_) {
    // ibus_proxy_destroy() will not really release the resource of |context_|
    // object. We still need to handle "destroy" signal and call
    // g_object_unref() there.
    ibus_proxy_destroy(reinterpret_cast<IBusProxy *>(context_));
    DCHECK(!context_);
  }

  if (pending_create_fake_ic_request_) {
    DCHECK(!fake_context_);
    // |pending_create_fake_ic_request_| will be deleted in
    // CreateInputContextDone().
    pending_create_fake_ic_request_->abandon();
    pending_create_fake_ic_request_ = NULL;
  } else if (fake_context_) {
    ibus_proxy_destroy(reinterpret_cast<IBusProxy *>(fake_context_));
    DCHECK(!fake_context_);
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

  DCHECK(widget_focused());
  DCHECK(focused_view());

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

  // We don't need to reset |fake_context_|.
}

void InputMethodIBus::UpdateContextFocusState() {
  if (!context_) {
    context_focused_ = false;
    UpdateFakeContextFocusState();
    return;
  }

  const bool old_context_focused = context_focused_;
  // Use switch here in case we are going to add more text input types.
  switch (GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
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

  UpdateFakeContextFocusState();
}

void InputMethodIBus::UpdateFakeContextFocusState() {
  if (!fake_context_)
    return;

  if (widget_focused() && !context_focused_ &&
      GetTextInputType() != ui::TEXT_INPUT_TYPE_PASSWORD) {
    // We disable input method in password fields, so it makes no sense to allow
    // switching input method.
    ibus_input_context_focus_in(fake_context_);
  } else {
    ibus_input_context_focus_out(fake_context_);
  }
}

void InputMethodIBus::ProcessKeyEventPostIME(const KeyEvent& key,
                                             bool handled) {
  // If we get here without a focused text input client, then it means the key
  // event is sent to the global ibus input context.
  if (!GetTextInputClient()) {
    DispatchKeyEventPostIME(key);
    return;
  }

  const View* old_focused_view = focused_view();

  // Same reason as above DCHECK.
  DCHECK(old_focused_view);

  if (key.type() == ui::ET_KEY_PRESSED && handled)
    ProcessFilteredKeyPressEvent(key);

  // In case the focus was changed by the key event. The |context_| should have
  // been reset when the focused view changed.
  if (old_focused_view != focused_view())
    return;

  if (HasInputMethodResult())
    ProcessInputMethodResult(key, handled);

  // In case the focus was changed when sending input method results to the
  // focused View.
  if (old_focused_view != focused_view())
    return;

  if (key.type() == ui::ET_KEY_PRESSED && !handled)
    ProcessUnfilteredKeyPressEvent(key);
  else if (key.type() == ui::ET_KEY_RELEASED)
    DispatchKeyEventPostIME(key);
}

void InputMethodIBus::ProcessFilteredKeyPressEvent(const KeyEvent& key) {
  if (NeedInsertChar()) {
    DispatchKeyEventPostIME(key);
  } else {
    KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_PROCESSKEY, key.flags());
    DispatchKeyEventPostIME(key);
  }
}

void InputMethodIBus::ProcessUnfilteredKeyPressEvent(const KeyEvent& key) {
  const View* old_focused_view = focused_view();
  DispatchKeyEventPostIME(key);

  // We shouldn't dispatch the character anymore if the key event caused focus
  // change.
  if (old_focused_view != focused_view())
    return;

  // If a key event was not filtered by |context_|, then it means the key
  // event didn't generate any result text. So we need to send corresponding
  // character to the focused text input client.
  // TODO(suzhe): support compose and dead keys. Gtk supports it in
  // GtkIMContextSimple class. We need similar thing after getting rid of Gtk.
  char16 ch = key.GetCharacter();
  TextInputClient* client = GetTextInputClient();
  if (ch && client)
    client->InsertChar(ch, key.flags());
}

void InputMethodIBus::ProcessInputMethodResult(const KeyEvent& key,
                                              bool filtered) {
  TextInputClient* client = GetTextInputClient();
  DCHECK(client);

  if (result_text_.length()) {
    if (filtered && NeedInsertChar()) {
      for (string16::const_iterator i = result_text_.begin();
           i != result_text_.end(); ++i) {
        client->InsertChar(*i, key.flags());
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
  KeyEvent key(pressed ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED,
               ui::VKEY_PROCESSKEY, 0);
  DispatchKeyEventPostIME(key);
}

InputMethodIBus::PendingKeyEvent* InputMethodIBus::NewPendingKeyEvent(
    const KeyEvent& key,
    guint32* ibus_keyval,
    guint32* ibus_keycode,
    guint32* ibus_state) {
#if defined(TOUCH_UI)
  if (key.native_event_2()) {
    XKeyEvent* x_key = reinterpret_cast<XKeyEvent*>(key.native_event_2());
    // Yes, ibus uses X11 keysym. We cannot use XLookupKeysym(), which doesn't
    // translate Shift and CapsLock states.
    KeySym keysym = NoSymbol;
    ::XLookupString(x_key, NULL, 0, &keysym, NULL);
    *ibus_keyval = keysym;
    *ibus_keycode = x_key->keycode;
  }
#elif defined(TOOLKIT_USES_GTK)
  if (key.native_event()) {
    GdkEventKey* gdk_key = reinterpret_cast<GdkEventKey*>(key.native_event());
    *ibus_keyval = gdk_key->keyval;
    *ibus_keycode = gdk_key->hardware_keycode;
  }
#endif
  else {
    // GdkKeyCodeForWindowsKeyCode() is actually nothing to do with Gtk, we
    // probably want to rename it to something like XKeySymForWindowsKeyCode(),
    // because Gtk keyval is actually same as X KeySym.
    *ibus_keyval = ui::GdkKeyCodeForWindowsKeyCode(
        key.key_code(), key.IsShiftDown() ^ key.IsCapsLockDown());
    *ibus_keycode = 0;
  }

  *ibus_state = IBusStateFromViewsFlags(key.flags());
  if (key.type() == ui::ET_KEY_RELEASED)
    *ibus_state |= IBUS_RELEASE_MASK;

  PendingKeyEvent* pending_key = new PendingKeyEvent(this, key);
  pending_key_events_.insert(pending_key);
  return pending_key;
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
    (*i)->abandon();
  }
  pending_key_events_.clear();
}

void InputMethodIBus::OnCommitText(IBusInputContext* context, IBusText* text) {
  DCHECK_EQ(context_, context);
  if (suppress_next_result_ || !text || !text->text)
    return;

  // We need to receive input method result even if the text input type is
  // ui::TEXT_INPUT_TYPE_NONE, to make sure we can always send correct
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

  ui::KeyboardCode key_code = ui::VKEY_UNKNOWN;
#if defined(TOUCH_UI)
  key_code = ui::KeyboardCodeFromXKeysym(keyval);
#elif defined(TOOLKIT_USES_GTK)
  key_code = ui::WindowsKeyCodeForGdkKeyCode(keyval);
#endif

  if (!key_code)
    return;

  KeyEvent key(state & IBUS_RELEASE_MASK ?
               ui::ET_KEY_RELEASED : ui::ET_KEY_PRESSED,
               key_code, ViewsFlagsFromIBusState(state));

  // It is not clear when the input method will forward us a fake key event.
  // If there is a pending key event, then we may already received some input
  // method results, so we dispatch this fake key event directly rather than
  // calling ProcessKeyEventPostIME(), which will clear pending input method
  // results.
  if (key.type() == ui::ET_KEY_PRESSED)
    ProcessUnfilteredKeyPressEvent(key);
  else
    DispatchKeyEventPostIME(key);
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

void InputMethodIBus::OnFakeDestroy(IBusInputContext* context) {
  DCHECK_EQ(fake_context_, context);
  g_object_unref(fake_context_);
  fake_context_ = NULL;
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
         data->input_method()->context_ == context ||
         data->input_method()->fake_context_ == context);

  gboolean handled = FALSE;
  if (!ibus_input_context_process_key_event_async_finish(context, res,
                                                         &handled, NULL)) {
    handled = FALSE;
  }

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

}  // namespace views
