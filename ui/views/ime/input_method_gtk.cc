// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/input_method_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/utf_string_conversions.h"
#include "ui/base/gtk/event_synthesis_gtk.h"
#include "ui/base/gtk/gtk_im_context_util.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/widget.h"

namespace views {

InputMethodGtk::InputMethodGtk(internal::InputMethodDelegate* delegate)
    : context_(NULL),
      context_simple_(NULL),
      widget_realize_id_(0),
      widget_unrealize_id_(0),
      context_focused_(false),
      handling_key_event_(false),
      composing_text_(false),
      composition_changed_(false),
      suppress_next_result_(false) {
  set_delegate(delegate);
}

InputMethodGtk::~InputMethodGtk() {
  if (widget()) {
    GtkWidget* native_view = widget()->GetNativeView();
    if (native_view) {
      g_signal_handler_disconnect(native_view, widget_realize_id_);
      g_signal_handler_disconnect(native_view, widget_unrealize_id_);
    }
  }
  if (context_) {
    g_object_unref(context_);
    context_ = NULL;
  }
  if (context_simple_) {
    g_object_unref(context_simple_);
    context_simple_ = NULL;
  }
}

void InputMethodGtk::Init(Widget* widget) {
  DCHECK(GTK_IS_WIDGET(widget->GetNativeView()));

  widget_realize_id_ =
      g_signal_connect(widget->GetNativeView(), "realize",
                       G_CALLBACK(OnWidgetRealizeThunk), this);
  widget_unrealize_id_ =
      g_signal_connect(widget->GetNativeView(), "unrealize",
                       G_CALLBACK(OnWidgetUnrealizeThunk), this);

  context_ = gtk_im_multicontext_new();
  context_simple_ = gtk_im_context_simple_new();

  // context_ and context_simple_ share the same callback handlers.
  // All data come from them are treated equally.
  // context_ is for full input method support.
  // context_simple_ is for supporting dead/compose keys when input method is
  // disabled, eg. in password input box.
  g_signal_connect(context_, "commit",
                   G_CALLBACK(OnCommitThunk), this);
  g_signal_connect(context_, "preedit_start",
                   G_CALLBACK(OnPreeditStartThunk), this);
  g_signal_connect(context_, "preedit_end",
                   G_CALLBACK(OnPreeditEndThunk), this);
  g_signal_connect(context_, "preedit_changed",
                   G_CALLBACK(OnPreeditChangedThunk), this);

  g_signal_connect(context_simple_, "commit",
                   G_CALLBACK(OnCommitThunk), this);
  g_signal_connect(context_simple_, "preedit_start",
                   G_CALLBACK(OnPreeditStartThunk), this);
  g_signal_connect(context_simple_, "preedit_end",
                   G_CALLBACK(OnPreeditEndThunk), this);
  g_signal_connect(context_simple_, "preedit_changed",
                   G_CALLBACK(OnPreeditChangedThunk), this);

  // Set client window if the widget is already realized.
  OnWidgetRealize(widget->GetNativeView());

  InputMethodBase::Init(widget);
}

void InputMethodGtk::OnFocus() {
  DCHECK(!widget_focused());
  InputMethodBase::OnFocus();
  UpdateContextFocusState();
}

void InputMethodGtk::OnBlur() {
  DCHECK(widget_focused());
  ConfirmCompositionText();
  InputMethodBase::OnBlur();
  UpdateContextFocusState();
}

void InputMethodGtk::DispatchKeyEvent(const KeyEvent& key) {
  DCHECK(key.type() == ui::ET_KEY_PRESSED || key.type() == ui::ET_KEY_RELEASED);
  suppress_next_result_ = false;

  // We should bypass |context_| and |context_simple_| only if there is no
  // text input client focused. Otherwise, always send the key event to either
  // |context_| or |context_simple_| even if the text input type is
  // ui::TEXT_INPUT_TYPE_NONE, to make sure we can get correct character result.
  if (!GetTextInputClient()) {
    DispatchKeyEventPostIME(key);
    return;
  }

  handling_key_event_ = true;
  composition_changed_ = false;
  result_text_.clear();

  // If it's a fake key event, then we need to synthesize a GdkEventKey.
  GdkEvent* event = key.gdk_event() ? key.gdk_event() :
                                      SynthesizeGdkEventKey(key);
  gboolean filtered = gtk_im_context_filter_keypress(
      context_focused_ ? context_ : context_simple_, &event->key);

  handling_key_event_ = false;

  const View* old_focused_view = GetFocusedView();
  if (key.type() == ui::ET_KEY_PRESSED && filtered)
    ProcessFilteredKeyPressEvent(key);

  // Ensure no focus change from processing the key event.
  if (old_focused_view == GetFocusedView()) {
    if (HasInputMethodResult())
      ProcessInputMethodResult(key, filtered);
    // Ensure no focus change sending input method results to the focused View.
    if (old_focused_view == GetFocusedView()) {
      if (key.type() == ui::ET_KEY_PRESSED && !filtered)
        ProcessUnfilteredKeyPressEvent(key);
      else if (key.type() == ui::ET_KEY_RELEASED)
        DispatchKeyEventPostIME(key);
    }
  }

  // Free the synthesized event if there was no underlying native event.
  if (event != key.gdk_event())
    gdk_event_free(event);
}

void InputMethodGtk::OnTextInputTypeChanged(View* view) {
  if (IsViewFocused(view)) {
    DCHECK(!composing_text_);
    UpdateContextFocusState();
  }
  InputMethodBase::OnTextInputTypeChanged(view);
}

void InputMethodGtk::OnCaretBoundsChanged(View* view) {
  gfx::Rect rect;
  if (!IsViewFocused(view) || !GetCaretBoundsInWidget(&rect))
    return;

  GdkRectangle gdk_rect = rect.ToGdkRectangle();
  gtk_im_context_set_cursor_location(context_, &gdk_rect);
}

void InputMethodGtk::CancelComposition(View* view) {
  if (IsViewFocused(view))
    ResetContext();
}

std::string InputMethodGtk::GetInputLocale() {
  // Not supported.
  return std::string("");
}

base::i18n::TextDirection InputMethodGtk::GetInputTextDirection() {
  // Not supported.
  return base::i18n::UNKNOWN_DIRECTION;
}

bool InputMethodGtk::IsActive() {
  // We always need to send keyboard events to either |context_| or
  // |context_simple_|, so just return true here.
  return true;
}

void InputMethodGtk::OnWillChangeFocus(View* focused_before, View* focused) {
  ConfirmCompositionText();
}

void InputMethodGtk::OnDidChangeFocus(View* focused_before, View* focused) {
  UpdateContextFocusState();

  // Force to update caret bounds, in case the View thinks that the caret
  // bounds has not changed.
  if (context_focused_)
    OnCaretBoundsChanged(GetFocusedView());
}

void InputMethodGtk::ConfirmCompositionText() {
  ui::TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText();

  ResetContext();
}

void InputMethodGtk::ResetContext() {
  if (!GetTextInputClient())
    return;

  DCHECK(widget_focused());
  DCHECK(GetFocusedView());
  DCHECK(!handling_key_event_);

  // To prevent any text from being committed when resetting the |context_|;
  handling_key_event_ = true;
  suppress_next_result_ = true;

  gtk_im_context_reset(context_);
  gtk_im_context_reset(context_simple_);

  // Some input methods may not honour the reset call. Focusing out/in the
  // |context_| to make sure it gets reset correctly.
  if (context_focused_) {
    gtk_im_context_focus_out(context_);
    gtk_im_context_focus_in(context_);
  }

  composition_.Clear();
  result_text_.clear();
  handling_key_event_ = false;
  composing_text_ = false;
  composition_changed_ = false;
}

void InputMethodGtk::UpdateContextFocusState() {
  bool old_context_focused = context_focused_;
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
  if (old_context_focused && !context_focused_)
    gtk_im_context_focus_out(context_);
  else if (!old_context_focused && context_focused_)
    gtk_im_context_focus_in(context_);

  // |context_simple_| can be used in any textfield, including password box, and
  // even if the focused text input client's text input type is
  // ui::TEXT_INPUT_TYPE_NONE.
  if (GetTextInputClient())
    gtk_im_context_focus_in(context_simple_);
  else
    gtk_im_context_focus_out(context_simple_);
}

void InputMethodGtk::ProcessFilteredKeyPressEvent(const KeyEvent& key) {
  if (NeedInsertChar()) {
    DispatchKeyEventPostIME(key);
  } else {
    KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_PROCESSKEY, key.flags());
    DispatchKeyEventPostIME(key);
  }
}

