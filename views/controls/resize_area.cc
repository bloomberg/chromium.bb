// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/resize_area.h"

#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_LINUX)
#include "gfx/gtk_util.h"
#endif

namespace views {

const char ResizeArea::kViewClassName[] = "views/ResizeArea";

#if defined(OS_WIN)
static HCURSOR g_resize_cursor = NULL;
#endif

////////////////////////////////////////////////////////////////////////////////
// ResizeArea

ResizeArea::ResizeArea(ResizeAreaDelegate* delegate)
    : delegate_(delegate),
      initial_position_(0) {
}

ResizeArea::~ResizeArea() {
}

std::string ResizeArea::GetClassName() const {
  return kViewClassName;
}

gfx::NativeCursor ResizeArea::GetCursorForPoint(Event::EventType event_type,
                                                const gfx::Point& p) {
  if (!enabled_)
    return NULL;
#if defined(OS_WIN)
  if (!g_resize_cursor)
    g_resize_cursor = LoadCursor(NULL, IDC_SIZEWE);
  return g_resize_cursor;
#elif defined(OS_LINUX)
  return gfx::GetCursor(GDK_SB_H_DOUBLE_ARROW);
#endif
}

bool ResizeArea::OnMousePressed(const views::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;

  // The resize area obviously will move once you start dragging so we need to
  // convert coordinates to screen coordinates so that we don't lose our
  // bearings.
  gfx::Point point(event.x(), 0);
  View::ConvertPointToScreen(this, &point);
  initial_position_ = point.x();

  return true;
}

bool ResizeArea::OnMouseDragged(const views::MouseEvent& event) {
  if (!event.IsLeftMouseButton())
    return false;

  ReportResizeAmount(event.x(), false);
  return true;
}

void ResizeArea::OnMouseReleased(const views::MouseEvent& event,
                                 bool canceled) {
  ReportResizeAmount(canceled ? initial_position_ : event.x(), true);
}

AccessibilityTypes::Role ResizeArea::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_SEPARATOR;
}

void ResizeArea::ReportResizeAmount(int resize_amount, bool last_update) {
  gfx::Point point(resize_amount, 0);
  View::ConvertPointToScreen(this, &point);
  resize_amount = point.x() - initial_position_;
  delegate_->OnResize(base::i18n::IsRTL() ? -resize_amount : resize_amount,
                      last_update);
}

}  // namespace views
