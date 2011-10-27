// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
#define UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class MouseEvent;
class Window;
}

namespace gfx {
class Rect;
}

namespace aura_shell {
class WorkspaceManager;

namespace internal {

// LayoutManager for the default window container.
class AURA_SHELL_EXPORT DefaultContainerLayoutManager
    : public aura::LayoutManager {
 public:
  DefaultContainerLayoutManager(
      aura::Window* owner, WorkspaceManager* workspace_manager);
  virtual ~DefaultContainerLayoutManager();

  // Invoked when a window receives drag event.
  void PrepareForMoveOrResize(aura::Window* drag, aura::MouseEvent* event);

  // Invoked when a drag event didn't start any drag operation.
  void CancelMoveOrResize(aura::Window* drag, aura::MouseEvent* event);

  // Invoked when a drag event moved the |window|.
  void ProcessMove(aura::Window* window, aura::MouseEvent* event);

  // Invoked when a user finished moving window.
  void EndMove(aura::Window* drag, aura::MouseEvent* evnet);

  // Invoked when a user finished resizing window.
  void EndResize(aura::Window* drag, aura::MouseEvent* evnet);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAdded(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void CalculateBoundsForChild(aura::Window* child,
                                       gfx::Rect* requested_bounds) OVERRIDE;
 private:
  aura::Window* owner_;

  WorkspaceManager* workspace_manager_;

  // A window that are currently moved or resized. Used to put
  // different constraints on drag window.
  aura::Window* drag_window_;

  // A flag to control layout behavior. This is set to true while
  // workspace manager is laying out children and LayoutManager
  // ignores bounds check.
  bool ignore_calculate_bounds_;

  DISALLOW_COPY_AND_ASSIGN(DefaultContainerLayoutManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