void InputMethodGtk::ProcessUnfilteredKeyPressEvent(const KeyEvent& key) {
  const View* old_focused_view = GetFocusedView();
  DispatchKeyEventPostIME(key);

  // We shouldn't dispatch the character anymore if the key event caused focus
  // change.
  if (old_focused_view != GetFocusedView())
    return;

  // If a key event was not filtered by |context_| or |context_simple_|, then
  // it means the key event didn't generate any result text. For some cases,
  // the key event may still generate a valid character, eg. a control-key
  // event (ctrl-a, return, tab, etc.). We need to send the character to the
  // focused text input client by calling TextInputClient::InsertChar().
  char16 ch = key.GetCharacter();
  ui::TextInputClient* client = GetTextInputClient();
  if (ch && client)
    client->InsertChar(ch, key.flags());
}

void InputMethodGtk::ProcessInputMethodResult(const KeyEvent& key,
                                              bool filtered) {
  ui::TextInputClient* client = GetTextInputClient();
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
}

bool InputMethodGtk::NeedInsertChar() const {
  return IsTextInputTypeNone() ||
      (!composing_text_ && result_text_.length() == 1);
}

bool InputMethodGtk::HasInputMethodResult() const {
  return result_text_.length() || composition_changed_;
}

void InputMethodGtk::SendFakeProcessKeyEvent(bool pressed) const {
  KeyEvent key(pressed ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED,
               ui::VKEY_PROCESSKEY, 0);
  DispatchKeyEventPostIME(key);
}

