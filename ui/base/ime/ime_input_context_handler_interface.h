// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_INPUT_CONTEXT_HANDLER_INTERFACE_H_
#define UI_BASE_IME_IME_INPUT_CONTEXT_HANDLER_INTERFACE_H_

#include <string>
#include "base/basictypes.h"
#include "ui/base/ime/ui_base_ime_export.h"

namespace chromeos {
class CompositionText;
}

namespace ui {

class UI_BASE_IME_EXPORT IMEInputContextHandlerInterface {
 public:
  // Called when the engine commit a text.
  virtual void CommitText(const std::string& text) = 0;

  // Called when the engine updates composition text.
  virtual void UpdateCompositionText(const chromeos::CompositionText& text,
                                     uint32 cursor_pos,
                                     bool visible) = 0;

  // Called when the engine request deleting surrounding string.
  virtual void DeleteSurroundingText(int32 offset, uint32 length) = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_IME_INPUT_CONTEXT_HANDLER_INTERFACE_H_
