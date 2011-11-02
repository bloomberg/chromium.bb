// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_
#define UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
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
namespace internal {
class Workspace;
class WorkspaceObserver;

// WorkspaceManager manages multiple workspaces in the desktop.
class AURA_SHELL_EXPORT WorkspaceManager {
 public:
  explicit WorkspaceManager(aura::Window* viewport);
  virtual ~WorkspaceManager();

  // Returns the viewport window this WorkspaceManager layouts
  // workspaces on.
  aura::Window* viewport() { return viewport_; }

  // Create new workspace. Workspace objects are managed by
  // this WorkspaceManager. Deleting workspace will automatically
  // remove the workspace from the workspace_manager.
  Workspace* CreateWorkspace();

  // Returns the active workspace.
  Workspace* GetActiveWorkspace() const;

  // Returns the workspace that contanis the |window|.
  Workspace* FindBy(aura::Window* window) const;

  // Returns the window for rotate operation based on the |location|.
  aura::Window* FindRotateWindowForLocation(const gfx::Point& location);

  // Sets the bounds of all workspaces.
  void LayoutWorkspaces();

  // Returns the bounds in which a window can be moved/resized.
  gfx::Rect GetDragAreaBounds();

  // Turn on/off overview mode.
  void SetOverview(bool overview);
  bool is_overview() const { return is_overview_; }

  // Rotate windows by moving |source| window to the position of |target|.
  void RotateWindows(aura::Window* source, aura::Window* target);

  // Sets the size of a single workspace (all workspaces have the same size).
  void SetWorkspaceSize(const gfx::Size& workspace_size);

  // Adds/Removes workspace observer.
  void AddObserver(WorkspaceObserver* observer);
  void RemoveObserver(WorkspaceObserver* observer);

  // Returns true if this workspace manager is laying out windows.
  // When true, LayoutManager must give windows their requested bounds.
  bool layout_in_progress() const { return layout_in_progress_; }

  // Sets the |layout_in_progress_| flag.
  void set_layout_in_progress(bool layout_in_progress) {
    layout_in_progress_ = layout_in_progress;
  }

 private:
  friend class Workspace;

  void AddWorkspace(Workspace* workspace);
  void RemoveWorkspace(Workspace* workspace);

  // Sets the active workspace.
  void SetActiveWorkspace(Workspace* workspace);

  // Returns the bounds of the work are given |workspace_bounds|.
  gfx::Rect GetWorkAreaBounds(const gfx::Rect& workspace_bounds);

  // Returns the index of the workspace that contains the |window|.
  int GetWorkspaceIndexContaining(aura::Window* window) const;

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

  // True if this layout manager is laying out windows.
  bool layout_in_progress_;

  ObserverList<WorkspaceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WORKSPACE_WORKSPACE_MANAGER_H_