GdkEvent* InputMethodGtk::SynthesizeGdkEventKey(const KeyEvent& key) const {
  guint keyval =
      ui::GdkKeyCodeForWindowsKeyCode(key.key_code(), key.IsShiftDown());
  guint state = 0;
  state |= key.IsShiftDown() ? GDK_SHIFT_MASK : 0;
  state |= key.IsControlDown() ? GDK_CONTROL_MASK : 0;
  state |= key.IsAltDown() ? GDK_MOD1_MASK : 0;
  state |= key.IsCapsLockDown() ? GDK_LOCK_MASK : 0;

  DCHECK(widget()->GetNativeView()->window);
  return ui::SynthesizeKeyEvent(widget()->GetNativeView()->window,
                                key.type() == ui::ET_KEY_PRESSED,
                                keyval, state);
}

void InputMethodGtk::OnCommit(GtkIMContext* context, gchar* text) {
  if (suppress_next_result_) {
    suppress_next_result_ = false;
    return;
  }

  // We need to receive input method result even if the text input type is
  // ui::TEXT_INPUT_TYPE_NONE, to make sure we can always send correct
  // character for each key event to the focused text input client.
  if (!GetTextInputClient())
    return;

  string16 utf16_text(UTF8ToUTF16(text));

  // Append the text to the buffer, because commit signal might be fired
  // multiple times when processing a key event.
  result_text_.append(utf16_text);

  // If we are not handling key event, do not bother sending text result if the
  // focused text input client does not support text input.
  if (!handling_key_event_ && !IsTextInputTypeNone()) {
    SendFakeProcessKeyEvent(true);
    GetTextInputClient()->InsertText(utf16_text);
    SendFakeProcessKeyEvent(false);
  }
}

void InputMethodGtk::OnPreeditStart(GtkIMContext* context) {
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  composing_text_ = true;
}

void InputMethodGtk::OnPreeditChanged(GtkIMContext* context) {
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  gchar* text = NULL;
  PangoAttrList* attrs = NULL;
  gint cursor_position = 0;
  gtk_im_context_get_preedit_string(context, &text, &attrs, &cursor_position);

  ui::ExtractCompositionTextFromGtkPreedit(text, attrs, cursor_position,
                                           &composition_);
  composition_changed_ = true;

  g_free(text);
  pango_attr_list_unref(attrs);

  if (composition_.text.length())
    composing_text_ = true;

  if (!handling_key_event_ && !IsTextInputTypeNone()) {
    SendFakeProcessKeyEvent(true);
    GetTextInputClient()->SetCompositionText(composition_);
    SendFakeProcessKeyEvent(false);
  }
}

void InputMethodGtk::OnPreeditEnd(GtkIMContext* context) {
  if (composition_.text.empty() || IsTextInputTypeNone())
    return;

  composition_changed_ = true;
  composition_.Clear();

  if (!handling_key_event_) {
    ui::TextInputClient* client = GetTextInputClient();
    if (client && client->HasCompositionText())
      client->ClearCompositionText();
  }
}

void InputMethodGtk::OnWidgetRealize(GtkWidget* widget) {
  // We should only set im context's client window once, because when setting
  // client window, im context may destroy and recreate its internal states and
  // objects.
  if (widget->window) {
    gtk_im_context_set_client_window(context_, widget->window);
    gtk_im_context_set_client_window(context_simple_, widget->window);
  }
}

void InputMethodGtk::OnWidgetUnrealize(GtkWidget* widget) {
  gtk_im_context_set_client_window(context_, NULL);
  gtk_im_context_set_client_window(context_simple_, NULL);
}

}  // namespace views
