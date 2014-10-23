// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_
#define UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/rect.h"
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
  // |shared_input_method| indicates if |host| is shared among other top level
  // widgets.
  InputMethodBridge(internal::InputMethodDelegate* delegate,
                    ui::InputMethod* host,
                    bool shared_input_method);
  virtual ~InputMethodBridge();

  // Overridden from InputMethod:
  virtual void OnFocus() override;
  virtual void OnBlur() override;
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) override;
  virtual void DispatchKeyEvent(const ui::KeyEvent& key) override;
  virtual void OnTextInputTypeChanged(View* view) override;
  virtual void OnCaretBoundsChanged(View* view) override;
  virtual void CancelComposition(View* view) override;
  virtual void OnInputLocaleChanged() override;
  virtual std::string GetInputLocale() override;
  virtual bool IsActive() override;
  virtual bool IsCandidatePopupOpen() const override;
  virtual void ShowImeIfNeeded() override;

  // Overridden from TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) override;
  virtual void ConfirmCompositionText() override;
  virtual void ClearCompositionText() override;
  virtual void InsertText(const base::string16& text) override;
  virtual void InsertChar(base::char16 ch, int flags) override;
  virtual gfx::NativeWindow GetAttachedWindow() const override;
  virtual ui::TextInputType GetTextInputType() const override;
  virtual ui::TextInputMode GetTextInputMode() const override;
  virtual int GetTextInputFlags() const override;
  virtual bool CanComposeInline() const override;
  virtual gfx::Rect GetCaretBounds() const override;
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) const override;
  virtual bool HasCompositionText() const override;
  virtual bool GetTextRange(gfx::Range* range) const override;
  virtual bool GetCompositionTextRange(gfx::Range* range) const override;
  virtual bool GetSelectionRange(gfx::Range* range) const override;
  virtual bool SetSelectionRange(const gfx::Range& range) override;
  virtual bool DeleteRange(const gfx::Range& range) override;
  virtual bool GetTextFromRange(const gfx::Range& range,
                                base::string16* text) const override;
  virtual void OnInputMethodChanged() override;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) override;
  virtual void EnsureCaretInRect(const gfx::Rect& rect) override;
  virtual void OnCandidateWindowShown() override;
  virtual void OnCandidateWindowUpdated() override;
  virtual void OnCandidateWindowHidden() override;
  virtual bool IsEditingCommandEnabled(int command_id) override;
  virtual void ExecuteEditingCommand(int command_id) override;

  // Overridden from FocusChangeListener.
  virtual void OnWillChangeFocus(View* focused_before, View* focused) override;
  virtual void OnDidChangeFocus(View* focused_before, View* focused) override;

  ui::InputMethod* GetHostInputMethod() const;

 private:
  class HostObserver;

  void UpdateViewFocusState();

  ui::InputMethod* host_;

  // An observer observing the host input method for cases that the host input
  // method is destroyed before this bridge input method.
  scoped_ptr<HostObserver> host_observer_;

  const bool shared_input_method_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBridge);
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_
