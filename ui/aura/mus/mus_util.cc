// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/mus_util.h"

#include "ui/aura/window.h"

namespace aura {

mus::Window* GetMusWindow(Window* window) {
  if (!window)
    return nullptr;
  return static_cast<mus::Window*>(window->GetNativeWindowProperty("mus"));
}

void SetMusWindow(Window* window, mus::Window* mus_window) {
  window->SetNativeWindowProperty("mus", mus_window);
}

}  // namespace aura
