// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_BASE_H_
#define UI_VIEWS_IME_INPUT_METHOD_BASE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/ime/input_method_delegate.h"
#include "views/focus/focus_manager.h"

namespace gfx {
class Rect;
}

namespace ui {
class TextInputClient;
}

namespace views {

class View;

// A helper class providing functionalities shared among InputMethod
// implementations.
class VIEWS_EXPORT InputMethodBase : public InputMethod,
                                     public FocusChangeListener {
 public:
  InputMethodBase();
  virtual ~InputMethodBase();

  // Overriden from InputMethod.
  virtual void set_delegate(internal::InputMethodDelegate* delegate) OVERRIDE;

  // If a derived class overrides this method, it should call parent's
  // implementation first.
  virtual void Init(Widget* widget) OVERRIDE;

  // If a derived class overrides this method, it should call parent's
  // implementation first, to make sure |widget_focused_| flag can be updated
  // correctly.
  virtual void OnFocus() OVERRIDE;

  // If a derived class overrides this method, it should call parent's
  // implementation first, to make sure |widget_focused_| flag can be updated
  // correctly.
  virtual void OnBlur() OVERRIDE;

  // If a derived class overrides this method, it should call parent's
  // implementation.
  virtual void OnTextInputTypeChanged(View* view) OVERRIDE;

  virtual ui::TextInputClient* GetTextInputClient() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual bool IsMock() const OVERRIDE;

  // Overridden from FocusChangeListener.
  virtual void OnWillChangeFocus(View* focused_before, View* focused) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before, View* focused) OVERRIDE;

 protected:
  // Getters and setters of properties.
  internal::InputMethodDelegate* delegate() const { return delegate_; }
  Widget* widget() const { return widget_; }
  bool widget_focused() const { return widget_focused_; }
  View* GetFocusedView() const;

  // Checks if the given View is focused. Returns true only if the View and
  // the Widget are both focused.
  bool IsViewFocused(View* view) const;

  // Checks if the focused text input client's text input type is
  // ui::TEXT_INPUT_TYPE_NONE. Also returns true if there is no focused text
  // input client.
  bool IsTextInputTypeNone() const;

  // Convenience method to call the focused text input client's
  // OnInputMethodChanged() method. It'll only take effect if the current text
  // input type is not ui::TEXT_INPUT_TYPE_NONE.
  void OnInputMethodChanged() const;

  // Convenience method to call delegate_->DispatchKeyEventPostIME().
  void DispatchKeyEventPostIME(const KeyEvent& key) const;

  // Gets the current text input client's caret bounds in Widget's coordinates.
  // Returns false if the current text input client doesn't support text input.
  bool GetCaretBoundsInWidget(gfx::Rect* rect) const;

 private:
  internal::InputMethodDelegate* delegate_;
  Widget* widget_;

  // Indicates if the top-level widget is focused or not.
  bool widget_focused_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBase);
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_BASE_H_
