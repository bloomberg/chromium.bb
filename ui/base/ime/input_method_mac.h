// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_MAC_H_
#define UI_BASE_IME_INPUT_METHOD_MAC_H_

#include "ui/base/ime/input_method_base.h"

namespace ui {

// A ui::InputMethod implementation for Mac.
// On the Mac, key events don't pass through InputMethod.
// Instead, NSTextInputClient calls are bridged to the currently focused
// ui::TextInputClient object.
class UI_BASE_EXPORT InputMethodMac : public InputMethodBase {
 public:
  explicit InputMethodMac(internal::InputMethodDelegate* delegate);
  virtual ~InputMethodMac();

  // Overriden from InputMethod.
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) override;
  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) override;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) override;
  virtual void CancelComposition(const TextInputClient* client) override;
  virtual void OnInputLocaleChanged() override;
  virtual std::string GetInputLocale() override;
  virtual bool IsActive() override;
  virtual bool IsCandidatePopupOpen() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodMac);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_MAC_H_
