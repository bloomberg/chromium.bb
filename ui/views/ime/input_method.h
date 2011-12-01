// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_H_
#define UI_VIEWS_IME_INPUT_METHOD_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/views/views_export.h"

namespace ui {
class TextInputClient;
}  // namespace ui

namespace views {

namespace internal {
class InputMethodDelegate;
}  // namespace internal

class KeyEvent;
class View;
class Widget;

// An interface implemented by an object that encapsulates a native input method
// service provided by the underlying operation system.
// Because on most systems, the system input method service is bound to
// individual native window. On Windows, its HWND, on Linux/Gtk, its GdkWindow.
// And in Views control system, only the top-level NativeWidget has a native
// window that can get keyboard focus. So this API is designed to be bound to
// the top-level NativeWidget.
class VIEWS_EXPORT InputMethod {
 public:
  virtual ~InputMethod() {}

  // Sets the delegate used by this InputMethod instance. It should only be
  // called by the internal NativeWidget or testing code.
  virtual void set_delegate(internal::InputMethodDelegate* delegate) = 0;

  // Initialize the InputMethod object and attach it to the given |widget|.
  // The |widget| must already be initialized.
  virtual void Init(Widget* widget) = 0;

  // Called when the top-level NativeWidget gets keyboard focus. It should only
  // be called by the top-level NativeWidget which owns this InputMethod
  // instance.
  virtual void OnFocus() = 0;

  // Called when the top-level NativeWidget loses keyboard focus. It should only
  // be called by the top-level NativeWidget which owns this InputMethod
  // instance.
  virtual void OnBlur() = 0;

  // Dispatch a key event to the input method. The key event will be dispatched
  // back to the caller via InputMethodDelegate::DispatchKeyEventPostIME(), once
  // it's processed by the input method. It should only be called by the
  // top-level NativeWidget which owns this InputMethod instance, or other
  // related platform dependent code, such as a message dispatcher.
  virtual void DispatchKeyEvent(const KeyEvent& key) = 0;

  // Called by the focused |view| whenever its text input type is changed.
  // Before calling this method, the focused |view| must confirm or clear
  // existing composition text and call InputMethod::CancelComposition() when
  // necessary. Otherwise unexpected behavior may happen. This method has no
  // effect if the |view| is not focused.
  virtual void OnTextInputTypeChanged(View* view) = 0;

  // Called by the focused |view| whenever its caret bounds is changed.
  // This method has no effect if the |view| is not focused.
  virtual void OnCaretBoundsChanged(View* view) = 0;

  // Called by the focused |view| to ask the input method cancel the ongoing
  // composition session. This method has no effect if the |view| is not
  // focused.
  virtual void CancelComposition(View* view) = 0;

  // Returns the locale of current keyboard layout or input method, as a BCP-47
  // tag, or an empty string if the input method cannot provide it.
  virtual std::string GetInputLocale() = 0;

  // Returns the text direction of current keyboard layout or input method, or
  // base::i18n::UNKNOWN_DIRECTION if the input method cannot provide it.
  virtual base::i18n::TextDirection GetInputTextDirection() = 0;

  // Checks if the input method is active, i.e. if it's ready for processing
  // keyboard event and generate composition or text result.
  // If the input method is inactive, then it's not necessary to inform it the
  // changes of caret bounds and text input type.
  // Note: character results may still be generated and sent to the text input
  // client by calling TextInputClient::InsertChar(), even if the input method
  // is not active.
  virtual bool IsActive() = 0;

  // Gets the focused text input client. Returns NULL if the Widget is not
  // focused, or there is no focused View or the focused View doesn't support
  // text input.
  virtual ui::TextInputClient* GetTextInputClient() const = 0;

  // Gets the text input type of the focused text input client. Returns
  // ui::TEXT_INPUT_TYPE_NONE if there is no focused text input client.
  virtual ui::TextInputType GetTextInputType() const = 0;

  // Returns true if the input method is a mock and not real.
  virtual bool IsMock() const = 0;

  // TODO(suzhe): Support mouse/touch event.
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_H_
