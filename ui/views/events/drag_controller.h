// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENTS_DRAG_CONTROLLER_H_
#define UI_VIEWS_EVENTS_DRAG_CONTROLLER_H_
#pragma once

namespace ui {

// DragController is responsible for writing drag data for a View, as well as
// supplying the supported drag operations without having to subclass View.
class DragController {
 public:
  // Writes the data for the drag.
  virtual void WriteDragData(View* sender,
                             const gfx::Point& press_pt,
                             OSExchangeData* data) = 0;

  // Returns the supported drag operations (see DragDropTypes for possible
  // values). A drag is only started if this returns a non-zero value.
  virtual int GetDragOperations(View* sender, const gfx::Point& p) = 0;

  // Returns true if a drag operation can be started.
  // |press_pt| represents the coordinates where the mouse was initially
  // pressed down. |p| is the current mouse coordinates.
  virtual bool CanStartDrag(View* sender,
                            const gfx::Point& press_pt,
                            const gfx::Point& p) = 0;

 protected:
  virtual ~DragController() {}
};

}  // namespace ui

#endif  // UI_VIEWS_EVENTS_DRAG_CONTROLLER_H_
