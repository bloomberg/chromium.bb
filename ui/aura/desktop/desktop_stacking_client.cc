// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop/desktop_stacking_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace aura {

DesktopStackingClient::DesktopStackingClient() {
  aura::client::SetStackingClient(this);
}

DesktopStackingClient::~DesktopStackingClient() {
  aura::client::SetStackingClient(NULL);
}

Window* DesktopStackingClient::GetDefaultParent(Window* window) {
  if (!null_parent_.get()) {
    null_parent_.reset(new aura::RootWindow(gfx::Rect(100, 100)));
    null_parent_->Init();
  }
  return null_parent_.get();
}

}  // namespace aura
