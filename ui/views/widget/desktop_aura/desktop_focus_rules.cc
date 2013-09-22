// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_focus_rules.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace views {

DesktopFocusRules::DesktopFocusRules(aura::Window* content_window)
    : content_window_(content_window) {}

DesktopFocusRules::~DesktopFocusRules() {}

bool DesktopFocusRules::SupportsChildActivation(aura::Window* window) const {
  // In Desktop-Aura, only the content_window or children of the RootWindow are
  // activatable.
  return window == content_window_->parent() ||
         window->GetRootWindow() == window;
}

}  // namespace views
