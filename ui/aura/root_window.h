// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_H_
#define UI_AURA_ROOT_WINDOW_H_
#pragma once

#include "ui/aura/window.h"

namespace aura {
namespace internal {

class FocusManager;

// A Window subclass that handles event targeting for certain types of
// MouseEvent.
class RootWindow : public Window {
 public:
  RootWindow();
  virtual ~RootWindow();

  // Handles a mouse event. Returns true if handled.
  bool HandleMouseEvent(const MouseEvent& event);

  // Handles a key event. Returns true if handled.
  bool HandleKeyEvent(const KeyEvent& event);

  // Overridden from Window:
  virtual bool IsTopLevelWindowContainer() const OVERRIDE;
  virtual FocusManager* GetFocusManager() OVERRIDE;

 private:
  Window* mouse_pressed_handler_;
  scoped_ptr<FocusManager> focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(RootWindow);
};

}  // namespace internal
}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_H_
