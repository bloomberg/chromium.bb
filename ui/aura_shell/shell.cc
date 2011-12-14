// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_types.h"
#include "ui/aura_shell/activation_controller.h"
#include "ui/aura_shell/app_list.h"
#include "ui/aura_shell/default_container_event_filter.h"
#include "ui/aura_shell/default_container_layout_manager.h"
#include "ui/aura_shell/root_window_event_filter.h"
#include "ui/aura_shell/root_window_layout_manager.h"
#include "ui/aura_shell/drag_drop_controller.h"
#include "ui/aura_shell/launcher/launcher.h"
#include "ui/aura_shell/modal_container_layout_manager.h"
#include "ui/aura_shell/shadow_controller.h"
#include "ui/aura_shell/shelf_layout_manager.h"
#include "ui/aura_shell/shell_accelerator_controller.h"
#include "ui/aura_shell/shell_accelerator_filter.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_factory.h"
#include "ui/aura_shell/shell_tooltip_manager.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/aura_shell/stacking_controller.h"
#include "ui/aura_shell/status_area_layout_manager.h"
#include "ui/aura_shell/toplevel_layout_manager.h"
#include "ui/aura_shell/toplevel_window_event_filter.h"
#include "ui/aura_shell/workspace_controller.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

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

  aura::Window* default_container = new aura::Window(NULL);
  default_container->SetEventFilter(
      new ToplevelWindowEventFilter(default_container));
  default_container->set_id(internal::kShellWindowId_DefaultContainer);
  containers->push_back(default_container);

  aura::Window* always_on_top_container = new aura::Window(NULL);
  always_on_top_container->SetEventFilter(
      new ToplevelWindowEventFilter(always_on_top_container));
  always_on_top_container->set_id(
      internal::kShellWindowId_AlwaysOnTopContainer);
  containers->push_back(always_on_top_container);

  aura::Window* launcher_container = new aura::Window(NULL);
  launcher_container->set_id(internal::kShellWindowId_LauncherContainer);
  containers->push_back(launcher_container);

  aura::Window* modal_container = new aura::Window(NULL);
  modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(modal_container));
  modal_container->SetLayoutManager(
      new internal::ModalContainerLayoutManager(modal_container));
  modal_container->set_id(internal::kShellWindowId_ModalContainer);
  containers->push_back(modal_container);

  // TODO(beng): Figure out if we can make this use ModalityEventFilter instead
  //             of stops_event_propagation.
  aura::Window* lock_container = new aura::Window(NULL);
  lock_container->set_stops_event_propagation(true);
  lock_container->set_id(internal::kShellWindowId_LockScreenContainer);
  containers->push_back(lock_container);

  aura::Window* lock_modal_container = new aura::Window(NULL);
  lock_modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(lock_modal_container));
  lock_modal_container->SetLayoutManager(
      new internal::ModalContainerLayoutManager(lock_modal_container));
  lock_modal_container->set_id(internal::kShellWindowId_LockModalContainer);
  containers->push_back(lock_modal_container);

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

