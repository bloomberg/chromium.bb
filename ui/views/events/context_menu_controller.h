// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENTS_CONTEXT_MENU_CONTROLLER_H_
#define UI_VIEWS_EVENTS_CONTEXT_MENU_CONTROLLER_H_
#pragma once

namespace gfx {
class Point;
}

namespace ui {

class View;

// ContextMenuController is responsible for showing the context menu for a
// View. To use a ContextMenuController invoke SetContextMenuController on a
// View. When the appropriate user gesture occurs ShowContextMenu is invoked
// on the ContextMenuController.
//
// Setting a ContextMenuController on a View makes the View process mouse
// events.
//
// It is up to subclasses that do their own mouse processing to invoke
// the appropriate ContextMenuController method, typically by invoking super's
// implementation for mouse processing.
//
class ContextMenuController {
 public:
  // Invoked to show the context menu for the source view. If |is_mouse_gesture|
  // is true, |point| is the location of the mouse. If |is_mouse_gesture| is
  // false, this method was not invoked by a mouse gesture and |point| is the
  // recommended location to show the menu at.
  //
  // |point| is in screen coordinates.
  virtual void ShowContextMenu(View* source,
                               const gfx::Point& point,
                               bool is_mouse_gesture) = 0;

 protected:
  virtual ~ContextMenuController() {}
};

}  // namespace ui

#endif  // UI_VIEWS_EVENTS_CONTEXT_MENU_CONTROLLER_H_
