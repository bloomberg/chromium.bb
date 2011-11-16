// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace_controller.h"

#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/default_container_layout_manager.h"
#include "ui/aura_shell/launcher/launcher.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"

namespace aura_shell {
namespace internal {

WorkspaceController::WorkspaceController(aura::Window* viewport)
    : workspace_manager_(new WorkspaceManager(viewport)),
      launcher_model_(NULL),
      ignore_move_event_(false) {
  workspace_manager_->AddObserver(this);
  aura::Desktop::GetInstance()->AddObserver(this);
}

WorkspaceController::~WorkspaceController() {
  workspace_manager_->RemoveObserver(this);
  if (launcher_model_)
    launcher_model_->RemoveObserver(this);
  aura::Desktop::GetInstance()->RemoveObserver(this);
}

void WorkspaceController::ToggleOverview() {
  workspace_manager_->SetOverview(!workspace_manager_->is_overview());
}

void WorkspaceController::SetLauncherModel(LauncherModel* launcher_model) {
  DCHECK(!launcher_model_);
  launcher_model_ = launcher_model;
  launcher_model_->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceController, aura::DesktopObserver overrides:

void WorkspaceController::OnDesktopResized(const gfx::Size& new_size) {
  workspace_manager_->SetWorkspaceSize(new_size);
}

void WorkspaceController::OnActiveWindowChanged(aura::Window* active) {
  // FindBy handles NULL.
  Workspace* workspace = workspace_manager_->FindBy(active);
  if (workspace)
    workspace->Activate();
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceController, aura_shell::internal::WorkspaceObserver overrides:

void WorkspaceController::WindowMoved(WorkspaceManager* manager,
                                      aura::Window* source,
                                      aura::Window* target) {
  if (ignore_move_event_ || !launcher_model_)
    return;
  int start_index = launcher_model_->ItemIndexByWindow(source);
  int target_index = launcher_model_->ItemIndexByWindow(target);
  // The following condition may not be hold depending on future
  // launcher design. Using DCHECK so that we can catch such change.
  DCHECK(start_index >=0);
  DCHECK(target_index >=0);

  ignore_move_event_ = true;
  launcher_model_->Move(start_index, target_index);
  ignore_move_event_ = false;
}

void WorkspaceController::ActiveWorkspaceChanged(WorkspaceManager* manager,
                                                 Workspace* old) {
  // TODO(oshima): Update Launcher and Status area state when the active
  // workspace's fullscreen state changes.
  //NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceController, aura_shell::LauncherModelObserver overrides:

void WorkspaceController::LauncherItemAdded(int index) {
}

void WorkspaceController::LauncherItemRemoved(int index) {
}

void WorkspaceController::LauncherItemMoved(int start_index, int target_index) {
  if (ignore_move_event_)
    return;
  DCHECK(launcher_model_);
  // Adjust target/source indices as the item positions in |launcher_model_|
  // has already been updated.
  aura::Window* start = launcher_model_->items()[target_index].window;
  size_t target_index_in_model =
      start_index < target_index ? target_index - 1 : target_index + 1;
  DCHECK_LE(target_index_in_model, launcher_model_->items().size());
  aura::Window* target = launcher_model_->items()[target_index_in_model].window;

  ignore_move_event_ = true;
  workspace_manager_->RotateWindows(start, target);
  ignore_move_event_ = false;
}

void WorkspaceController::LauncherItemImagesChanged(int index) {
}

}  // namespace internal
}  // namespace aura_shell
