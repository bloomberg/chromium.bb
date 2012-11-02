// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop/desktop_stacking_client.h"

#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_capture_client.h"
#include "ui/aura/window.h"

namespace aura {

DesktopStackingClient::DesktopStackingClient() {
  aura::client::SetStackingClient(this);
}

DesktopStackingClient::~DesktopStackingClient() {
  aura::client::SetStackingClient(NULL);
}

Window* DesktopStackingClient::GetDefaultParent(Window* window,
                                                const gfx::Rect& bounds) {
  if (!null_parent_.get()) {
    null_parent_.reset(new aura::RootWindow(
        aura::RootWindow::CreateParams(gfx::Rect(100, 100))));
    null_parent_->Init();
    null_parent_->set_focus_manager(new FocusManager);

    capture_client_.reset(
        new aura::shared::RootWindowCaptureClient(null_parent_.get()));
  }
  return null_parent_.get();
}

}  // namespace aura
