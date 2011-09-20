// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop.h"
#include "ui/aura/toplevel_window_container.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/desktop_layout_manager.h"
#include "ui/aura_shell/shell_factory.h"

namespace aura_shell {

namespace internal {

aura::Window* CreateToplevelWindowContainer() {
  aura::Window* container = new aura::internal::ToplevelWindowContainer;
  container->Init();
  container->SetBounds(gfx::Rect(0, 0, 1024, 768), 0);
  container->SetVisibility(aura::Window::VISIBILITY_SHOWN);
  container->SetParent(aura::Desktop::GetInstance()->window());
  aura::Desktop::GetInstance()->set_toplevel_window_container(container);
  return container;
}

}  // namespace internal

void InitDesktopWindow() {
  aura::Window* desktop_window = aura::Desktop::GetInstance()->window();
  internal::DesktopLayoutManager* desktop_layout =
      new internal::DesktopLayoutManager(desktop_window);
  desktop_window->SetLayoutManager(desktop_layout);

  // The order of creation here is important to establish the z-index:
  desktop_layout->set_background_widget(internal::CreateDesktopBackground());
  desktop_layout->set_toplevel_window_container(
      internal::CreateToplevelWindowContainer());
  desktop_layout->set_launcher_widget(internal::CreateLauncher());
  desktop_layout->set_status_area_widget(internal::CreateStatusArea());
}

}  // namespace aura_shell

