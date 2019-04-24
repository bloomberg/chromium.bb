// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kDisallowReparentKey, false)

AlwaysOnTopController::AlwaysOnTopController(
    aura::Window* always_on_top_container,
    aura::Window* pip_container)
    : always_on_top_container_(always_on_top_container),
      pip_container_(pip_container) {
  DCHECK_NE(kShellWindowId_DefaultContainer, always_on_top_container_->id());
  DCHECK_NE(kShellWindowId_DefaultContainer, pip_container_->id());
  always_on_top_container_->SetLayoutManager(
      new WorkspaceLayoutManager(always_on_top_container_));
  pip_container_->SetLayoutManager(new WorkspaceLayoutManager(pip_container_));
  // Container should be empty.
  DCHECK(always_on_top_container_->children().empty());
  DCHECK(pip_container_->children().empty());
  always_on_top_container_->AddObserver(this);
  pip_container->AddObserver(this);
}

AlwaysOnTopController::~AlwaysOnTopController() {
  // At this point, all windows should be removed and AlwaysOnTopController
  // will have removed itself as an observer in OnWindowDestroying.
  DCHECK(!always_on_top_container_);
  DCHECK(!pip_container_);
}

aura::Window* AlwaysOnTopController::GetContainer(aura::Window* window) const {
  DCHECK(always_on_top_container_);
  DCHECK(pip_container_);

  if (!window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    return always_on_top_container_->GetRootWindow()->GetChildById(
        kShellWindowId_DefaultContainer);
  }
  if (window->parent() && wm::GetWindowState(window)->IsPip())
    return pip_container_;

  return always_on_top_container_;
}

void AlwaysOnTopController::SetLayoutManagerForTest(
    std::unique_ptr<WorkspaceLayoutManager> layout_manager) {
  always_on_top_container_->SetLayoutManager(layout_manager.release());
}

void AlwaysOnTopController::SetDisallowReparent(aura::Window* window) {
  window->SetProperty(kDisallowReparentKey, true);
}

void AlwaysOnTopController::AddWindow(aura::Window* window) {
  window->AddObserver(this);
  wm::GetWindowState(window)->AddObserver(this);
}

void AlwaysOnTopController::RemoveWindow(aura::Window* window) {
  window->RemoveObserver(this);
  wm::GetWindowState(window)->RemoveObserver(this);
}

void AlwaysOnTopController::ReparentWindow(aura::Window* window) {
  DCHECK(window->type() == aura::client::WINDOW_TYPE_NORMAL ||
         window->type() == aura::client::WINDOW_TYPE_POPUP);
  aura::Window* container = GetContainer(window);
  if (window->parent() != container &&
      !window->GetProperty(ash::kDisallowReparentKey))
    container->AddChild(window);
}

void AlwaysOnTopController::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.old_parent == always_on_top_container_ ||
      params.old_parent == pip_container_) {
    RemoveWindow(params.target);
  }

  if (params.new_parent == always_on_top_container_ ||
      params.new_parent == pip_container_) {
    AddWindow(params.target);
  }
}

void AlwaysOnTopController::OnWindowPropertyChanged(aura::Window* window,
                                                    const void* key,
                                                    intptr_t old) {
  if (window != always_on_top_container_ && window != pip_container_ &&
      key == aura::client::kAlwaysOnTopKey) {
    ReparentWindow(window);
  }
}

void AlwaysOnTopController::OnWindowDestroying(aura::Window* window) {
  if (window == always_on_top_container_) {
    always_on_top_container_->RemoveObserver(this);
    always_on_top_container_ = nullptr;
  } else if (window == pip_container_) {
    pip_container_->RemoveObserver(this);
    pip_container_ = nullptr;
  } else {
    RemoveWindow(window);
  }
}

void AlwaysOnTopController::OnPreWindowStateTypeChange(
    wm::WindowState* window_state,
    mojom::WindowStateType old_type) {
  ReparentWindow(window_state->window());
}

}  // namespace ash
