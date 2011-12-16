// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/compact_layout_manager.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/window_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace aura_shell {
namespace internal {

CompactLayoutManager::CompactLayoutManager() {
}

CompactLayoutManager::~CompactLayoutManager() {
  for (Windows::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
}

void CompactLayoutManager::OnWindowResized() {
}

void CompactLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  windows_.insert(child);
  child->AddObserver(this);
  if (child->GetProperty(aura::kShowStateKey))
    UpdateBoundsFromShowState(child);
}

void CompactLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
}

void CompactLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visibile) {
}

void CompactLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  gfx::Rect bounds = requested_bounds;
  // Avoid a janky resize on startup by ensuring the initial bounds fill the
  // screen.
  if (child->GetIntProperty(aura::kShowStateKey) == ui::SHOW_STATE_MAXIMIZED)
    bounds = gfx::Screen::GetPrimaryMonitorBounds();
  SetChildBoundsDirect(child, bounds);
}

void CompactLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                      const char* name,
                                                      void* old) {
  if (name == aura::kShowStateKey)
    UpdateBoundsFromShowState(window);
}

}  // namespace internal
}  // namespace aura_shell
