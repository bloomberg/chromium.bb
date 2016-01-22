// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_BASE_H_
#define UI_BASE_IME_INPUT_METHOD_BASE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/ui_base_ime_export.h"
#include "ui/events/event_dispatcher.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class InputMethodObserver;
class KeyEvent;
class TextInputClient;

// A helper class providing functionalities shared among ui::InputMethod
// implementations.
class UI_BASE_IME_EXPORT InputMethodBase
    : NON_EXPORTED_BASE(public InputMethod),
      public base::SupportsWeakPtr<InputMethodBase> {
 public:
  InputMethodBase();
  ~InputMethodBase() override;

  // Overriden from InputMethod.
  void SetDelegate(internal::InputMethodDelegate* delegate) override;
  void OnFocus() override;
  void OnBlur() override;
  void SetFocusedTextInputClient(TextInputClient* client) override;
  void DetachTextInputClient(TextInputClient* client) override;
  TextInputClient* GetTextInputClient() const override;

  // If a derived class overrides this method, it should call parent's
  // implementation.
  void OnTextInputTypeChanged(const TextInputClient* client) override;

  TextInputType GetTextInputType() const override;
  TextInputMode GetTextInputMode() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  void ShowImeIfNeeded() override;

  void AddObserver(InputMethodObserver* observer) override;
  void RemoveObserver(InputMethodObserver* observer) override;

 protected:
  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) {}
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) {}

  // Returns true if |client| is currently focused.
  bool IsTextInputClientFocused(const TextInputClient* client);

  // Checks if the focused text input client's text input type is
  // TEXT_INPUT_TYPE_NONE. Also returns true if there is no focused text
  // input client.
  bool IsTextInputTypeNone() const;

  // Convenience method to call the focused text input client's
  // OnInputMethodChanged() method. It'll only take effect if the current text
  // input type is not TEXT_INPUT_TYPE_NONE.
  void OnInputMethodChanged() const;

  // Convenience method to call delegate_->DispatchKeyEventPostIME().
  // Returns true if the event was processed
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) const;

  // Convenience method to notify all observers of TextInputClient changes.
  void NotifyTextInputStateChanged(const TextInputClient* client);

  // Convenience method to notify all observers of CaretBounds changes on
  // |client| which is the text input client with focus.
  void NotifyTextInputCaretBoundsChanged(const TextInputClient* client);

 private:
  void SetFocusedTextInputClientInternal(TextInputClient* client);

  internal::InputMethodDelegate* delegate_;
  TextInputClient* text_input_client_;

  base::ObserverList<InputMethodObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBase);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_BASE_H_
