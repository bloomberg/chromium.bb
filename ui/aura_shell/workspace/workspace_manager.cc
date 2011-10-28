// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace/workspace_manager.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

namespace {

// The horizontal margein between workspaces in pixels.
const int kWorkspaceHorizontalMargin = 50;

// Minimum/maximum scale for overview mode.
const float kMaxOverviewScale = 0.9f;
const float kMinOverviewScale = 0.3f;

}

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// WindowManager, public:

WorkspaceManager::WorkspaceManager(aura::Window* viewport)
    : viewport_(viewport),
      active_workspace_(NULL),
      workspace_size_(
          gfx::Screen::GetMonitorAreaNearestWindow(viewport_).size()),
      is_overview_(false) {
}

WorkspaceManager::~WorkspaceManager() {
  std::vector<Workspace*> copy_to_delete(workspaces_);
  STLDeleteElements(&copy_to_delete);
}

Workspace* WorkspaceManager::CreateWorkspace() {
  Workspace* workspace = new Workspace(this);
  LayoutWorkspaces();
  return workspace;
}

Workspace* WorkspaceManager::GetActiveWorkspace() const {
  return active_workspace_;
}

Workspace* WorkspaceManager::FindBy(aura::Window* window) const {
  int index = GetWorkspaceIndexContaining(window);
  return index < 0 ? NULL : workspaces_[index];
}

aura::Window* WorkspaceManager::FindRotateWindowForLocation(
    const gfx::Point& point) {
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end();
       ++i) {
    aura::Window* window = (*i)->FindRotateWindowForLocation(point);
    if (window)
      return window;
  }
  return NULL;
}

void WorkspaceManager::LayoutWorkspaces() {
  UpdateViewport();

  gfx::Rect bounds(workspace_size_);
  int x = 0;
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end();
       ++i) {
    Workspace* workspace = *i;
    bounds.set_x(x);
    workspace->SetBounds(bounds);
    x += bounds.width() + kWorkspaceHorizontalMargin;
  }
}

gfx::Rect WorkspaceManager::GetDragAreaBounds() {
  return GetWorkAreaBounds(gfx::Rect(viewport_->bounds().size()));
}

void WorkspaceManager::SetOverview(bool overview) {
  if (is_overview_ == overview)
    return;
  is_overview_ = overview;

  ui::Transform transform;
  if (is_overview_) {
    // TODO(oshima|sky): We limit the how small windows can be shrinked
    // in overview mode, thus part of the viewport may not be visible.
    // We need to add capability to scroll/move viewport in overview mode.
    float scale = std::min(
        kMaxOverviewScale,
        workspace_size_.width() /
        static_cast<float>(viewport_->bounds().width()));
    scale = std::max(kMinOverviewScale, scale);

    transform.SetScale(scale, scale);

    int overview_width = viewport_->bounds().width() * scale;
    int dx = 0;
    if (overview_width < workspace_size_.width()) {
      dx = (workspace_size_.width() - overview_width) / 2;
    } else if (active_workspace_) {
      // Center the active workspace.
      int active_workspace_mid_x = (active_workspace_->bounds().x() +
          active_workspace_->bounds().width() / 2) * scale;
      dx = workspace_size_.width() / 2 - active_workspace_mid_x;
      dx = std::min(0, std::max(dx, workspace_size_.width() - overview_width));
    }

    transform.SetTranslateX(dx);
    transform.SetTranslateY(workspace_size_.height() *  (1.0f - scale) / 2);
  } else if (active_workspace_) {
    transform.SetTranslateX(-active_workspace_->bounds().x());
  }

  ui::LayerAnimator::ScopedSettings settings(viewport_->layer()->GetAnimator());
  viewport_->layer()->SetTransform(transform);
}

void WorkspaceManager::RotateWindows(aura::Window* source,
                                     aura::Window* target) {
  DCHECK(source);
  DCHECK(target);
  int source_ws_index = GetWorkspaceIndexContaining(source);
  int target_ws_index = GetWorkspaceIndexContaining(target);
  DCHECK(source_ws_index >= 0);
  DCHECK(target_ws_index >= 0);
  if (source_ws_index == target_ws_index) {
    workspaces_[source_ws_index]->RotateWindows(source, target);
  } else {
    aura::Window* insert = source;
    if (source_ws_index < target_ws_index) {
      for (int i = target_ws_index; i >= source_ws_index; --i) {
        insert = workspaces_[i]->ShiftWindows(
            insert, source, target, Workspace::SHIFT_TO_LEFT);
        // |target| can only be in the 1st workspace.
        target = NULL;
      }
    } else {
      for (int i = target_ws_index; i <= source_ws_index; ++i) {
        insert = workspaces_[i]->ShiftWindows(
            insert, source, target, Workspace::SHIFT_TO_RIGHT);
        // |target| can only be in the 1st workspace.
        target = NULL;
      }
    }
  }
}

void WorkspaceManager::SetWorkspaceSize(const gfx::Size& workspace_size) {
  if (workspace_size == workspace_size_)
    return;
  workspace_size_ = workspace_size;
  LayoutWorkspaces();
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceManager, private:

void WorkspaceManager::AddWorkspace(Workspace* workspace) {
  Workspaces::iterator i = std::find(workspaces_.begin(),
                                     workspaces_.end(),
                                     workspace);
  DCHECK(i == workspaces_.end());
  workspaces_.push_back(workspace);
}

void WorkspaceManager::RemoveWorkspace(Workspace* workspace) {
  Workspaces::iterator i = std::find(workspaces_.begin(),
                                     workspaces_.end(),
                                     workspace);
  DCHECK(i != workspaces_.end());
  if (workspace == active_workspace_)
    active_workspace_ = NULL;
  workspaces_.erase(i);
  LayoutWorkspaces();
}

void WorkspaceManager::SetActiveWorkspace(Workspace* workspace) {
  DCHECK(std::find(workspaces_.begin(), workspaces_.end(),
                   workspace) != workspaces_.end());
  active_workspace_ = workspace;

  is_overview_ = false;
  UpdateViewport();
}

gfx::Rect WorkspaceManager::GetWorkAreaBounds(
    const gfx::Rect& workspace_bounds) {
  gfx::Rect bounds = workspace_bounds;
  bounds.Inset(
      aura::Desktop::GetInstance()->screen()->work_area_insets());
  return bounds;
}

// Returns the index of the workspace that contains the |window|.
int WorkspaceManager::GetWorkspaceIndexContaining(aura::Window* window) const {
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end();
       ++i) {
    if ((*i)->Contains(window))
      return i - workspaces_.begin();
  }
  return -1;
}

void WorkspaceManager::UpdateViewport() {
  int num_workspaces = std::max(1, static_cast<int>(workspaces_.size()));
  int total_width = workspace_size_.width() * num_workspaces +
      kWorkspaceHorizontalMargin * (num_workspaces - 1);
  gfx::Rect bounds(0, 0, total_width, workspace_size_.height());

  if (viewport_->GetTargetBounds() != bounds)
    viewport_->SetBounds(bounds);

  // Move to active workspace.
  if (active_workspace_) {
    ui::Transform transform;
    transform.SetTranslateX(-active_workspace_->bounds().x());
    ui::LayerAnimator::ScopedSettings settings(
        viewport_->layer()->GetAnimator());
    viewport_->SetTransform(transform);
  }
}

}  // namespace internal
}  // namespace aura_shell
