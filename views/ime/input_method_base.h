// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_IME_INPUT_METHOD_BASE_H_
#define VIEWS_IME_INPUT_METHOD_BASE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/focus/focus_manager.h"
#include "views/ime/input_method.h"
#include "views/ime/input_method_delegate.h"
#include "views/ime/text_input_client.h"

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

  virtual TextInputClient* GetTextInputClient() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual bool IsMock() const OVERRIDE;

  // Overridden from FocusChangeListener. Derived classes shouldn't override
  // this method. Override FocusedViewWillChange() and FocusedViewDidChange()
  // instead.
  virtual void FocusWillChange(View* focused_before, View* focused) OVERRIDE;

 protected:
  // Getters and setters of properties.
  internal::InputMethodDelegate* delegate() const { return delegate_; }
  Widget* widget() const { return widget_; }
  View* focused_view() const { return focused_view_; }
  bool widget_focused() const { return widget_focused_; }

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

  // Called just before changing the focused view. Should be overridden by
  // derived classes. The default implementation does nothing.
  virtual void FocusedViewWillChange();

  // Called just after changing the focused view. Should be overridden by
  // derived classes. The default implementation does nothing.
  // Note: It's called just after changing the value of |focused_view_|. As it's
  // called inside FocusChangeListener's FocusWillChange() method, which is
  // called by the FocusManager before actually changing the focus, the derived
  // class should not rely on the actual focus state of the |focused_view_|.
  virtual void FocusedViewDidChange();

 private:
  internal::InputMethodDelegate* delegate_;
  Widget* widget_;
  View* focused_view_;

  // Indicates if the top-level widget is focused or not.
  bool widget_focused_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBase);
};

}  // namespace views

#endif  // VIEWS_IME_INPUT_METHOD_BASE_H_
