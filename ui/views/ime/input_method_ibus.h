// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_IBUS_H_
#define UI_VIEWS_IME_INPUT_METHOD_IBUS_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/ime/character_composer.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/events/event.h"
#include "ui/views/ime/input_method_base.h"
#include "views/view.h"

// Forward declarations, so that we don't need to include ibus.h in this file.
typedef struct _GAsyncResult GAsyncResult;
typedef struct _IBusBus IBusBus;
typedef struct _IBusInputContext IBusInputContext;
typedef struct _IBusText IBusText;

namespace views {

// An InputMethod implementation based on IBus.
class InputMethodIBus : public InputMethodBase {
 public:
  explicit InputMethodIBus(internal::InputMethodDelegate* delegate);
  virtual ~InputMethodIBus();

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

  // Returns true when
  // 1) built with GYP_DEFINES="touchui=1" or,
  // 2) enabled by SetEnabledInputMethodIBus or,
  // 3) enabled by command line flag "--enable-inputmethod-ibus"
  static bool IsInputMethodIBusEnabled();
  // Enable/Disable InputMethodIBus
  static void SetEnableInputMethodIBus(bool enabled);

 private:
  // A class to hold all data related to a key event being processed by the
  // input method but still has no result back yet.
  class PendingKeyEvent;

  // A class to hold information of a pending request for creating an ibus input
  // context.
  class PendingCreateICRequest;

  // Overridden from InputMethodBase:
  virtual void OnWillChangeFocus(View* focused_before, View* focused) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before, View* focused) OVERRIDE;

  // Creates |context_| instance asynchronously.
  void CreateContext();

  // Sets |context_| and hooks necessary signals.
  void SetContext(IBusInputContext* ic);

  // Destroys |context_| instance.
  void DestroyContext();

  // Asks the client to confirm current composition text.
  void ConfirmCompositionText();

  // Resets |context_| and abandon all pending results and key events.
  void ResetContext();

  // Checks the availability of focused text input client and update focus state
  // of |context_| and |context_simple_| accordingly.
  void UpdateContextFocusState();

  // Process a key returned from the input method.
  void ProcessKeyEventPostIME(const KeyEvent& key, guint32 ibus_keycode,
                              bool handled);

  // Processes a key event that was already filtered by the input method.
  // A VKEY_PROCESSKEY may be dispatched to the focused View.
  void ProcessFilteredKeyPressEvent(const KeyEvent& key);

  // Processes a key event that was not filtered by the input method.
  void ProcessUnfilteredKeyPressEvent(const KeyEvent& key,
                                      guint32 ibus_keycode);

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

  // Called when a pending key event has finished. The event will be removed
  // from |pending_key_events_|.
  void FinishPendingKeyEvent(PendingKeyEvent* pending_key);

  // Abandons all pending key events. It usually happends when we lose keyboard
  // focus, the text input type is changed or we are destroyed.
  void AbandonAllPendingKeyEvents();

  // Event handlers for IBusInputContext:
  CHROMEG_CALLBACK_1(InputMethodIBus, void, OnCommitText,
                     IBusInputContext*, IBusText*);
  CHROMEG_CALLBACK_3(InputMethodIBus, void, OnForwardKeyEvent,
                     IBusInputContext*, guint, guint, guint);
  CHROMEG_CALLBACK_0(InputMethodIBus, void, OnShowPreeditText,
                     IBusInputContext*);
  CHROMEG_CALLBACK_3(InputMethodIBus, void, OnUpdatePreeditText,
                     IBusInputContext*, IBusText*, guint, gboolean);
  CHROMEG_CALLBACK_0(InputMethodIBus, void, OnHidePreeditText,
                     IBusInputContext*);
  CHROMEG_CALLBACK_0(InputMethodIBus, void, OnDestroy, IBusInputContext*);

  // Event handlers for IBusBus:
  CHROMEG_CALLBACK_0(InputMethodIBus, void, OnIBusConnected, IBusBus*);
  CHROMEG_CALLBACK_0(InputMethodIBus, void, OnIBusDisconnected, IBusBus*);

  // Returns the global IBusBus instance.
  static IBusBus* GetIBus();

  // Callback function for ibus_input_context_process_key_event_async().
  static void ProcessKeyEventDone(IBusInputContext* context,
                                  GAsyncResult* res,
                                  PendingKeyEvent* data);

  // Callback function for ibus_bus_create_input_context_async().
  static void CreateInputContextDone(IBusBus* bus,
                                     GAsyncResult* res,
                                     PendingCreateICRequest* data);

  // The input context for actual text input.
  IBusInputContext* context_;

  // All pending key events. Note: we do not own these object, we just save
  // pointers to these object so that we can abandon them when necessary.
  // They will be deleted in ProcessKeyEventDone().
  std::set<PendingKeyEvent*> pending_key_events_;

  // The pending request for creating the |context_| instance. We need to keep
  // this pointer so that we can receive or abandon the result.
  PendingCreateICRequest* pending_create_ic_request_;

  // Pending composition text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  ui::CompositionText composition_;

  // Pending result text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  string16 result_text_;

  // Indicates if |context_| and |context_simple_| are focused or not.
  bool context_focused_;

  // Indicates if there is an ongoing composition text.
  bool composing_text_;

  // Indicates if the composition text is changed or deleted.
  bool composition_changed_;

  // If it's true then all input method result received before the next key
  // event will be discarded.
  bool suppress_next_result_;

  // An object to compose a character from a sequence of key presses
  // including dead key etc.
  ui::CharacterComposer character_composer_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodIBus);
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_IBUS_H_
