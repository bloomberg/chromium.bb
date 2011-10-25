// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace/workspace.h"

#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/workspace/workspace_manager.h"

namespace aura_shell {

Workspace::Workspace(WorkspaceManager* manager)
    : workspace_manager_(manager) {
  workspace_manager_->AddWorkspace(this);
}

Workspace::~Workspace() {
  workspace_manager_->RemoveWorkspace(this);
}

void Workspace::SetBounds(const gfx::Rect& bounds) {
  int dx = bounds.x() - bounds_.x();
  bounds_ = bounds;

  for (aura::Window::Windows::iterator i = windows_.begin();
       i != windows_.end();
       i++) {
    aura::Window* window = *i;
    gfx::Rect bounds = window->GetTargetBounds();
    bounds.Offset(dx, 0);
    window->SetBounds(bounds);
  }
}

bool Workspace::AddWindowAfter(aura::Window* window, aura::Window* after) {
  if (!CanAdd(window))
    return false;
  DCHECK(!Contains(window));

  if (!after) {  // insert at the end;
    windows_.push_back(window);
  } else {
    DCHECK(Contains(after));
    aura::Window::Windows::iterator i =
        std::find(windows_.begin(), windows_.end(), after);
    windows_.insert(++i, window);
  }
  return true;
}

void Workspace::RemoveWindow(aura::Window* window) {
  DCHECK(Contains(window));
  windows_.erase(std::find(windows_.begin(), windows_.end(), window));
}

bool Workspace::Contains(aura::Window* window) const {
  return std::find(windows_.begin(), windows_.end(), window) != windows_.end();
}

void Workspace::Activate() {
  workspace_manager_->SetActiveWorkspace(this);
}

int Workspace::GetIndexOf(aura::Window* window) const {
  aura::Window::Windows::const_iterator i =
      std::find(windows_.begin(), windows_.end(), window);
  return i == windows_.end() ? -1 : i - windows_.begin();
}

bool Workspace::CanAdd(aura::Window* window) const {
  // TODO(oshima): This should be based on available space and the
  // size of the |window|.
  NOTIMPLEMENTED();
  return windows_.size() < 2;
}

}  // namespace aura_shell
