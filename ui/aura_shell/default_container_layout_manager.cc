// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/default_container_layout_manager.h"

#include "base/auto_reset.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window_types.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/base/view_prop.h"
#include "ui/gfx/rect.h"
#include "views/widget/native_widget_aura.h"

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// DefaultContainerLayoutManager, public:

DefaultContainerLayoutManager::DefaultContainerLayoutManager(
    aura::Window* owner,
    WorkspaceManager* workspace_manager)
    : owner_(owner),
      workspace_manager_(workspace_manager),
      drag_window_(NULL),
      ignore_calculate_bounds_(false) {
}

DefaultContainerLayoutManager::~DefaultContainerLayoutManager() {}

void DefaultContainerLayoutManager::PrepareForMoveOrResize(
    aura::Window* drag,
    aura::MouseEvent* event) {
  drag_window_ = drag;
}

void DefaultContainerLayoutManager::CancelMoveOrResize(
    aura::Window* drag,
    aura::MouseEvent* event) {
  drag_window_ = NULL;
}

void DefaultContainerLayoutManager::ProcessMove(
    aura::Window* drag,
    aura::MouseEvent* event) {
  AutoReset<bool> reset(&ignore_calculate_bounds_, true);

  // TODO(oshima): Just zooming out may (and will) move/swap window without
  // a users's intent. We probably should scroll viewport, but that may not
  // be enough. See crbug.com/101826 for more discussion.
  workspace_manager_->SetOverview(true);

  gfx::Point point_in_owner = event->location();
  aura::Window::ConvertPointToWindow(
      drag,
      owner_,
      &point_in_owner);
  // TODO(oshima): We should support simply moving to another
  // workspace when the destination workspace has enough room to accomodate.
  aura::Window* rotate_target =
      workspace_manager_->FindRotateWindowForLocation(point_in_owner);
  if (rotate_target)
    workspace_manager_->RotateWindows(drag, rotate_target);
}

void DefaultContainerLayoutManager::EndMove(
    aura::Window* drag,
    aura::MouseEvent* evnet) {
  // TODO(oshima): finish moving window between workspaces.
  AutoReset<bool> reset(&ignore_calculate_bounds_, true);
  drag_window_ = NULL;
  Workspace* workspace = workspace_manager_->GetActiveWorkspace();
  if (workspace)
    workspace->Layout(NULL, NULL);
  workspace_manager_->SetOverview(false);
}

void DefaultContainerLayoutManager::EndResize(
    aura::Window* drag,
    aura::MouseEvent* evnet) {
  AutoReset<bool> reset(&ignore_calculate_bounds_, true);
  drag_window_ = NULL;
  Workspace* workspace = workspace_manager_->GetActiveWorkspace();
  if (workspace)
    workspace->Layout(NULL, NULL);
  workspace_manager_->SetOverview(false);
}

////////////////////////////////////////////////////////////////////////////////
// DefaultContainerLayoutManager, aura::LayoutManager implementation:

void DefaultContainerLayoutManager::OnWindowResized() {
  // Workspace is updated via DesktopObserver::OnDesktopResized.
}

void DefaultContainerLayoutManager::OnWindowAdded(aura::Window* child) {
  intptr_t type = reinterpret_cast<intptr_t>(
      ui::ViewProp::GetValue(child, views::NativeWidgetAura::kWindowTypeKey));
  if (type != views::Widget::InitParams::TYPE_WINDOW)
    return;

  AutoReset<bool> reset(&ignore_calculate_bounds_, true);

  Workspace* workspace = workspace_manager_->GetActiveWorkspace();
  if (workspace) {
    aura::Window* active = aura::Desktop::GetInstance()->active_window();
    // Active window may not be in the default container layer.
    if (!workspace->Contains(active))
      active = NULL;
    if (workspace->AddWindowAfter(child, active))
      return;
  }
  // Create new workspace if new |child| doesn't fit to current workspace.
  Workspace* new_workspace = workspace_manager_->CreateWorkspace();
  new_workspace->AddWindowAfter(child, NULL);
  new_workspace->Activate();
}

void DefaultContainerLayoutManager::OnWillRemoveWindow(aura::Window* child) {
  AutoReset<bool> reset(&ignore_calculate_bounds_, true);
  Workspace* workspace = workspace_manager_->FindBy(child);
  if (!workspace)
    return;
  workspace->RemoveWindow(child);
  if (workspace->is_empty())
    delete workspace;
}

void DefaultContainerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  NOTIMPLEMENTED();
}

void DefaultContainerLayoutManager::CalculateBoundsForChild(
    aura::Window* child,
    gfx::Rect* requested_bounds) {
  intptr_t type = reinterpret_cast<intptr_t>(
      ui::ViewProp::GetValue(child, views::NativeWidgetAura::kWindowTypeKey));
  if (type != views::Widget::InitParams::TYPE_WINDOW ||
      ignore_calculate_bounds_)
    return;

  // If a drag window is requesting bounds, make sure its attached to
  // the workarea's top and fits within the total drag area.
  if (drag_window_) {
    gfx::Rect drag_area =  workspace_manager_->GetDragAreaBounds();
    requested_bounds->set_y(drag_area.y());
    *requested_bounds = requested_bounds->AdjustToFit(drag_area);
    return;
  }

  Workspace* workspace = workspace_manager_->FindBy(child);
  gfx::Rect work_area = workspace->GetWorkAreaBounds();
  requested_bounds->set_origin(
      gfx::Point(child->GetTargetBounds().x(), work_area.y()));
  *requested_bounds = requested_bounds->AdjustToFit(work_area);
}

}  // namespace internal
}  // namespace aura_shell
