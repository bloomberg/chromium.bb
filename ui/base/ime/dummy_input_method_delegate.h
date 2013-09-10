// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_DUMMY_INPUT_METHOD_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ui_export.h"

namespace ui {
namespace internal {

class UI_EXPORT DummyInputMethodDelegate
    : NON_EXPORTED_BASE(public InputMethodDelegate) {
 public:
  DummyInputMethodDelegate();
  virtual ~DummyInputMethodDelegate();

  // InputMethodDelegate overrides:
  virtual bool DispatchKeyEventPostIME(
      const base::NativeEvent& native_key_event) OVERRIDE;
  virtual bool DispatchFabricatedKeyEventPostIME(ui::EventType type,
                                                 ui::KeyboardCode key_code,
                                                 int flags) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyInputMethodDelegate);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_INPUT_METHOD_DELEGATE_H_
