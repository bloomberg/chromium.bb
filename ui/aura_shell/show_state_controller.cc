// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/show_state_controller.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/property_util.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/base/ui_base_types.h"

namespace aura_shell {
namespace internal {

ShowStateController::ShowStateController(
    WorkspaceManager* workspace_manager)
    : workspace_manager_(workspace_manager) {
}

ShowStateController::~ShowStateController() {
}

void ShowStateController::OnWindowPropertyChanged(aura::Window* window,
                                                  const char* name,
                                                  void* old) {
  if (name != aura::kShowStateKey)
    return;
  if (window->GetIntProperty(name) == ui::SHOW_STATE_NORMAL) {
    // Restore the size of window first, then let Workspace layout the window.
    const gfx::Rect* restore = GetRestoreBounds(window);
    window->SetProperty(aura::kRestoreBoundsKey, NULL);
    if (restore)
      window->SetBounds(*restore);
    delete restore;
  } else if (old == reinterpret_cast<void*>(ui::SHOW_STATE_NORMAL)) {
    // Store the restore bounds only if previous state is normal.
    DCHECK(window->GetProperty(aura::kRestoreBoundsKey) == NULL);
    SetRestoreBounds(window, window->GetTargetBounds());
  }

  workspace_manager_->FindBy(window)->Layout(window);
}

}  // namespace internal
}  // namespace aura_shell
