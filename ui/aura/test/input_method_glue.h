// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_INPUT_METHOD_GLUE_H_
#define UI_AURA_TEST_INPUT_METHOD_GLUE_H_

#include "ui/base/ime/input_method_delegate.h"
#include "ui/events/event_handler.h"

namespace aura {

class WindowTreeHost;

class InputMethodGlue : public ui::EventHandler,
                        public ui::internal::InputMethodDelegate {
 public:
  explicit InputMethodGlue(aura::WindowTreeHost* host);
  ~InputMethodGlue() override;

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* key) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(ui::KeyEvent* key) override;

  aura::WindowTreeHost* host_;
  bool processing_ime_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(InputMethodGlue);
};

}  // namespace aura

#endif  // UI_AURA_TEST_INPUT_METHOD_GLUE_H_
