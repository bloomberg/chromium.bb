// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/single_split_view.h"

#if defined(TOOLKIT_USES_GTK)
#include <gdk/gdk.h>
#endif

#include "skia/ext/skia_utils_win.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/single_split_view_listener.h"
#include "views/background.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/cursor.h"
#endif

namespace views {

// static
const char SingleSplitView::kViewClassName[] =
    "ui/views/controls/SingleSplitView";

// Size of the divider in pixels.
static const int kDividerSize = 4;

SingleSplitView::SingleSplitView(View* leading,
                                 View* trailing,
                                 Orientation orientation,
                                 SingleSplitViewListener* listener)
    : is_horizontal_(orientation == HORIZONTAL_SPLIT),
      divider_offset_(-1),
      resize_leading_on_bounds_change_(true),
      listener_(listener) {
  AddChildView(leading);
  AddChildView(trailing);
#if defined(OS_WIN)
  set_background(
      views::Background::CreateSolidBackground(
          skia::COLORREFToSkColor(GetSysColor(COLOR_3DFACE))));
#endif
}

void SingleSplitView::Layout() {
  gfx::Rect leading_bounds;
  gfx::Rect trailing_bounds;
  CalculateChildrenBounds(bounds(), &leading_bounds, &trailing_bounds);

  if (has_children()) {
    if (child_at(0)->IsVisible())
      child_at(0)->SetBoundsRect(leading_bounds);
    if (child_count() > 1) {
      if (child_at(1)->IsVisible())
        child_at(1)->SetBoundsRect(trailing_bounds);
    }
  }

  SchedulePaint();

  // Invoke super's implementation so that the children are layed out.
  View::Layout();
}

std::string SingleSplitView::GetClassName() const {
  return kViewClassName;
}

void SingleSplitView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
  state->name = accessible_name_;
}

gfx::Size SingleSplitView::GetPreferredSize() {
  int width = 0;
  int height = 0;
  for (int i = 0; i < 2 && i < child_count(); ++i) {
    View* view = child_at(i);
    gfx::Size pref = view->GetPreferredSize();
    if (is_horizontal_) {
      width += pref.width();
      height = std::max(height, pref.height());
    } else {
      width = std::max(width, pref.width());
      height += pref.height();
    }
  }
  if (is_horizontal_)
    width += kDividerSize;
  else
    height += kDividerSize;
  return gfx::Size(width, height);
}

gfx::NativeCursor SingleSplitView::GetCursor(const MouseEvent& event) {
  if (!IsPointInDivider(event.location()))
    return gfx::kNullCursor;
#if defined(USE_AURA)
  return is_horizontal_ ?
      aura::kCursorEastWestResize : aura::kCursorNorthSouthResize;
#elif defined(OS_WIN)
  static HCURSOR we_resize_cursor = LoadCursor(NULL, IDC_SIZEWE);
  static HCURSOR ns_resize_cursor = LoadCursor(NULL, IDC_SIZENS);
  return is_horizontal_ ? we_resize_cursor : ns_resize_cursor;
#elif defined(TOOLKIT_USES_GTK)
  return gfx::GetCursor(is_horizontal_ ? GDK_SB_H_DOUBLE_ARROW :
                                         GDK_SB_V_DOUBLE_ARROW);
#endif
}

void SingleSplitView::CalculateChildrenBounds(
    const gfx::Rect& bounds,
    gfx::Rect* leading_bounds,
    gfx::Rect* trailing_bounds) const {
  bool is_leading_visible = has_children() && child_at(0)->IsVisible();
  bool is_trailing_visible = child_count() > 1 && child_at(1)->IsVisible();

  if (!is_leading_visible && !is_trailing_visible) {
    *leading_bounds = gfx::Rect();
    *trailing_bounds = gfx::Rect();
    return;
  }

  int divider_at;

  if (!is_trailing_visible) {
    divider_at = GetPrimaryAxisSize(bounds.width(), bounds.height());
  } else if (!is_leading_visible) {
    divider_at = 0;
  } else {
    divider_at =
        CalculateDividerOffset(divider_offset_, this->bounds(), bounds);
    divider_at = NormalizeDividerOffset(divider_at, bounds);
  }

  int divider_size =
      !is_leading_visible || !is_trailing_visible ? 0 : kDividerSize;

  if (is_horizontal_) {
    *leading_bounds = gfx::Rect(0, 0, divider_at, bounds.height());
    *trailing_bounds =
        gfx::Rect(divider_at + divider_size, 0,
                  std::max(0, bounds.width() - divider_at - divider_size),
                  bounds.height());
  } else {
    *leading_bounds = gfx::Rect(0, 0, bounds.width(), divider_at);
    *trailing_bounds =
        gfx::Rect(0, divider_at + divider_size, bounds.width(),
                  std::max(0, bounds.height() - divider_at - divider_size));
  }
}

