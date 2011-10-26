// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_
#define UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/aura/desktop_observer.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/size.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
class Rect;
}

namespace aura_shell {
class Workspace;

// WorkspaceManager manages multiple workspaces in the desktop.
class AURA_SHELL_EXPORT WorkspaceManager : public aura::DesktopObserver {
 public:
  explicit WorkspaceManager(aura::Window* viewport);
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
  void LayoutWorkspaces();

  // Returns the bounds in which a window can be moved/resized.
  gfx::Rect GetDragAreaBounds();

  // Turn on/off overview mode.
  void SetOverview(bool overview);
  bool is_overview() const { return is_overview_; }

  // Overridden from aura::DesktopObserver:
  virtual void OnDesktopResized(const gfx::Size& new_size) OVERRIDE;
  virtual void OnActiveWindowChanged(aura::Window* active) OVERRIDE;

 private:
  friend class Workspace;
  FRIEND_TEST_ALL_PREFIXES(WorkspaceManagerTest, Overview);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceManagerTest, LayoutWorkspaces);

  void AddWorkspace(Workspace* workspace);
  void RemoveWorkspace(Workspace* workspace);

  // Sets the active workspace.
  void SetActiveWorkspace(Workspace* workspace);

  // Returns the bounds of the work are given |workspace_bounds|.
  gfx::Rect GetWorkAreaBounds(const gfx::Rect& workspace_bounds);

  // Update viewport size and move to the active workspace.
  void UpdateViewport();

  aura::Window* viewport_;

  Workspace* active_workspace_;

  std::vector<Workspace*> workspaces_;

  // The size of a single workspace. This is generally the same as the size of
  // monitor.
  gfx::Size workspace_size_;

  // True if the workspace manager is in overview mode.
  bool is_overview_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManager);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_
