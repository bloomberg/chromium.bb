// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/toplevel_layout_manager.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/property_util.h"
#include "ui/aura_shell/shelf_layout_controller.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace aura_shell {
namespace internal {

ToplevelLayoutManager::ToplevelLayoutManager() : shelf_(NULL) {
}

ToplevelLayoutManager::~ToplevelLayoutManager() {
  for (Windows::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
}

void ToplevelLayoutManager::OnWindowResized() {
}

void ToplevelLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  windows_.insert(child);
  child->AddObserver(this);
  if (child->GetProperty(aura::kShowStateKey))
    WindowStateChanged(child);
}

void ToplevelLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  UpdateShelfVisibility();
}

void ToplevelLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                           bool visibile) {
  UpdateShelfVisibility();
}

void ToplevelLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

void ToplevelLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                    const char* name,
                                                    void* old) {
  if (name == aura::kShowStateKey)
    WindowStateChanged(window);
}

void ToplevelLayoutManager::WindowStateChanged(aura::Window* window) {
  switch (window->GetIntProperty(aura::kShowStateKey)) {
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBounds(window);
      window->SetProperty(aura::kRestoreBoundsKey, NULL);
      if (restore)
        window->SetBounds(*restore);
      delete restore;
      break;
    }

    case ui::SHOW_STATE_MAXIMIZED:
      SetRestoreBoundsIfNotSet(window);
      window->SetBounds(gfx::Screen::GetMonitorWorkAreaNearestWindow(window));
      break;

    case ui::SHOW_STATE_FULLSCREEN:
      SetRestoreBoundsIfNotSet(window);
      window->SetBounds(gfx::Screen::GetMonitorAreaNearestWindow(window));
      break;

    default:
      break;
  }

  UpdateShelfVisibility();
}

void ToplevelLayoutManager::UpdateShelfVisibility() {
  if (!shelf_)
    return;

  bool has_fullscreen_window = false;
  for (Windows::const_iterator i = windows_.begin(); i != windows_.end(); ++i) {
    if ((*i)->GetIntProperty(aura::kShowStateKey) ==
        ui::SHOW_STATE_FULLSCREEN) {
      has_fullscreen_window = true;
      break;
    }
  }
  shelf_->SetVisible(!has_fullscreen_window);
}

}  // namespace internal
}  // namespace aura_shell
