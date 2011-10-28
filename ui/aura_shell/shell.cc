// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell.h"

#include "base/bind.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/toplevel_window_container.h"
#include "ui/aura/window.h"
#include "ui/aura/window_types.h"
#include "ui/aura_shell/default_container_event_filter.h"
#include "ui/aura_shell/default_container_layout_manager.h"
#include "ui/aura_shell/desktop_layout_manager.h"
#include "ui/aura_shell/launcher/launcher.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_factory.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/base/view_prop.h"
#include "ui/gfx/compositor/layer.h"
#include "views/widget/native_widget_aura.h"
#include "views/widget/widget.h"

namespace aura_shell {

namespace {

// The right/left margin of work area in the screen.
const int kWorkAreaHorizontalMargin = 15;

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
  default_container->SetEventFilter(
      new internal::DefaultContainerEventFilter(default_container));
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
  desktop_layout->set_launcher_widget(launcher_->widget());
  desktop_layout->set_status_area_widget(internal::CreateStatusArea());

  desktop_window->screen()->set_work_area_insets(
      gfx::Insets(
          0, kWorkAreaHorizontalMargin,
          launcher_->widget()->GetWindowScreenBounds().height(),
          kWorkAreaHorizontalMargin));

  // Workspace Manager
  workspace_manager_.reset(new WorkspaceManager(toplevel_container));
  toplevel_container->SetLayoutManager(
      new internal::DefaultContainerLayoutManager(
          toplevel_container, workspace_manager_.get()));

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
  workspace_manager_->SetOverview(!workspace_manager_->is_overview());
}

////////////////////////////////////////////////////////////////////////////////
// Shell, aura::DesktopDelegate implementation:

void Shell::AddChildToDefaultParent(aura::Window* window) {
  aura::Window* parent = NULL;
  intptr_t type = reinterpret_cast<intptr_t>(
      ui::ViewProp::GetValue(window, views::NativeWidgetAura::kWindowTypeKey));
  switch (static_cast<Widget::InitParams::Type>(type)) {
    case Widget::InitParams::TYPE_WINDOW:
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
    case Widget::InitParams::TYPE_CONTROL:
    case Widget::InitParams::TYPE_BUBBLE:
    case Widget::InitParams::TYPE_POPUP:
      parent = GetContainer(internal::kShellWindowId_DefaultContainer);
      break;
    case Widget::InitParams::TYPE_MENU:
    case Widget::InitParams::TYPE_TOOLTIP:
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
