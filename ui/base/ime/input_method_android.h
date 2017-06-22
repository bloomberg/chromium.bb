// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_ANDROID_H_
#define UI_BASE_IME_INPUT_METHOD_ANDROID_H_

#include "base/macros.h"
#include "ui/base/ime/input_method_base.h"

namespace ui {

// A ui::InputMethod implementation for Aura on Android platform.
class UI_BASE_IME_EXPORT InputMethodAndroid : public InputMethodBase {
 public:
  explicit InputMethodAndroid(internal::InputMethodDelegate* delegate);
  ~InputMethodAndroid() override;

 private:
  // Overriden from InputMethod.
  bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                NativeEventResult* result) override;
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;

  DISALLOW_COPY_AND_ASSIGN(InputMethodAndroid);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_ANDROID_H_

