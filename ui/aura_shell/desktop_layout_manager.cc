// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/desktop_layout_manager.h"

#include "ui/aura/window.h"
#include "views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// DesktopLayoutManager, public:

DesktopLayoutManager::DesktopLayoutManager(aura::Window* owner)
    : owner_(owner),
      background_widget_(NULL) {
}

DesktopLayoutManager::~DesktopLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopLayoutManager, aura::LayoutManager implementation:

void DesktopLayoutManager::OnWindowResized() {
  background_widget_->SetBounds(
      gfx::Rect(owner_->bounds().width(), owner_->bounds().height()));
}
