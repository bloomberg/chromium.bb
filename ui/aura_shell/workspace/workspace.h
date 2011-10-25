// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WORKSPACE_WORKSPACE_H_
#define UI_AURA_SHELL_WORKSPACE_WORKSPACE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace aura_shell {
class WorkspaceManager;
class WorkspaceTest;

// A workspace is a partial area of the entire desktop that is visible to
// a user at a given time. The size of workspace is generally same as
// the size of the monitor, and the desktiop can have multiple workspaces.
//  One workspace can contains limited number of windows and
// the workspace manager may create new workspace if there is
// no room for new windowk.
//
// TODO(oshima): Add a work area bounds for workspace. It will be used to
// limit the resize, layout and as bounds for a maximized window.
class AURA_SHELL_EXPORT Workspace {
 public:
  explicit Workspace(WorkspaceManager* manager);
  virtual ~Workspace();

  //  Returns true if this workspace has no windows.
  bool is_empty() const { return windows_.empty(); }

  // Sets/gets bounds of this workspace.
  const gfx::Rect& bounds() { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  // Adds the |window| at the position after the window |after|.  It
  // inserts at the end if |after| is NULL. Return true if the
  // |window| was successfully added to this workspace, or false if it
  // failed.
  bool AddWindowAfter(aura::Window* window, aura::Window* after);

  // Removes the |window| from this workspace.
  void RemoveWindow(aura::Window* window);

  // Return true if this workspace has the |window|.
  bool Contains(aura::Window* window) const;

  // Activates this workspace.
  void Activate();

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, WorkspaceBasic);

  // Returns the index in layout order of the |window| in this workspace.
  int GetIndexOf(aura::Window* window) const;

  // Returns true if the given |window| can be added to this workspace.
  bool CanAdd(aura::Window* window) const;

  WorkspaceManager* workspace_manager_;

  gfx::Rect bounds_;

  // Windows in the workspace in layout order.
  std::vector<aura::Window*> windows_;

  DISALLOW_COPY_AND_ASSIGN(Workspace);
};

typedef std::vector<Workspace*> Workspaces;

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WORKSPACE_WORKSPACE_H_
