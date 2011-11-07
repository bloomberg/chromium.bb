// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WINDOW_FRAME_H_
#define UI_AURA_SHELL_WINDOW_FRAME_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace aura_shell {

// aura::Window property name for a pointer to the WindowFrame interface.
AURA_SHELL_EXPORT extern const char* const kWindowFrameKey;

// Interface for clients implementing a window frame.  Implementors should
// add a pointer to this interface to each aura::Window, using the key above.
class AURA_SHELL_EXPORT WindowFrame {
 public:
  virtual ~WindowFrame() {}

  // Called when the mouse enters or exits a top-level window.
  virtual void OnWindowHoverChanged(bool hovered) = 0;
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WINDOW_FRAME_H_
