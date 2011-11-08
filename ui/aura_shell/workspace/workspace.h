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
namespace internal {

class WorkspaceManager;
class WorkspaceTest;

// A workspace is a partial area of the entire desktop (viewport) that
// is visible to the user at a given time. The size of the workspace is
// generally the same as the size of the monitor, and the desktop can
// have multiple workspaces.
// A workspace contains a limited number of windows and the workspace
// manager may create a new workspace if there is not enough room for
// a new window.
class AURA_SHELL_EXPORT Workspace {
 public:
  explicit Workspace(WorkspaceManager* manager);
  virtual ~Workspace();

  // Specifies the direction to shift windows in |ShiftWindows()|.
  enum ShiftDirection {
    SHIFT_TO_RIGHT,
    SHIFT_TO_LEFT
  };

  //  Returns true if this workspace has no windows.
  bool is_empty() const { return windows_.empty(); }

  // Sets/gets bounds of this workspace.
  const gfx::Rect& bounds() { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  // Returns the work area bounds of this workspace in viewport
  // coordinates.
  gfx::Rect GetWorkAreaBounds() const;

  // Adds the |window| at the position after the window |after|.  It
  // inserts at the end if |after| is NULL. Return true if the
  // |window| was successfully added to this workspace, or false if it
  // failed.
  bool AddWindowAfter(aura::Window* window, aura::Window* after);

  // Removes |window| from this workspace.
  void RemoveWindow(aura::Window* window);

  // Return true if this workspace has |window|.
  bool Contains(aura::Window* window) const;

  // Returns a window to rotate to based on |position|.
  aura::Window* FindRotateWindowForLocation(const gfx::Point& position);

  // Rotates the windows by removing |source| and inserting it to the
  // position that |target| was in. It re-layouts windows except for |source|.
  void RotateWindows(aura::Window* source, aura::Window* target);

  // Shift the windows in the workspace by inserting |window| until it
  // reaches |until|. If |direction| is |SHIFT_TO_RIGHT|, |insert| is
  // inserted at the position of |target| or at the beginning if
  // |target| is NULL. If |direction| is |SHIFT_TO_LEFT|, |insert| is
  // inserted after the position of |target|, or at the end if
  // |target| is NULL.  It returns the window that is overflowed by
  // shifting, or NULL if shifting stopped at |until|.
  aura::Window* ShiftWindows(aura::Window* insert,
                             aura::Window* until,
                             aura::Window* target,
                             ShiftDirection direction);

  // Activates this workspace.
  void Activate();

  // Layout windows. The workspace doesn't set bounds on
  // |WorkspaceManager::ignored_window| if it's set. It still uses the window's
  // bounds to calculate bounds for other windows. Moving animation is
  // applied to all windows except for the window specified by |no_animation|
  // and |ignore|.
  void Layout(aura::Window* no_animation);

  // Returns true if the workspace contains a fullscreen window.
  bool ContainsFullscreenWindow() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, WorkspaceBasic);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, RotateWindows);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, ShiftWindowsSingle);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, ShiftWindowsMultiple);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceManagerTest, RotateWindows);

  // Returns the index in layout order of |window| in this workspace.
  int GetIndexOf(aura::Window* window) const;

  // Returns true if the given |window| can be added to this workspace.
  bool CanAdd(aura::Window* window) const;

  // Moves |window| to the given point. It performs animation when
  // |animate| is true.
  void MoveWindowTo(aura::Window* window,
                    const gfx::Point& origin,
                    bool animate);

  // Returns the sum of all window's width.
  int GetTotalWindowsWidth() const;

  // Test only: Changes how may windows workspace can have.
  // Returns the current value so that it can be reverted back to
  // original value.
  static size_t SetMaxWindowsCount(size_t max);

  WorkspaceManager* workspace_manager_;

  gfx::Rect bounds_;

  // Windows in the workspace in layout order.
  std::vector<aura::Window*> windows_;

  DISALLOW_COPY_AND_ASSIGN(Workspace);
};

typedef std::vector<Workspace*> Workspaces;

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WORKSPACE_WORKSPACE_H_
