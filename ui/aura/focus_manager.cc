// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/focus_manager.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/focus_change_observer.h"
#include "ui/aura/window_delegate.h"

namespace aura {
FocusManager::FocusManager() : focused_window_(NULL) {
}

FocusManager::~FocusManager() {
}

void FocusManager::AddObserver(FocusChangeObserver* observer) {
  observers_.AddObserver(observer);
}

void FocusManager::RemoveObserver(FocusChangeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FocusManager::SetFocusedWindow(Window* focused_window,
                                    const Event* event) {
  if (focused_window == focused_window_)
    return;
  if (focused_window && !focused_window->CanFocus())
    return;
  // The NULL-check of |focused_window| is essential here before asking the
  // activation client, since it is valid to clear the focus by calling
  // SetFocusedWindow() to NULL.

  if (focused_window) {
    RootWindow* root = focused_window->GetRootWindow();
    DCHECK(root);
    if (client::GetActivationClient(root) &&
        !client::GetActivationClient(root)->OnWillFocusWindow(
            focused_window, event)) {
      return;
    }
  }

  Window* old_focused_window = focused_window_;
  focused_window_ = focused_window;
  if (old_focused_window && old_focused_window->delegate())
    old_focused_window->delegate()->OnBlur();
  if (focused_window_ && focused_window_->delegate())
    focused_window_->delegate()->OnFocus(old_focused_window);
  if (focused_window_) {
    FOR_EACH_OBSERVER(FocusChangeObserver, observers_,
                      OnWindowFocused(focused_window));
  }
}

Window* FocusManager::GetFocusedWindow() {
  return focused_window_;
}

bool FocusManager::IsFocusedWindow(const Window* window) const {
  return focused_window_ == window;
}

}  // namespace ash
