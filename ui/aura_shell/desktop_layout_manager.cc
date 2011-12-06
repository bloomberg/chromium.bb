// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/desktop_layout_manager.h"

#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// DesktopLayoutManager, public:

DesktopLayoutManager::DesktopLayoutManager(aura::Window* owner)
    : owner_(owner),
      background_widget_(NULL) {
}

DesktopLayoutManager::~DesktopLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopLayoutManager, aura::LayoutManager implementation:

void DesktopLayoutManager::OnWindowResized() {
  gfx::Rect fullscreen_bounds =
      gfx::Rect(owner_->bounds().width(), owner_->bounds().height());

  aura::Window::Windows::const_iterator i;
  for (i = owner_->children().begin(); i != owner_->children().end(); ++i)
    (*i)->SetBounds(fullscreen_bounds);

  background_widget_->SetBounds(fullscreen_bounds);
}

void DesktopLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void DesktopLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}

void DesktopLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                          bool visible) {
}

void DesktopLayoutManager::SetChildBounds(aura::Window* child,
                                          const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

}  // namespace internal
}  // namespace aura_shell
