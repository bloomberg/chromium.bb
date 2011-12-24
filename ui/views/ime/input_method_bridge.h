// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_
#define UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/ime/input_method_base.h"

namespace ui {
class InputMethod;
}  // namespace ui

namespace views {

class View;

// A "bridge" InputMethod implementation for views top-level widgets, which just
// sends/receives IME related events to/from a system-wide ui::InputMethod
// object.
class InputMethodBridge : public InputMethodBase,
                          public ui::TextInputClient {
 public:
  InputMethodBridge(internal::InputMethodDelegate* delegate,
                    ui::InputMethod* host);
  virtual ~InputMethodBridge();

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

  // Overridden from TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE;
  virtual void ConfirmCompositionText() OVERRIDE;
  virtual void ClearCompositionText() OVERRIDE;
  virtual void InsertText(const string16& text) OVERRIDE;
  virtual void InsertChar(char16 ch, int flags) OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() OVERRIDE;
  virtual bool HasCompositionText() OVERRIDE;
  virtual bool GetTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetCompositionTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetSelectionRange(ui::Range* range) OVERRIDE;
  virtual bool SetSelectionRange(const ui::Range& range) OVERRIDE;
  virtual bool DeleteRange(const ui::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(
      const ui::Range& range, string16* text) OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;

  // Overridden from FocusChangeListener.
  virtual void OnWillChangeFocus(View* focused_before, View* focused) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before, View* focused) OVERRIDE;

 private:
  void UpdateViewFocusState();

  ui::InputMethod* const host_;
  bool context_focused_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBridge);
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_
