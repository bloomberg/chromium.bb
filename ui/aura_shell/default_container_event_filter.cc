// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/default_container_event_filter.h"

#include "ui/aura/event.h"
#include "ui/aura/hit_test.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/default_container_layout_manager.h"

namespace aura_shell {
namespace internal {

DefaultContainerEventFilter::DefaultContainerEventFilter(aura::Window* owner)
    : ToplevelWindowEventFilter(owner),
      drag_state_(DRAG_NONE) {
}

DefaultContainerEventFilter::~DefaultContainerEventFilter() {
}

bool DefaultContainerEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                      aura::MouseEvent* event) {
  DefaultContainerLayoutManager* layout_manager =
      static_cast<DefaultContainerLayoutManager*>(owner()->layout_manager());
  DCHECK(layout_manager);

  // Notify layout manager that drag event may move/resize the target wnidow.
  if (event->type() == ui::ET_MOUSE_DRAGGED && drag_state_ == DRAG_NONE)
    layout_manager->PrepareForMoveOrResize(target, event);

  bool handled = ToplevelWindowEventFilter::PreHandleMouseEvent(target, event);

  switch (event->type()) {
    case ui::ET_MOUSE_DRAGGED:
      // Cancel move/resize if the event wasn't handled, or
      // drag_state_ didn't move to MOVE or RESIZE.
      if (!handled || (drag_state_ == DRAG_NONE && !UpdateDragState()))
        layout_manager->CancelMoveOrResize(target, event);
      break;
    case ui::ET_MOUSE_RELEASED:
      if (drag_state_ == DRAG_MOVE)
        layout_manager->EndMove(target, event);
      else if (drag_state_ == DRAG_RESIZE)
        layout_manager->EndResize(target, event);

      drag_state_ = DRAG_NONE;
      break;
    default:
      break;
  }
  return handled;
}

bool DefaultContainerEventFilter::UpdateDragState() {
  DCHECK_EQ(DRAG_NONE, drag_state_);
  switch (window_component()) {
    case HTCAPTION:
      drag_state_ = DRAG_MOVE;
      break;
    case HTTOP:
    case HTTOPRIGHT:
    case HTRIGHT:
    case HTBOTTOMRIGHT:
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTLEFT:
    case HTTOPLEFT:
    case HTGROWBOX:
      drag_state_ = DRAG_RESIZE;
      break;
    default:
      return false;
  }
  return true;
}

}  // internal
}  // aura_shell
