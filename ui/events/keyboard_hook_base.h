// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYBOARD_HOOK_BASE_H_
#define UI_EVENTS_KEYBOARD_HOOK_BASE_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/keyboard_hook.h"

namespace ui {

class KeyEvent;

class KeyboardHookBase : public KeyboardHook {
 public:
  KeyboardHookBase(base::Optional<base::flat_set<int>> native_key_codes,
                   KeyEventCallback callback);
  ~KeyboardHookBase() override;

 protected:
  // Indicates whether |key_code| should be intercepted by the keyboard hook.
  bool ShouldCaptureKeyEvent(int key_code) const;

  // Forwards the key event using |key_event_callback_|.
  void ForwardCapturedKeyEvent(std::unique_ptr<KeyEvent> event);

 private:
  // Used to forward key events.
  KeyEventCallback key_event_callback_;

  // The set of keys which should be intercepted by the keyboard hook.
  base::Optional<base::flat_set<int>> key_codes_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardHookBase);
};

}  // namespace ui

#endif  // UI_EVENTS_KEYBOARD_HOOK_BASE_H_
