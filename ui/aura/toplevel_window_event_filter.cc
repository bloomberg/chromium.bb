// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/toplevel_window_event_filter.h"

#include "ui/aura/cursor.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"

#if !defined(OS_WIN)
#include "ui/aura/hit_test.h"
#endif

namespace aura {

ToplevelWindowEventFilter::ToplevelWindowEventFilter(Window* owner)
    : EventFilter(owner),
      window_component_(HTNOWHERE) {
}

ToplevelWindowEventFilter::~ToplevelWindowEventFilter() {
}

bool ToplevelWindowEventFilter::OnMouseEvent(Window* target,
                                             MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      window_component_ =
          target->delegate()->GetNonClientComponent(event->location());
      UpdateCursorForWindowComponent();
      break;
    case ui::ET_MOUSE_PRESSED:
      MoveWindowToFront(target);
      mouse_down_offset_ = event->location();
      window_location_ = target->bounds().origin();
      if (window_component_ == HTCAPTION)
        return true;
      break;
    case ui::ET_MOUSE_DRAGGED:
      if (window_component_ == HTCAPTION) {
        gfx::Point new_origin(event->location());
        new_origin.Offset(-mouse_down_offset_.x(), -mouse_down_offset_.y());
        new_origin.Offset(target->bounds().x(), target->bounds().y());
        target->SetBounds(gfx::Rect(new_origin, target->bounds().size()), 0);
        return true;
      }
      break;
    case ui::ET_MOUSE_RELEASED:
      window_component_ = HTNOWHERE;
      break;
    default:
      break;
  }
  return EventFilter::OnMouseEvent(target, event);
}

void ToplevelWindowEventFilter::MoveWindowToFront(Window* target) {
  Window* parent = target->parent();
  Window* child = target;
  while (parent) {
    parent->MoveChildToFront(child);
    if (parent == owner())
      break;
    parent = parent->parent();
    child = child->parent();
  }
}

void ToplevelWindowEventFilter::UpdateCursorForWindowComponent() {
  switch (window_component_) {
    case HTLEFT:
    case HTRIGHT:
      Desktop::GetInstance()->SetCursor(CURSOR_SIZE_HORIZONTAL);
      break;
    case HTBOTTOM:
      Desktop::GetInstance()->SetCursor(CURSOR_SIZE_VERTICAL);
      break;
    default:
      Desktop::GetInstance()->SetCursor(CURSOR_POINTER);
      break;
  }
}

}  // namespace aura
