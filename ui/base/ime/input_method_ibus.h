// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_IBUS_H_
#define UI_BASE_IME_INPUT_METHOD_IBUS_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/ime/character_composer.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ibus_client.h"
#include "ui/base/ime/input_method_base.h"

namespace dbus {
class ObjectPath;
}
namespace chromeos {
namespace ibus {
class IBusText;
}  // namespace ibus
}  // namespace chromeos

namespace ui {

// A ui::InputMethod implementation based on IBus.
class UI_EXPORT InputMethodIBus : public InputMethodBase {
 public:
  explicit InputMethodIBus(internal::InputMethodDelegate* delegate);
  virtual ~InputMethodIBus();

  // Overridden from InputMethod:
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void Init(bool focused) OVERRIDE;
  virtual void DispatchKeyEvent(
      const base::NativeEvent& native_key_event) OVERRIDE;
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE;
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;

  // Called when the connection with ibus-daemon is established.
  virtual void OnConnected();

  // Called when the connection with ibus-daemon is shutdowned.
  virtual void OnDisconnected();

  // Sets |new_client| as a new IBusClient. InputMethodIBus owns the object.
  // A client has to be set before InputMethodIBus::Init() is called.
  void set_ibus_client(scoped_ptr<internal::IBusClient> new_client);

  // The caller is not allowed to delete the object.
  internal::IBusClient* ibus_client() const;

 protected:
  // Converts |text| into CompositionText.
  void ExtractCompositionText(const chromeos::ibus::IBusText& text,
                              uint32 cursor_position,
                              CompositionText* out_composition) const;

 private:
  class PendingKeyEvent;
  class PendingCreateICRequest;

  // Overridden from InputMethodBase:
  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) OVERRIDE;
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) OVERRIDE;

  // Creates context asynchronously.
  void CreateContext();

  // Sets necessary signal handlers.
  void SetUpSignalHandlers();

  // Destroys context.
  void DestroyContext();

  // Asks the client to confirm current composition text.
  void ConfirmCompositionText();

  // Resets context and abandon all pending results and key events.
  void ResetContext();

  // Checks the availability of focused text input client and update focus
  // state.
  void UpdateContextFocusState();

  // Process a key returned from the input method.
  void ProcessKeyEventPostIME(const base::NativeEvent& native_key_event,
                              uint32 ibus_keycode,
                              bool handled);

  // Processes a key event that was already filtered by the input method.
  // A VKEY_PROCESSKEY may be dispatched to the focused View.
  void ProcessFilteredKeyPressEvent(const base::NativeEvent& native_key_event);

  // Processes a key event that was not filtered by the input method.
  void ProcessUnfilteredKeyPressEvent(const base::NativeEvent& native_key_event,
                                      uint32 ibus_keycode);
  void ProcessUnfilteredFabricatedKeyPressEvent(EventType type,
                                                KeyboardCode key_code,
                                                int flags,
                                                uint32 ibus_keyval);

  // Processes an unfiltered key press event with character composer.
  // This method returns true if the key press is filtered by the composer.
  bool ProcessUnfilteredKeyPressEventWithCharacterComposer(uint32 ibus_keyval,
                                                           uint32 state);

  // Sends input method result caused by the given key event to the focused text
  // input client.
  void ProcessInputMethodResult(const base::NativeEvent& native_key_event,
                                bool filtered);

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

  // Releases context focus and confirms the composition text. Then destroy
  // object proxy.
  void ResetInputContext();

  // Returns true if the connection to ibus-daemon is established.
  bool IsConnected();

  // Returns true if the input context is ready to use.
  bool IsContextReady();

  // Event handlers for IBusInputContext:
  void OnCommitText(const chromeos::ibus::IBusText& text);
  void OnForwardKeyEvent(uint32 keyval, uint32 keycode, uint32 status);
  void OnShowPreeditText();
  void OnUpdatePreeditText(const chromeos::ibus::IBusText& text,
                           uint32 cursor_pos,
                           bool visible);
  void OnHidePreeditText();

  void CreateInputContextDone(PendingCreateICRequest* ic_request,
                              const dbus::ObjectPath& object_path);
  void CreateInputContextFail(PendingCreateICRequest* ic_request);
  static void ProcessKeyEventDone(PendingKeyEvent* pending_key_event,
                                  bool is_handled);

  scoped_ptr<internal::IBusClient> ibus_client_;

  // All pending key events. Note: we do not own these object, we just save
  // pointers to these object so that we can abandon them when necessary.
  // They will be deleted in ProcessKeyEventDone().
  std::set<PendingKeyEvent*> pending_key_events_;

  // The pending request for creating the input context. We need to keep this
  // pointer so that we can receive or abandon the result.
  PendingCreateICRequest* pending_create_ic_request_;

  // Pending composition text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  CompositionText composition_;

  // Pending result text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  string16 result_text_;

  // Indicates if input context is focused or not.
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
  CharacterComposer character_composer_;

  // Used for making callbacks.
  base::WeakPtrFactory<InputMethodIBus> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodIBus);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_IBUS_H_
