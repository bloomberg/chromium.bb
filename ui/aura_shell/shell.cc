// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell.h"

#include "ui/aura/desktop.h"
#include "ui/aura/toplevel_window_container.h"
#include "ui/aura/window.h"
#include "ui/aura/window_types.h"
#include "ui/aura_shell/desktop_layout_manager.h"
#include "ui/aura_shell/shell_factory.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "views/widget/widget.h"

namespace aura_shell {

namespace {
// Creates each of the special window containers that holds windows of various
// types in the shell UI. They are added to |containers| from back to front in
// the z-index.
void CreateSpecialContainers(aura::Window::Windows* containers) {
  aura::Window* background_container = new aura::Window(NULL);
  background_container->set_id(
      internal::kShellWindowId_DesktopBackgroundContainer);
  containers->push_back(background_container);

  aura::Window* default_container = new aura::ToplevelWindowContainer;
  default_container->set_id(internal::kShellWindowId_DefaultContainer);
  containers->push_back(default_container);

  aura::Window* always_on_top_container = new aura::ToplevelWindowContainer;
  always_on_top_container->set_id(
      internal::kShellWindowId_AlwaysOnTopContainer);
  containers->push_back(always_on_top_container);

  aura::Window* launcher_container = new aura::Window(NULL);
  launcher_container->set_id(internal::kShellWindowId_LauncherContainer);
  containers->push_back(launcher_container);

  aura::Window* lock_container = new aura::Window(NULL);
  lock_container->set_stops_event_propagation(true);
  lock_container->set_id(internal::kShellWindowId_LockScreenContainer);
  containers->push_back(lock_container);

  aura::Window* status_container = new aura::Window(NULL);
  status_container->set_id(internal::kShellWindowId_StatusContainer);
  containers->push_back(status_container);

  aura::Window* menu_container = new aura::Window(NULL);
  menu_container->set_id(internal::kShellWindowId_MenusAndTooltipsContainer);
  containers->push_back(menu_container);
}
}  // namespace

Shell::Shell() {
  aura::Desktop::GetInstance()->SetDelegate(this);
}

Shell::~Shell() {
}

void Shell::Init() {
  aura::Window::Windows containers;
  CreateSpecialContainers(&containers);
  aura::Window::Windows::const_iterator i;
  for (i = containers.begin(); i != containers.end(); ++i) {
    (*i)->Init();
    aura::Desktop::GetInstance()->window()->AddChild(*i);
    (*i)->Show();
  }

  aura::Window* root_window = aura::Desktop::GetInstance()->window();
  internal::DesktopLayoutManager* desktop_layout =
      new internal::DesktopLayoutManager(root_window);
  root_window->SetLayoutManager(desktop_layout);

  desktop_layout->set_background_widget(internal::CreateDesktopBackground());
  desktop_layout->set_launcher_widget(internal::CreateLauncher());
  desktop_layout->set_status_area_widget(internal::CreateStatusArea());
}

aura::Window* Shell::GetContainer(int container_id) {
  return const_cast<aura::Window*>(
      const_cast<const aura_shell::Shell*>(this)->GetContainer(container_id));
}

const aura::Window* Shell::GetContainer(int container_id) const {
  return aura::Desktop::GetInstance()->window()->GetChildById(container_id);
}

void Shell::AddChildToDefaultParent(aura::Window* window) {
  aura::Window* parent = NULL;
  switch (window->type()) {
    case aura::kWindowType_Toplevel:
      parent = GetContainer(internal::kShellWindowId_DefaultContainer);
      break;
    case aura::kWindowType_Menu:
    case aura::kWindowType_Tooltip:
      parent = GetContainer(internal::kShellWindowId_MenusAndTooltipsContainer);
      break;
    default:
      // This will crash for controls, since they can't be parented to anything.
      break;
  }
  parent->AddChild(window);
}

aura::Window* Shell::GetTopmostWindowToActivate(aura::Window* ignore) const {
  const aura::ToplevelWindowContainer* container =
      GetContainer(internal::kShellWindowId_DefaultContainer)->
          AsToplevelWindowContainer();
  return container->GetTopmostWindowToActivate(ignore);
}

}  // namespace aura_shell
