// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/toplevel_layout_manager.h"

#include "ui/aura/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/property_util.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace aura_shell {
namespace internal {

ToplevelLayoutManager::ToplevelLayoutManager() {
}

ToplevelLayoutManager::~ToplevelLayoutManager() {
  for (Windows::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
}

void ToplevelLayoutManager::OnWindowResized() {
}

void ToplevelLayoutManager::OnWindowAdded(aura::Window* child) {
  windows_.insert(child);
  child->AddObserver(this);
  if (child->GetProperty(aura::kShowStateKey))
    WindowStateChanged(child);
}

void ToplevelLayoutManager::OnWillRemoveWindow(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
}

void ToplevelLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                           bool visibile) {
}

void ToplevelLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

void ToplevelLayoutManager::OnPropertyChanged(aura::Window* window,
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
      // TODO: need to hide the launcher.
      break;

    default:
      break;
  }
}

}  // namespace internal
}  // namespace aura_shell
