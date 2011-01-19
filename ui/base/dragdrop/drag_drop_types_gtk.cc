// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_drop_types.h"

#include <gtk/gtk.h>

namespace ui {

// static
int ui::DragDropTypes::DragOperationToGdkDragAction(int drag_operation) {
  int gdk_drag_action = 0;
  if (drag_operation & DRAG_MOVE)
    gdk_drag_action |= GDK_ACTION_MOVE;
  if (drag_operation & DRAG_COPY)
    gdk_drag_action |= GDK_ACTION_COPY;
  if (drag_operation & DRAG_LINK)
    gdk_drag_action |= GDK_ACTION_LINK;
  return gdk_drag_action;
}

// static
int ui::DragDropTypes::GdkDragActionToDragOperation(int gdk_drag_action) {
  int drag_operation = DRAG_NONE;
  if (gdk_drag_action & GDK_ACTION_COPY)
    drag_operation |= DRAG_COPY;
  if (gdk_drag_action & GDK_ACTION_MOVE)
    drag_operation |= DRAG_MOVE;
  if (gdk_drag_action & GDK_ACTION_LINK)
    drag_operation |= DRAG_LINK;
  return drag_operation;
}

}  // namespace ui
