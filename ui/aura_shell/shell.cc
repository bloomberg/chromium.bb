// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_types.h"
#include "ui/aura_shell/default_container_event_filter.h"
#include "ui/aura_shell/default_container_layout_manager.h"
#include "ui/aura_shell/desktop_event_filter.h"
#include "ui/aura_shell/desktop_layout_manager.h"
#include "ui/aura_shell/drag_drop_controller.h"
#include "ui/aura_shell/launcher/launcher.h"
#include "ui/aura_shell/modal_container_layout_manager.h"
#include "ui/aura_shell/shelf_layout_controller.h"
#include "ui/aura_shell/shell_accelerator_controller.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_factory.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/aura_shell/stacking_controller.h"
#include "ui/aura_shell/toplevel_layout_manager.h"
#include "ui/aura_shell/toplevel_window_event_filter.h"
#include "ui/aura_shell/workspace_controller.h"
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
  aura::Desktop::GetInstance()->SetEventFilter(
      new internal::DesktopEventFilter);
  aura::Desktop::GetInstance()->SetStackingClient(
      new internal::StackingController);
}

Shell::~Shell() {
  // Drag drop controller needs a valid shell instance. We destroy it first.
  drag_drop_controller_.reset();

  DCHECK(instance_ == this);
  instance_ = NULL;

  // Make sure we delete WorkspaceController before launcher is
  // deleted as it has a reference to launcher model.
  workspace_controller_.reset();
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
void Shell::DeleteInstanceForTesting() {
  delete instance_;
  instance_ = NULL;
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

  internal::StackingController* stacking_controller =
      static_cast<internal::StackingController*>(
          desktop_window->stacking_client());
  stacking_controller->Init();

  internal::DesktopLayoutManager* desktop_layout =
      new internal::DesktopLayoutManager(desktop_window);
  desktop_window->SetLayoutManager(desktop_layout);

  desktop_layout->set_background_widget(internal::CreateDesktopBackground());
  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  launcher_.reset(new Launcher(default_container));

  views::Widget* status_widget = NULL;
  if (delegate_.get())
    status_widget = delegate_->CreateStatusArea();
  if (!status_widget)
    status_widget = internal::CreateStatusArea();

  shelf_layout_controller_.reset(new internal::ShelfLayoutController(
      launcher_->widget(), status_widget));

  desktop_layout->set_shelf(shelf_layout_controller_.get());

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAuraWindows)) {
    EnableWorkspaceManager();
  } else {
    internal::ToplevelLayoutManager* toplevel_layout_manager =
        new internal::ToplevelLayoutManager();
    default_container->SetLayoutManager(toplevel_layout_manager);
    toplevel_layout_manager->set_shelf(shelf_layout_controller_.get());
  }

  // Force a layout.
  desktop_layout->OnWindowResized();

  // Initialize drag drop controller.
  drag_drop_controller_.reset(new internal::DragDropController);
  aura::Desktop::GetInstance()->SetProperty(aura::kDesktopDragDropClientKey,
      static_cast<aura::DragDropClient*>(drag_drop_controller_.get()));
}

aura::Window* Shell::GetContainer(int container_id) {
  return const_cast<aura::Window*>(
      const_cast<const aura_shell::Shell*>(this)->GetContainer(container_id));
}

const aura::Window* Shell::GetContainer(int container_id) const {
  return aura::Desktop::GetInstance()->GetChildById(container_id);
}

void Shell::AddDesktopEventFilter(aura::EventFilter* filter) {
  static_cast<internal::DesktopEventFilter*>(
      aura::Desktop::GetInstance()->event_filter())->AddFilter(filter);
}

void Shell::RemoveDesktopEventFilter(aura::EventFilter* filter) {
  static_cast<internal::DesktopEventFilter*>(
      aura::Desktop::GetInstance()->event_filter())->RemoveFilter(filter);
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
  workspace_controller_->SetLauncherModel(launcher_->model());
  default_container->SetEventFilter(
      new internal::DefaultContainerEventFilter(default_container));
  default_container->SetLayoutManager(
      new internal::DefaultContainerLayoutManager(
          workspace_controller_->workspace_manager()));
}

}  // namespace aura_shell
