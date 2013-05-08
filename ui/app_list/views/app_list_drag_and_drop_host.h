// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_
#define UI_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_

#include <string>

namespace gfx {
class Point;
}  // namespace gfx

namespace app_list {

// This class will get used by the AppListView to drag and drop Application
// shortcuts onto another host (the launcher).
class ApplicationDragAndDropHost {
 public:
  // A drag operation could get started. The recipient has to return true if
  // he wants to take it - e.g. |location_in_screen_poordinates| is over a
  // target area. The passed |app_id| identifies the application which should
  // get dropped.
  virtual bool StartDrag(const std::string& app_id,
                         const gfx::Point& location_in_screen_coordinates) = 0;

  // This gets only called when the |StartDrag| function returned true and it
  // dispatches the mouse coordinate change accordingly. When the function
  // returns false it requests that the operation be aborted since the event
  // location is out of bounds.
  virtual bool Drag(const gfx::Point& location_in_screen_coordinates) = 0;

  // Once |StartDrag| returned true, this function is guaranteed to be called
  // when the mouse / touch events stop. If |cancel| is set, the drag operation
  // was aborted, otherwise the change should be kept.
  virtual void EndDrag(bool cancel) = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_
