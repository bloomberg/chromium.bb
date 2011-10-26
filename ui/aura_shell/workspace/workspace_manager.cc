// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ui/aura_shell/workspace/workspace_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/aura_shell/workspace/workspace.h"

namespace {

// The horizontal margein between workspaces in pixels.
const int kWorkspaceHorizontalMargin = 50;
}

namespace aura_shell {

////////////////////////////////////////////////////////////////////////////////
// WindowManager, public:

WorkspaceManager::WorkspaceManager()
    : active_workspace_(NULL) {
}

WorkspaceManager::~WorkspaceManager() {
  std::vector<Workspace*> copy_to_delete(workspaces_);
  STLDeleteElements(&copy_to_delete);
}

Workspace* WorkspaceManager::CreateWorkspace() {
  return new Workspace(this);
}

Workspace* WorkspaceManager::GetActiveWorkspace() const {
  return active_workspace_;
}

Workspace* WorkspaceManager::FindBy(aura::Window* window) const {
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end();
       i++) {
    if ((*i)->Contains(window))
      return *i;
  }
  return NULL;
}

void WorkspaceManager::Layout() {
  gfx::Rect bounds(workspace_size_);
  int x = 0;
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end();
       i++) {
    Workspace* workspace = *i;
    bounds.set_x(x);
    workspace->SetBounds(bounds);
    x += workspace_size_.width() + kWorkspaceHorizontalMargin;
  }
}

int WorkspaceManager::GetTotalWidth() const {
  return !workspaces_.size() ? 0 :
      workspace_size().width() * workspaces_.size() +
      kWorkspaceHorizontalMargin * (workspaces_.size() - 1);
}

////////////////////////////////////////////////////////////////////////////////
// WindowManager, private:

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
}

void WorkspaceManager::SetActiveWorkspace(Workspace* workspace) {
  DCHECK(std::find(workspaces_.begin(),
                   workspaces_.end(),
                   workspace)
         != workspaces_.end());
  active_workspace_ = workspace;
}

}  // namespace aura_shell
