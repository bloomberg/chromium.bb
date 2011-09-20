// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop.h"
#include "ui/aura/toplevel_window_container.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/desktop_layout_manager.h"
#include "ui/aura_shell/shell_factory.h"

namespace aura_shell {

void InitDesktopWindow() {
  aura::Window* desktop_window = aura::Desktop::GetInstance()->window();
  internal::DesktopLayoutManager* desktop_layout =
      new internal::DesktopLayoutManager(desktop_window);
  desktop_window->SetLayoutManager(desktop_layout);

  // The order of creation here is important to establish the z-index:
  desktop_layout->set_background_widget(internal::CreateDesktopBackground());
  desktop_layout->set_toplevel_window_container(
      aura::Desktop::GetInstance()->toplevel_window_container());
  desktop_window->MoveChildToFront(
      aura::Desktop::GetInstance()->toplevel_window_container());
  desktop_layout->set_launcher_widget(internal::CreateLauncher());
  desktop_layout->set_status_area_widget(internal::CreateStatusArea());
}

}  // namespace aura_shell

