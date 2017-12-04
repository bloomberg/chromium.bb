// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_X11_KEYBOARD_IMPL_H_
#define REMOTING_HOST_LINUX_X11_KEYBOARD_IMPL_H_

#include "remoting/host/linux/x11_keyboard.h"

#include "base/macros.h"
#include "ui/gfx/x/x11.h"

namespace remoting {

class X11KeyboardImpl : public X11Keyboard {
 public:
  X11KeyboardImpl(Display* display);
  ~X11KeyboardImpl() override;

  // KeyboardInterface overrides.
  std::vector<uint32_t> GetUnusedKeycodes() override;

  void PressKey(uint32_t keycode, uint32_t modifiers) override;

  bool FindKeycode(uint32_t code_point,
                   uint32_t* keycode,
                   uint32_t* modifiers) override;

  bool ChangeKeyMapping(uint32_t keycode, uint32_t code_point) override;

  void Flush() override;

  void Sync() override;

 private:
  // X11 graphics context.
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(X11KeyboardImpl);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X11_KEYBOARD_IMPL_H_
