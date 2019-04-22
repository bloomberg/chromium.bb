// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_aura_window_utils.h"

#include "ui/views/widget/widget.h"

namespace views {

namespace {
AXAuraWindowUtils* g_instance = nullptr;
}

AXAuraWindowUtils::~AXAuraWindowUtils() = default;

// static
AXAuraWindowUtils* AXAuraWindowUtils::Get() {
  if (!g_instance)
    g_instance = new AXAuraWindowUtils();
  return g_instance;
}

// static
void AXAuraWindowUtils::Set(std::unique_ptr<AXAuraWindowUtils> new_instance) {
  if (g_instance)
    delete g_instance;
  g_instance = new_instance.release();
}

aura::Window* AXAuraWindowUtils::GetParent(aura::Window* window) {
  return window->parent();
}

aura::Window::Windows AXAuraWindowUtils::GetChildren(aura::Window* window) {
  return window->children();
}

bool AXAuraWindowUtils::IsRootWindow(aura::Window* window) const {
  return window->IsRootWindow();
}

views::Widget* AXAuraWindowUtils::GetWidgetForNativeView(aura::Window* window) {
  return Widget::GetWidgetForNativeView(window);
}

AXAuraWindowUtils::AXAuraWindowUtils() = default;

}  // namespace views
