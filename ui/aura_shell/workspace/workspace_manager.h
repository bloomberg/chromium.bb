// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_
#define UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_

#include <vector>

#include "base/basictypes.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/gfx/size.h"

namespace aura {
class Window;
}

namespace aura_shell {
class Workspace;

// WorkspaceManager manages multiple workspaces in the desktop.
class AURA_SHELL_EXPORT WorkspaceManager {
 public:
  explicit WorkspaceManager();
  virtual ~WorkspaceManager();

  // Create new workspace. Workspace objects are managed by
  // this WorkspaceManager. Deleting workspace will automatically
  // remove the workspace from the workspace_manager.
  Workspace* CreateWorkspace();

  // Returns the active workspace.
  Workspace* GetActiveWorkspace() const;

  // Returns the workspace that contanis the |window|.
  Workspace* FindBy(aura::Window* window) const;

  // Sets the bounds of all workspaces.
  void Layout();

  // Returns the total width of all workspaces.
  int GetTotalWidth() const;

  // Sets/gets the size of the workspace.
  void set_workspace_size(const gfx::Size& size) { workspace_size_ = size; }
  const gfx::Size& workspace_size() const { return workspace_size_; }

 private:
  friend class Workspace;

  void AddWorkspace(Workspace* workspace);
  void RemoveWorkspace(Workspace* workspace);

  // Sets the active workspace.
  void SetActiveWorkspace(Workspace* workspace);

  gfx::Size workspace_size_;

  Workspace* active_workspace_;

  std::vector<Workspace*> workspaces_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManager);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_
