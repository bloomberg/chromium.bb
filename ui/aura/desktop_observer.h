// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_OBSERVER_H_
#define UI_AURA_DESKTOP_OBSERVER_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace gfx {
class Size;
}

namespace aura {

class Window;

class AURA_EXPORT DesktopObserver {
 public:
  // Invoked after the desktop is resized.
  virtual void OnDesktopResized(const gfx::Size& new_size) {}

  // Invoked when a new window is initialized.
  virtual void OnWindowInitialized(Window* window) {}

  // Invoked when the active window is changed. |active| may be NULL if there is
  // not active window.
  virtual void OnActiveWindowChanged(Window* active) {}

 protected:
  virtual ~DesktopObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_OBSERVER_H_
