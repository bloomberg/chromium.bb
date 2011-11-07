// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/desktop.h"
#include "ui/aura/toplevel_window_container.h"
#include "ui/aura/window.h"
#include "ui/aura/window_types.h"
#include "ui/aura_shell/default_container_event_filter.h"
#include "ui/aura_shell/default_container_layout_manager.h"
#include "ui/aura_shell/desktop_layout_manager.h"
#include "ui/aura_shell/launcher/launcher.h"
#include "ui/aura_shell/shelf_layout_controller.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_factory.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/aura_shell/toplevel_layout_manager.h"
#include "ui/aura_shell/workspace/workspace_controller.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "views/widget/native_widget_aura.h"
#include "views/widget/widget.h"

namespace aura_shell {

namespace {

using views::Widget;

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

// static
Shell* Shell::instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

Shell::Shell()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  aura::Desktop::GetInstance()->SetDelegate(this);
}

Shell::~Shell() {
}

// static
Shell* Shell::GetInstance() {
  if (!instance_) {
    instance_ = new Shell;
    instance_->Init();
  }
  return instance_;
}

void Shell::Init() {
  aura::Desktop* desktop_window = aura::Desktop::GetInstance();
  desktop_window->SetCursor(aura::kCursorPointer);

  aura::Window::Windows containers;
  CreateSpecialContainers(&containers);
  aura::Window::Windows::const_iterator i;
  for (i = containers.begin(); i != containers.end(); ++i) {
    (*i)->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    desktop_window->AddChild(*i);
    (*i)->Show();
  }

  internal::DesktopLayoutManager* desktop_layout =
      new internal::DesktopLayoutManager(desktop_window);
  desktop_window->SetLayoutManager(desktop_layout);

  desktop_layout->set_background_widget(internal::CreateDesktopBackground());
  aura::ToplevelWindowContainer* toplevel_container =
      GetContainer(internal::kShellWindowId_DefaultContainer)->
          AsToplevelWindowContainer();
  launcher_.reset(new Launcher(toplevel_container));

  shelf_layout_controller_.reset(new internal::ShelfLayoutController(
      launcher_->widget(), internal::CreateStatusArea()));
  desktop_layout->set_shelf(shelf_layout_controller_.get());

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAuraWindows)) {
    EnableWorkspaceManager();
  } else {
    internal::ToplevelLayoutManager* toplevel_layout_manager =
        new internal::ToplevelLayoutManager();
    toplevel_container->SetLayoutManager(toplevel_layout_manager);
    toplevel_layout_manager->set_shelf(shelf_layout_controller_.get());
  }

  // Force a layout.
  desktop_layout->OnWindowResized();
}

void Shell::SetDelegate(ShellDelegate* delegate) {
  delegate_.reset(delegate);
}

aura::Window* Shell::GetContainer(int container_id) {
  return const_cast<aura::Window*>(
      const_cast<const aura_shell::Shell*>(this)->GetContainer(container_id));
}

const aura::Window* Shell::GetContainer(int container_id) const {
  return aura::Desktop::GetInstance()->GetChildById(container_id);
}

void Shell::ToggleOverview() {
  if (workspace_controller_.get())
    workspace_controller_->ToggleOverview();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

void Shell::EnableWorkspaceManager() {
  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);

  workspace_controller_.reset(
      new internal::WorkspaceController(default_container));
  default_container->SetEventFilter(
      new internal::DefaultContainerEventFilter(default_container));
  default_container->SetLayoutManager(
      new internal::DefaultContainerLayoutManager(
          workspace_controller_->workspace_manager()));
}

////////////////////////////////////////////////////////////////////////////////
// Shell, aura::DesktopDelegate implementation:

void Shell::AddChildToDefaultParent(aura::Window* window) {
  aura::Window* parent = NULL;
  switch (window->type()) {
    case aura::WINDOW_TYPE_NORMAL:
    case aura::WINDOW_TYPE_POPUP:
      parent = GetContainer(internal::kShellWindowId_DefaultContainer);
      break;
    case aura::WINDOW_TYPE_MENU:
    case aura::WINDOW_TYPE_TOOLTIP:
      parent = GetContainer(internal::kShellWindowId_MenusAndTooltipsContainer);
      break;
    default:
      NOTREACHED() << "Window " << window->id()
                   << " has unhandled type " << window->type();
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
