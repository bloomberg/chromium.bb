// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace/workspace_controller.h"

#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/default_container_layout_manager.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"

namespace aura_shell {
namespace internal {

WorkspaceController::WorkspaceController(aura::Window* window)
    : workspace_manager_(new WorkspaceManager(window)),
      layout_manager_(new internal::DefaultContainerLayoutManager(
          window, workspace_manager_.get())) {
  window->SetLayoutManager(layout_manager_);
  aura::Desktop::GetInstance()->AddObserver(this);
}

WorkspaceController::~WorkspaceController() {
  aura::Desktop::GetInstance()->RemoveObserver(this);
}

void WorkspaceController::ToggleOverview() {
  workspace_manager_->SetOverview(!workspace_manager_->is_overview());
}

void WorkspaceController::OnDesktopResized(const gfx::Size& new_size) {
  layout_manager_->set_ignore_calculate_bounds(true);
  workspace_manager_->SetWorkspaceSize(new_size);
  layout_manager_->set_ignore_calculate_bounds(false);
}

void WorkspaceController::OnActiveWindowChanged(aura::Window* active) {
  // FindBy handles NULL.
  Workspace* workspace = workspace_manager_->FindBy(active);
  if (workspace)
    workspace->Activate();
}

}  // namespace internal
}  // namespace aura_shell
