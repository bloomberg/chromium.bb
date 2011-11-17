// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_GTK_H_
#define UI_VIEWS_IME_INPUT_METHOD_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/ime/input_method_base.h"
#include "views/view.h"

namespace views {

// An InputMethod implementation based on GtkIMContext. Most code are copied
// from chrome/browser/renderer_host/gtk_im_context_wrapper.*
// It's intended for testing purpose.
class InputMethodGtk : public InputMethodBase {
 public:
  explicit InputMethodGtk(internal::InputMethodDelegate* delegate);
  virtual ~InputMethodGtk();

  // Overridden from InputMethod:
  virtual void Init(Widget* widget) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void DispatchKeyEvent(const KeyEvent& key) OVERRIDE;
  virtual void OnTextInputTypeChanged(View* view) OVERRIDE;
  virtual void OnCaretBoundsChanged(View* view) OVERRIDE;
  virtual void CancelComposition(View* view) OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;

 private:
  // Overridden from InputMethodBase:
  virtual void OnWillChangeFocus(View* focused_before, View* focused) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before, View* focused) OVERRIDE;

  // Asks the client to confirm current composition text.
  void ConfirmCompositionText();

  // Resets |context_| and |context_simple_|.
  void ResetContext();

  // Checks the availability of focused text input client and update focus state
  // of |context_| and |context_simple_| accordingly.
  void UpdateContextFocusState();

  // Processes a key event that was already filtered by the input method.
  // A VKEY_PROCESSKEY may be dispatched to the focused View.
  void ProcessFilteredKeyPressEvent(const KeyEvent& key);

  // Processes a key event that was not filtered by the input method.
  void ProcessUnfilteredKeyPressEvent(const KeyEvent& key);

  // Sends input method result caused by the given key event to the focused text
  // input client.
  void ProcessInputMethodResult(const KeyEvent& key, bool filtered);

  // Checks if the pending input method result needs inserting into the focused
  // text input client as a single character.
  bool NeedInsertChar() const;

  // Checks if there is pending input method result.
  bool HasInputMethodResult() const;

  // Fabricates a key event with VKEY_PROCESSKEY key code and dispatches it to
  // the focused View.
  void SendFakeProcessKeyEvent(bool pressed) const;

  // Synthesize a GdkEventKey based on given key event. The returned GdkEventKey
  // must be freed with gdk_event_free().
  GdkEvent* SynthesizeGdkEventKey(const KeyEvent& key) const;

  // Event handlers:
  CHROMEG_CALLBACK_1(InputMethodGtk, void, OnCommit, GtkIMContext*, gchar*);
  CHROMEG_CALLBACK_0(InputMethodGtk, void, OnPreeditStart, GtkIMContext*);
  CHROMEG_CALLBACK_0(InputMethodGtk, void, OnPreeditChanged, GtkIMContext*);
  CHROMEG_CALLBACK_0(InputMethodGtk, void, OnPreeditEnd, GtkIMContext*);

  CHROMEGTK_CALLBACK_0(InputMethodGtk, void, OnWidgetRealize);
  CHROMEGTK_CALLBACK_0(InputMethodGtk, void, OnWidgetUnrealize);

  GtkIMContext* context_;
  GtkIMContext* context_simple_;

  ui::CompositionText composition_;

  string16 result_text_;

  // Signal ids for corresponding gtk signals.
  gulong widget_realize_id_;
  gulong widget_unrealize_id_;

  // Indicates if |context_| and |context_simple_| are focused or not.
  bool context_focused_;

  // Indicates if we are handling a key event.
  bool handling_key_event_;

  // Indicates if there is an ongoing composition text.
  bool composing_text_;

  // Indicates if the composition text is changed or deleted.
  bool composition_changed_;

  // If it's true then all input method result received before the next key
  // event will be discarded.
  bool suppress_next_result_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodGtk);
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_GTK_H_
