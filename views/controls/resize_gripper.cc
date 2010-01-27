// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/resize_gripper.h"

#include "base/logging.h"
#include "app/resource_bundle.h"
#include "grit/app_resources.h"

namespace views {

const char ResizeGripper::kViewClassName[] = "views/ResizeGripper";

#if defined(OS_WIN)
static HCURSOR g_resize_cursor = NULL;
#endif

////////////////////////////////////////////////////////////////////////////////
// ResizeGripper

ResizeGripper::ResizeGripper(ResizeGripperDelegate* delegate)
    : delegate_(delegate), initial_position_(0) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  SkBitmap* gripper_image = rb.GetBitmapNamed(IDR_RESIZE_GRIPPER);
  SetImage(gripper_image);
}

ResizeGripper::~ResizeGripper() {
}

std::string ResizeGripper::GetClassName() const {
  return kViewClassName;
}

gfx::NativeCursor ResizeGripper::GetCursorForPoint(Event::EventType event_type,
                                                   int x, int y) {
  if (!enabled_)
    return NULL;
#if defined(OS_WIN)
  if (!g_resize_cursor)
    g_resize_cursor = LoadCursor(NULL, IDC_SIZEWE);
  return g_resize_cursor;
#elif defined(OS_LINUX)
  return gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW);
#endif
}

bool ResizeGripper::OnMousePressed(const views::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;

  // The resize gripper obviously will move once you start dragging so we need
  // to convert coordinates to screen coordinates so that we don't loose our
  // bearings.
  gfx::Point point(event.x(), 0);
  View::ConvertPointToScreen(this, &point);
  initial_position_ = point.x();

  return true;
}

bool ResizeGripper::OnMouseDragged(const views::MouseEvent& event) {
  if (!event.IsLeftMouseButton())
    return false;

  ReportResizeAmount(event.x(), false);
  return true;
}

void ResizeGripper::OnMouseReleased(const views::MouseEvent& event,
                                    bool canceled) {
  if (canceled)
    ReportResizeAmount(initial_position_, true);
  else
    ReportResizeAmount(event.x(), true);
}

void ResizeGripper::ReportResizeAmount(int resize_amount, bool last_update) {
  gfx::Point point(resize_amount, 0);
  View::ConvertPointToScreen(this, &point);
  resize_amount = point.x() - initial_position_;

  if (UILayoutIsRightToLeft())
    resize_amount = -1 * resize_amount;
  delegate_->OnResize(resize_amount, last_update);
}

}  // namespace views
