// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_AURALINUX_H_
#define UI_BASE_IME_INPUT_METHOD_AURALINUX_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/linux/linux_input_method_context.h"

namespace ui {

// A ui::InputMethod implementation for Aura on Linux platforms. The
// implementation details are separated to ui::LinuxInputMethodContext
// interface.
class UI_BASE_IME_EXPORT InputMethodAuraLinux
    : public InputMethodBase,
      public LinuxInputMethodContextDelegate {
 public:
  explicit InputMethodAuraLinux(internal::InputMethodDelegate* delegate);
  ~InputMethodAuraLinux() override;

  LinuxInputMethodContext* GetContextForTesting(bool is_simple);

  // Overriden from InputMethod.
  bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                NativeEventResult* result) override;
  void DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  void OnInputLocaleChanged() override;
  std::string GetInputLocale() override;
  bool IsCandidatePopupOpen() const override;
  void OnFocus() override;
  void OnBlur() override;

  // Overriden from ui::LinuxInputMethodContextDelegate
  void OnCommit(const base::string16& text) override;
  void OnPreeditChanged(const CompositionText& composition_text) override;
  void OnPreeditEnd() override;
  void OnPreeditStart() override{};

 protected:
  // Overridden from InputMethodBase.
  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) override;
  void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                TextInputClient* focused) override;

 private:
  bool HasInputMethodResult();
  bool NeedInsertChar() const;
  ui::EventDispatchDetails SendFakeProcessKeyEvent(ui::KeyEvent* event) const;
  void ConfirmCompositionText();
  void UpdateContextFocusState();
  void ResetContext();

  scoped_ptr<LinuxInputMethodContext> context_;
  scoped_ptr<LinuxInputMethodContext> context_simple_;

  base::string16 result_text_;

  ui::CompositionText composition_;

  // The current text input type used to indicates if |context_| and
  // |context_simple_| are focused or not.
  TextInputType text_input_type_;

  // Indicates if currently in sync mode when handling a key event.
  // This is used in OnXXX callbacks from GTK IM module.
  bool is_sync_mode_;

  // Indicates if the composition text is changed or deleted.
  bool composition_changed_;

  // If it's true then all input method result received before the next key
  // event will be discarded.
  bool suppress_next_result_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodAuraLinux);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_AURALINUX_H_
