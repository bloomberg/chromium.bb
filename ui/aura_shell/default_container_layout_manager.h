// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
#define UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
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
namespace internal {

class ShowStateController;
class WorkspaceManager;

// LayoutManager for the default window container.
class AURA_SHELL_EXPORT DefaultContainerLayoutManager
    : public aura::LayoutManager {
 public:
  explicit DefaultContainerLayoutManager(WorkspaceManager* workspace_manager);
  virtual ~DefaultContainerLayoutManager();

  // Returns the workspace manager for this container.
  WorkspaceManager* workspace_manager() {
    return workspace_manager_;
  }

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
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;
 private:
  // Owned by WorkspaceController.
  WorkspaceManager* workspace_manager_;

  scoped_ptr<ShowStateController> show_state_controller_;

  DISALLOW_COPY_AND_ASSIGN(DefaultContainerLayoutManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
