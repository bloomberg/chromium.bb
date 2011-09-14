// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_manager.h"

#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"

#if !defined(OS_WIN)
#include "ui/aura/hit_test.h"
#endif

namespace aura {

WindowManager::WindowManager(Window* owner)
    : owner_(owner),
      window_component_(HTNOWHERE) {
}

WindowManager::~WindowManager() {
}

bool WindowManager::OnMouseEvent(MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      // TODO(beng): some windows (e.g. disabled ones, tooltips, etc) may not be
      //             focusable.
      owner_->GetFocusManager()->SetFocusedWindow(owner_);
      window_component_ =
          owner_->delegate()->GetNonClientComponent(event->location());
      MoveWindowToFront();
      mouse_down_offset_ = event->location();
      window_location_ = owner_->bounds().origin();
      break;
    case ui::ET_MOUSE_DRAGGED:
      if (window_component_ == HTCAPTION) {
        gfx::Point new_origin(event->location());
        new_origin.Offset(-mouse_down_offset_.x(), -mouse_down_offset_.y());
        new_origin.Offset(owner_->bounds().x(), owner_->bounds().y());
        owner_->SetBounds(gfx::Rect(new_origin, owner_->bounds().size()), 0);
        return true;
      }
      break;
    case ui::ET_MOUSE_RELEASED:
      window_component_ = HTNOWHERE;
      break;
    default:
      break;
  }
  return false;
}

void WindowManager::MoveWindowToFront() {
  Window* parent = owner_->parent();
  Window* child = owner_;
  while (parent) {
    parent->MoveChildToFront(child);
    parent = parent->parent();
    child = child->parent();
  }
}

}  // namespace aura