void SingleSplitView::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
}

bool SingleSplitView::OnMousePressed(const MouseEvent& event) {
  if (!IsPointInDivider(event.location()))
    return false;
  drag_info_.initial_mouse_offset = GetPrimaryAxisSize(event.x(), event.y());
  drag_info_.initial_divider_offset =
      NormalizeDividerOffset(divider_offset_, bounds());
  return true;
}

bool SingleSplitView::OnMouseDragged(const MouseEvent& event) {
  if (child_count() < 2)
    return false;

  int delta_offset = GetPrimaryAxisSize(event.x(), event.y()) -
      drag_info_.initial_mouse_offset;
  if (is_horizontal_ && base::i18n::IsRTL())
    delta_offset *= -1;
  // Honor the minimum size when resizing.
  gfx::Size min = child_at(0)->GetMinimumSize();
  int new_size = std::max(GetPrimaryAxisSize(min.width(), min.height()),
                          drag_info_.initial_divider_offset + delta_offset);

  // And don't let the view get bigger than our width.
  new_size = std::min(GetPrimaryAxisSize() - kDividerSize, new_size);

  if (new_size != divider_offset_) {
    set_divider_offset(new_size);
    if (!listener_ || listener_->SplitHandleMoved(this))
      Layout();
  }
  return true;
}

void SingleSplitView::OnMouseCaptureLost() {
  if (child_count() < 2)
    return;

  if (drag_info_.initial_divider_offset != divider_offset_) {
    set_divider_offset(drag_info_.initial_divider_offset);
    if (!listener_ || listener_->SplitHandleMoved(this))
      Layout();
  }
}

void SingleSplitView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  divider_offset_ = CalculateDividerOffset(divider_offset_, previous_bounds,
                                           bounds());
}

bool SingleSplitView::IsPointInDivider(const gfx::Point& p) {
  if (child_count() < 2)
    return false;

  if (!child_at(0)->IsVisible() || !child_at(1)->IsVisible())
    return false;

  int divider_relative_offset;
  if (is_horizontal_) {
    divider_relative_offset =
        p.x() - child_at(base::i18n::IsRTL() ? 1 : 0)->width();
  } else {
    divider_relative_offset = p.y() - child_at(0)->height();
  }
  return (divider_relative_offset >= 0 &&
      divider_relative_offset < kDividerSize);
}

int SingleSplitView::CalculateDividerOffset(
    int divider_offset,
    const gfx::Rect& previous_bounds,
    const gfx::Rect& new_bounds) const {
  if (resize_leading_on_bounds_change_ && divider_offset != -1) {
    // We do not update divider_offset on minimize (to zero) and on restore
    // (to largest value). As a result we get back to the original value upon
    // window restore.
    bool is_minimize_or_restore =
        previous_bounds.height() == 0 || new_bounds.height() == 0;
    if (!is_minimize_or_restore) {
      if (is_horizontal_)
        divider_offset += new_bounds.width() - previous_bounds.width();
      else
        divider_offset += new_bounds.height() - previous_bounds.height();

      if (divider_offset < 0)
        divider_offset = kDividerSize;
    }
  }
  return divider_offset;
}

int SingleSplitView::NormalizeDividerOffset(int divider_offset,
                                            const gfx::Rect& bounds) const {
  int primary_axis_size = GetPrimaryAxisSize(bounds.width(), bounds.height());
  if (divider_offset < 0)
    // primary_axis_size may < kDividerSize during initial layout.
    return std::max(0, (primary_axis_size - kDividerSize) / 2);
  return std::min(divider_offset,
                  std::max(primary_axis_size - kDividerSize, 0));
}

}  // namespace views
