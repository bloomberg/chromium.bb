// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WORKSPACE_WORKSPACE_CONTROLLER_H_
#define UI_AURA_SHELL_WORKSPACE_WORKSPACE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/desktop_observer.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Size;
}

namespace aura_shell {
namespace internal {

class DefaultContainerLayoutManager;
class WorkspaceManager;

// WorkspaceControlls owns a WorkspaceManager. WorkspaceControlls bridges
// events From DesktopObserver translating them to WorkspaceManager.
class AURA_SHELL_EXPORT WorkspaceController : public aura::DesktopObserver {
 public:
  explicit WorkspaceController(aura::Window* window);
  virtual ~WorkspaceController();

  void ToggleOverview();

  internal::DefaultContainerLayoutManager* layout_manager() {
    return layout_manager_;
  }

  // DesktopObserver overrides:
  virtual void OnDesktopResized(const gfx::Size& new_size) OVERRIDE;
  virtual void OnActiveWindowChanged(aura::Window* active) OVERRIDE;

 private:
  scoped_ptr<WorkspaceManager> workspace_manager_;

  // This is owned by the window it's installed on.
  internal::DefaultContainerLayoutManager* layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceController);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WORKSPACE_WORKSPACE_CONTROLLER_H_