Shell::Shell(ShellDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      accelerator_controller_(new ShellAcceleratorController),
      delegate_(delegate) {
  aura::RootWindow::GetInstance()->SetEventFilter(
      new internal::RootWindowEventFilter);
}

Shell::~Shell() {
  RemoveRootWindowEventFilter(accelerator_filter_.get());

  // ShellTooltipManager needs a valid shell instance. We delete it before
  // deleting the shell |instance_|.
  RemoveRootWindowEventFilter(tooltip_manager_.get());
  aura::RootWindow::GetInstance()->SetProperty(
      aura::kRootWindowTooltipClientKey,
      NULL);

  // Make sure we delete WorkspaceController before launcher is
  // deleted as it has a reference to launcher model.
  workspace_controller_.reset();
  launcher_.reset();

  // Delete containers now so that child windows does not access
  // observers when they are destructed. This has to be after launcher
  // is destructed because launcher closes the widget in its destructor.
  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  while (!root_window->children().empty()) {
    aura::Window* child = root_window->children()[0];
    delete child;
  }

  tooltip_manager_.reset();

  // Drag drop controller needs a valid shell instance. We destroy it first.
  drag_drop_controller_.reset();

  DCHECK(instance_ == this);
  instance_ = NULL;
}

// static
Shell* Shell::CreateInstance(ShellDelegate* delegate) {
  CHECK(!instance_);
  instance_ = new Shell(delegate);
  instance_->Init();
  return instance_;
}

// static
Shell* Shell::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

// static
void Shell::DeleteInstance() {
  delete instance_;
  instance_ = NULL;
}

void Shell::Init() {
  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  root_window->SetCursor(aura::kCursorPointer);

  activation_controller_.reset(new internal::ActivationController);

  aura::Window::Windows containers;
  CreateSpecialContainers(&containers);
  aura::Window::Windows::const_iterator i;
  for (i = containers.begin(); i != containers.end(); ++i) {
    (*i)->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    root_window->AddChild(*i);
    (*i)->Show();
  }

  // This is created after the special containers, since it expects them.
  stacking_controller_.reset(new internal::StackingController);

  internal::RootWindowLayoutManager* root_window_layout =
      new internal::RootWindowLayoutManager(root_window);
  root_window->SetLayoutManager(root_window_layout);

  root_window_layout->set_background_widget(
      internal::CreateDesktopBackground());
  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  launcher_.reset(new Launcher(default_container));

  views::Widget* status_widget = NULL;
  if (delegate_.get())
    status_widget = delegate_->CreateStatusArea();
  if (!status_widget)
    status_widget = internal::CreateStatusArea();

  internal::ShelfLayoutManager* shelf_layout_manager =
      new internal::ShelfLayoutManager(launcher_->widget(), status_widget);
  GetContainer(aura_shell::internal::kShellWindowId_LauncherContainer)->
      SetLayoutManager(shelf_layout_manager);

  internal::StatusAreaLayoutManager* status_area_layout_manager =
      new internal::StatusAreaLayoutManager(shelf_layout_manager);
  GetContainer(aura_shell::internal::kShellWindowId_StatusContainer)->
      SetLayoutManager(status_area_layout_manager);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAuraNoShadows))
    shadow_controller_.reset(new internal::ShadowController());

  if (command_line->HasSwitch(switches::kAuraWorkspaceManager)) {
    EnableWorkspaceManager();
  } else {
    internal::ToplevelLayoutManager* toplevel_layout_manager =
        new internal::ToplevelLayoutManager();
    default_container->SetLayoutManager(toplevel_layout_manager);
    toplevel_layout_manager->set_shelf(shelf_layout_manager);
  }

  // Force a layout.
  root_window_layout->OnWindowResized();

  // Initialize ShellAcceleratorFilter
  accelerator_filter_.reset(new internal::ShellAcceleratorFilter);
  AddRootWindowEventFilter(accelerator_filter_.get());

  // Initialize ShellTooltipManager
  tooltip_manager_.reset(new ShellTooltipManager);
  aura::RootWindow::GetInstance()->SetProperty(
      aura::kRootWindowTooltipClientKey,
      static_cast<aura::TooltipClient*>(tooltip_manager_.get()));
  AddRootWindowEventFilter(tooltip_manager_.get());

  // Initialize drag drop controller.
  drag_drop_controller_.reset(new internal::DragDropController);
  aura::RootWindow::GetInstance()->SetProperty(
      aura::kRootWindowDragDropClientKey,
      static_cast<aura::DragDropClient*>(drag_drop_controller_.get()));
}

aura::Window* Shell::GetContainer(int container_id) {
  return const_cast<aura::Window*>(
      const_cast<const aura_shell::Shell*>(this)->GetContainer(container_id));
}

const aura::Window* Shell::GetContainer(int container_id) const {
  return aura::RootWindow::GetInstance()->GetChildById(container_id);
}

void Shell::AddRootWindowEventFilter(aura::EventFilter* filter) {
  static_cast<internal::RootWindowEventFilter*>(
      aura::RootWindow::GetInstance()->event_filter())->AddFilter(filter);
}

void Shell::RemoveRootWindowEventFilter(aura::EventFilter* filter) {
  static_cast<internal::RootWindowEventFilter*>(
      aura::RootWindow::GetInstance()->event_filter())->RemoveFilter(filter);
}

void Shell::ToggleOverview() {
  if (workspace_controller_.get())
    workspace_controller_->ToggleOverview();
}

void Shell::ToggleAppList() {
  if (!app_list_.get())
    app_list_.reset(new internal::AppList);
  app_list_->SetVisible(!app_list_->IsVisible());
}

// Returns true if the screen is locked.
bool Shell::IsScreenLocked() const {
  const aura::Window* lock_screen_container = GetContainer(
      internal::kShellWindowId_LockScreenContainer);
  const aura::Window::Windows& lock_screen_windows =
      lock_screen_container->children();
  aura::Window::Windows::const_iterator lock_screen_it =
      std::find_if(lock_screen_windows.begin(), lock_screen_windows.end(),
                   std::mem_fun(&aura::Window::IsVisible));
  if (lock_screen_it != lock_screen_windows.end())
    return true;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

void Shell::EnableWorkspaceManager() {
  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);

  workspace_controller_.reset(
      new internal::WorkspaceController(default_container));
  workspace_controller_->SetLauncherModel(launcher_->model());
  default_container->SetEventFilter(
      new internal::DefaultContainerEventFilter(default_container));
  default_container->SetLayoutManager(
      new internal::DefaultContainerLayoutManager(
          workspace_controller_->workspace_manager()));
}

}  // namespace aura_shell
