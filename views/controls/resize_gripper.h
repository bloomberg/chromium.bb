// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_RESIZE_GRIPPER_H_
#define VIEWS_CONTROLS_RESIZE_GRIPPER_H_

#include <string>

#include "views/controls/image_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// A simple resize gripper (two vertical bars).
//
////////////////////////////////////////////////////////////////////////////////
class ResizeGripper : public ImageView {
 public:
  //////////////////////////////////////////////////////////////////////////////
  //
  // The interface needed for getting notified about the resize event.
  //
  //////////////////////////////////////////////////////////////////////////////
  class ResizeGripperDelegate {
  public:
    // OnResize is sent when resizing is detected. |resize_amount| specifies the
    // number of pixels that the user wants to resize by, and can be negative or
    // positive (depending on direction of dragging and flips according to
    // locale directionality: dragging to the left in LTR locales gives negative
    // |resize_amount| but positive amount for RTL). |done_resizing| is true if
    // the user has released the mouse.
    virtual void OnResize(int resize_amount, bool done_resizing) = 0;
  };

  explicit ResizeGripper(ResizeGripperDelegate* delegate);
  virtual ~ResizeGripper();

  // Overridden from views::View:
  virtual std::string GetClassName() const;
  virtual gfx::NativeCursor GetCursorForPoint(Event::EventType event_type,
                                              int x, int y);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);

  static const char kViewClassName[];

 private:
  // Report the amount the user resized by to the delegate, accounting for
  // directionality.
  void ReportResizeAmount(int resize_amount, bool last_update);

  // The delegate to notify when we have updates.
  ResizeGripperDelegate* delegate_;

  // The mouse position at start (in screen coordinates).
  int initial_position_;

  DISALLOW_COPY_AND_ASSIGN(ResizeGripper);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_RESIZE_GRIPPER_H_
