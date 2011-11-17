// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WORKSPACE_CONTROLLER_H_
#define UI_AURA_SHELL_WORKSPACE_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/desktop_observer.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/aura_shell/launcher/launcher_model_observer.h"
#include "ui/aura_shell/workspace/workspace_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Size;
}

namespace aura_shell {
class LauncherModel;

namespace internal {

class WorkspaceManager;

// WorkspaceControlls owns a WorkspaceManager. WorkspaceControlls bridges
// events From DesktopObserver translating them to WorkspaceManager, and
// a move event between Laucher and Workspace.
class AURA_SHELL_EXPORT WorkspaceController :
      public aura::DesktopObserver,
      public aura_shell::internal::WorkspaceObserver,
      public aura_shell::LauncherModelObserver {
 public:
  explicit WorkspaceController(aura::Window* workspace_viewport);
  virtual ~WorkspaceController();

  void ToggleOverview();

  void SetLauncherModel(LauncherModel* launcher_model);

  // Returns the workspace manager that this controler owns.
  WorkspaceManager* workspace_manager() {
    return workspace_manager_.get();
  }

  // DesktopObserver overrides:
  virtual void OnDesktopResized(const gfx::Size& new_size) OVERRIDE;
  virtual void OnActiveWindowChanged(aura::Window* active) OVERRIDE;

  // WorkspaceObserver overrides:
  virtual void WindowMoved(WorkspaceManager* manager,
                           aura::Window* source,
                           aura::Window* target) OVERRIDE;
  virtual void ActiveWorkspaceChanged(WorkspaceManager* manager,
                                      Workspace* old) OVERRIDE;

  // Invoked after an item has been added to the model.
  virtual void LauncherItemAdded(int index) OVERRIDE;
  virtual void LauncherItemRemoved(int index) OVERRIDE;
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE;
  virtual void LauncherItemImagesChanged(int index) OVERRIDE;

 private:
  scoped_ptr<WorkspaceManager> workspace_manager_;

  // Owned by Launcher.
  LauncherModel* launcher_model_;

  // True while the controller is moving window either on workspace or launcher.
  // Used to prevent infinite recursive call between the workspace and launcher.
  bool ignore_move_event_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceController);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WORKSPACE_CONTROLLER_H_
