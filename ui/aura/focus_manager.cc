// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/focus_manager.h"

#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"

namespace aura {
namespace internal {

FocusManager::FocusManager(Window* owner)
    : owner_(owner),
      focused_window_(NULL) {
}

FocusManager::~FocusManager() {
}

void FocusManager::SetFocusedWindow(Window* focused_window) {
  if (focused_window == focused_window_)
    return;
  if (focused_window_)
    focused_window_->delegate()->OnBlur();
  focused_window_ = focused_window;
  if (focused_window_)
    focused_window_->delegate()->OnFocus();
}

}  // namespace internal
}  // namespace aura
