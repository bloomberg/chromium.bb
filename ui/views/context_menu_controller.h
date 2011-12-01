// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTEXT_MENU_CONTROLLER_H_
#define UI_VIEWS_CONTEXT_MENU_CONTROLLER_H_
#pragma once

#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace views {
class View;

// ContextMenuController is responsible for showing the context menu for a
// View. To use a ContextMenuController invoke set_context_menu_controller on a
// View. When the appropriate user gesture occurs ShowContextMenu is invoked
// on the ContextMenuController.
//
// Setting a ContextMenuController on a view makes the view process mouse
// events.
//
// It is up to subclasses that do their own mouse processing to invoke
// the appropriate ContextMenuController method, typically by invoking super's
// implementation for mouse processing.
class VIEWS_EXPORT ContextMenuController {
 public:
  // Invoked to show the context menu for the source view. If |is_mouse_gesture|
  // is true, |p| is the location of the mouse. If |is_mouse_gesture| is false,
  // this method was not invoked by a mouse gesture and |p| is the recommended
  // location to show the menu at.
  //
  // |p| is in screen coordinates.
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture) = 0;

 protected:
  virtual ~ContextMenuController() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTEXT_MENU_CONTROLLER_H_
