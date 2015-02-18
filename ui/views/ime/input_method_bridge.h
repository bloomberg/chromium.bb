// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_
#define UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/geometry/rect.h"
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
  ~InputMethodBridge() override;

  // Overridden from InputMethod:
  void OnFocus() override;
  void OnBlur() override;
  bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                NativeEventResult* result) override;
  void DispatchKeyEvent(const ui::KeyEvent& key) override;
  void OnTextInputTypeChanged(View* view) override;
  void OnCaretBoundsChanged(View* view) override;
  void CancelComposition(View* view) override;
  void OnInputLocaleChanged() override;
  std::string GetInputLocale() override;
  bool IsActive() override;
  bool IsCandidatePopupOpen() const override;
  void ShowImeIfNeeded() override;

  // Overridden from TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(base::char16 ch, int flags) override;
  gfx::NativeWindow GetAttachedWindow() const override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32 index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretInRect(const gfx::Rect& rect) override;
  void OnCandidateWindowShown() override;
  void OnCandidateWindowUpdated() override;
  void OnCandidateWindowHidden() override;
  bool IsEditCommandEnabled(int command_id) override;
  void SetEditCommandForNextKeyEvent(int command_id) override;

  // Overridden from FocusChangeListener.
  void OnWillChangeFocus(View* focused_before, View* focused) override;
  void OnDidChangeFocus(View* focused_before, View* focused) override;

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
