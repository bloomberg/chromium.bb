// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/default_container_layout_manager.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_types.h"
#include "ui/aura_shell/property_util.h"
#include "ui/aura_shell/show_state_controller.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/native_widget_aura.h"

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// DefaultContainerLayoutManager, public:

DefaultContainerLayoutManager::DefaultContainerLayoutManager(
    WorkspaceManager* workspace_manager)
    : workspace_manager_(workspace_manager),
      show_state_controller_(new ShowStateController(workspace_manager)) {
}

DefaultContainerLayoutManager::~DefaultContainerLayoutManager() {}

void DefaultContainerLayoutManager::PrepareForMoveOrResize(
    aura::Window* drag,
    aura::MouseEvent* event) {
  workspace_manager_->set_ignored_window(drag);
}

void DefaultContainerLayoutManager::CancelMoveOrResize(
    aura::Window* drag,
    aura::MouseEvent* event) {
  workspace_manager_->set_ignored_window(NULL);
}

void DefaultContainerLayoutManager::ProcessMove(
    aura::Window* drag,
    aura::MouseEvent* event) {
  // TODO(oshima): Just zooming out may (and will) move/swap window without
  // a users's intent. We probably should scroll viewport, but that may not
  // be enough. See crbug.com/101826 for more discussion.
  workspace_manager_->SetOverview(true);

  gfx::Point point_in_owner = event->location();
  aura::Window::ConvertPointToWindow(
      drag,
      workspace_manager_->contents_view(),
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
  workspace_manager_->set_ignored_window(NULL);
  Workspace* workspace = workspace_manager_->FindBy(drag);
  workspace->Layout(NULL);
  workspace->Activate();
  workspace_manager_->SetOverview(false);
}

void DefaultContainerLayoutManager::EndResize(
    aura::Window* drag,
    aura::MouseEvent* evnet) {
  workspace_manager_->set_ignored_window(NULL);
  Workspace* workspace = workspace_manager_->GetActiveWorkspace();
  if (workspace)
    workspace->Layout(NULL);
  workspace_manager_->SetOverview(false);
}

////////////////////////////////////////////////////////////////////////////////
// DefaultContainerLayoutManager, aura::LayoutManager implementation:

void DefaultContainerLayoutManager::OnWindowResized() {
  // Workspace is updated via DesktopObserver::OnDesktopResized.
}

void DefaultContainerLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (child->type() != aura::WINDOW_TYPE_NORMAL || child->transient_parent())
    return;

  if (!child->GetProperty(aura::kShowStateKey))
    child->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_NORMAL);

  child->AddObserver(show_state_controller_.get());

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

void DefaultContainerLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  child->RemoveObserver(show_state_controller_.get());
  ClearRestoreBounds(child);

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

void DefaultContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  gfx::Rect adjusted_bounds = requested_bounds;

  // First, calculate the adjusted bounds.
  if (child->type() != aura::WINDOW_TYPE_NORMAL ||
      workspace_manager_->layout_in_progress() ||
      child->transient_parent()) {
    // Use the requested bounds as is.
  } else if (child == workspace_manager_->ignored_window()) {
    // If a drag window is requesting bounds, make sure its attached to
    // the workarea's top and fits within the total drag area.
    gfx::Rect drag_area =  workspace_manager_->GetDragAreaBounds();
    adjusted_bounds.set_y(drag_area.y());
    adjusted_bounds = adjusted_bounds.AdjustToFit(drag_area);
  } else {
    Workspace* workspace = workspace_manager_->FindBy(child);
    gfx::Rect work_area = workspace->GetWorkAreaBounds();
    adjusted_bounds.set_origin(
        gfx::Point(child->GetTargetBounds().x(), work_area.y()));
    adjusted_bounds = adjusted_bounds.AdjustToFit(work_area);
  }

  ui::WindowShowState show_state = static_cast<ui::WindowShowState>(
      child->GetIntProperty(aura::kShowStateKey));

  // Second, check if the window is either maximized or in fullscreen mode.
  if (show_state == ui::SHOW_STATE_MAXIMIZED ||
      show_state == ui::SHOW_STATE_FULLSCREEN) {
    // If the request is not from workspace manager,
    // remember the requested bounds.
    if (!workspace_manager_->layout_in_progress())
      SetRestoreBounds(child, adjusted_bounds);

    Workspace* workspace = workspace_manager_->FindBy(child);
    if (show_state == ui::SHOW_STATE_MAXIMIZED)
      adjusted_bounds = workspace->GetWorkAreaBounds();
    else
      adjusted_bounds = workspace->bounds();
    // Don't
    if (child->GetTargetBounds() == adjusted_bounds)
      return;
  }
  SetChildBoundsDirect(child, adjusted_bounds);
}

}  // namespace internal
}  // namespace aura_shell
