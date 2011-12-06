// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/status_area_layout_manager.h"

#include "base/auto_reset.h"
#include "ui/aura_shell/shelf_layout_manager.h"

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, public:

StatusAreaLayoutManager::StatusAreaLayoutManager(ShelfLayoutManager* shelf)
    : in_layout_(false),
      shelf_(shelf) {
}

StatusAreaLayoutManager::~StatusAreaLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, aura::LayoutManager implementation:

void StatusAreaLayoutManager::OnWindowResized() {
  LayoutStatusArea();
}

void StatusAreaLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void StatusAreaLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void StatusAreaLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child, bool visible) {
}

void StatusAreaLayoutManager::SetChildBounds(
    aura::Window* child, const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
  if (!in_layout_)
    LayoutStatusArea();
}

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, private:

void StatusAreaLayoutManager::LayoutStatusArea() {
  // Shelf layout manager may be already doing layout.
  if (shelf_->in_layout())
    return;

  AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
  shelf_->LayoutShelf();
}

}  // internal
}  // aura_shell
