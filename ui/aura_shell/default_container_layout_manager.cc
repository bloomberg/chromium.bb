// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/default_container_layout_manager.h"

#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window_types.h"
#include "ui/base/view_prop.h"
#include "ui/gfx/rect.h"
#include "views/widget/native_widget_aura.h"

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// DefaultContainerLayoutManager, public:

DefaultContainerLayoutManager::DefaultContainerLayoutManager(
    aura::Window* owner)
    : owner_(owner) {
}

DefaultContainerLayoutManager::~DefaultContainerLayoutManager() {}

////////////////////////////////////////////////////////////////////////////////
// DefaultContainerLayoutManager, aura::LayoutManager implementation:

void DefaultContainerLayoutManager::OnWindowResized() {
  aura::Window::Windows::const_iterator i = owner_->children().begin();
  // Use SetBounds because window may be maximized or fullscreen.
  for (; i != owner_->children().end(); ++i) {
    aura::Window* w = *i;
    if (w->show_state() == ui::SHOW_STATE_MAXIMIZED)
      w->Maximize();
    else if (w->show_state() == ui::SHOW_STATE_FULLSCREEN)
      w->Fullscreen();
    else
      w->SetBounds(w->bounds());
  }
  NOTIMPLEMENTED();
}

void DefaultContainerLayoutManager::OnWindowAdded(aura::Window* child) {
  child->SetBounds(child->bounds());
  NOTIMPLEMENTED();
}

void DefaultContainerLayoutManager::OnWillRemoveWindow(aura::Window* child) {
  NOTIMPLEMENTED();
}

void DefaultContainerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* window, bool visibile) {
  NOTIMPLEMENTED();
}

void DefaultContainerLayoutManager::CalculateBoundsForChild(
    aura::Window* child, gfx::Rect* requested_bounds) {
  intptr_t type = reinterpret_cast<intptr_t>(
      ui::ViewProp::GetValue(child, views::NativeWidgetAura::kWindowTypeKey));
  // DCLM controls windows with a frame.
  if (type != views::Widget::InitParams::TYPE_WINDOW)
    return;
  // TODO(oshima): Figure out bounds for default windows.
  gfx::Rect viewport_bounds = owner_->bounds();

  // A window can still be placed outside of the screen.
  requested_bounds->SetRect(
      requested_bounds->x(),
      viewport_bounds.y(),
      std::min(requested_bounds->width(), viewport_bounds.width()),
      std::min(requested_bounds->height(), viewport_bounds.height()));
}

}  // namespace internal
}  // namespace aura_shell
