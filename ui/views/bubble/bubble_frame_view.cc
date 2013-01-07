// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include <algorithm>

#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

namespace {

// Get the |vertical| or horizontal screen overflow of the |window_bounds|.
int GetOffScreenLength(const gfx::Rect& monitor_bounds,
                       const gfx::Rect& window_bounds,
                       bool vertical) {
  if (monitor_bounds.IsEmpty() || monitor_bounds.Contains(window_bounds))
    return 0;

  //  window_bounds
  //  +-------------------------------+
  //  |             top               |
  //  |      +----------------+       |
  //  | left | monitor_bounds | right |
  //  |      +----------------+       |
  //  |            bottom             |
  //  +-------------------------------+
  if (vertical)
    return std::max(0, monitor_bounds.y() - window_bounds.y()) +
           std::max(0, window_bounds.bottom() - monitor_bounds.bottom());
  return std::max(0, monitor_bounds.x() - window_bounds.x()) +
         std::max(0, window_bounds.right() - monitor_bounds.right());
}

}  // namespace

namespace views {

BubbleFrameView::BubbleFrameView(const gfx::Insets& margins,
                                 BubbleBorder* border)
    : bubble_border_(border),
      content_margins_(margins) {
  set_border(bubble_border_);
}

BubbleFrameView::~BubbleFrameView() {}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = GetLocalBounds();
  client_bounds.Inset(border()->GetInsets());
  client_bounds.Inset(content_margins());
  return client_bounds;
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return const_cast<BubbleFrameView*>(this)->GetUpdatedWindowBounds(
      gfx::Rect(), client_bounds.size(), false);
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  return GetWidget()->client_view()->NonClientHitTest(point);
}

gfx::Size BubbleFrameView::GetPreferredSize() {
  gfx::Size client_size(GetWidget()->client_view()->GetPreferredSize());
  return GetUpdatedWindowBounds(gfx::Rect(), client_size, false).size();
}

gfx::Rect BubbleFrameView::GetUpdatedWindowBounds(const gfx::Rect& anchor_rect,
                                                  gfx::Size client_size,
                                                  bool adjust_if_offscreen) {
  // Give the contents a margin.
  client_size.Enlarge(content_margins_.width(), content_margins_.height());

  const BubbleBorder::ArrowLocation arrow = bubble_border_->arrow_location();
  if (adjust_if_offscreen && BubbleBorder::has_arrow(arrow)) {
    if (!bubble_border_->is_arrow_at_center(arrow)) {
      // Try to mirror the anchoring if the bubble does not fit on the screen.
      MirrorArrowIfOffScreen(true, anchor_rect, client_size);
      MirrorArrowIfOffScreen(false, anchor_rect, client_size);
    } else {
      OffsetArrowIfOffScreen(anchor_rect, client_size);
    }
  }

  // Calculate the bounds with the arrow in its updated location and offset.
  return bubble_border_->GetBounds(anchor_rect, client_size);
}

void BubbleFrameView::SetBubbleBorder(BubbleBorder* border) {
  bubble_border_ = border;
  set_border(bubble_border_);

  // Update the background, which relies on the border.
  set_background(new views::BubbleBackground(border));
}

gfx::Rect BubbleFrameView::GetMonitorBounds(const gfx::Rect& rect) {
  // TODO(scottmg): Native is wrong. http://crbug.com/133312
  return gfx::Screen::GetNativeScreen()->GetDisplayNearestPoint(
      rect.CenterPoint()).work_area();
}

void BubbleFrameView::MirrorArrowIfOffScreen(
    bool vertical,
    const gfx::Rect& anchor_rect,
    const gfx::Size& client_size) {
  // Check if the bounds don't fit on screen.
  gfx::Rect monitor_rect(GetMonitorBounds(anchor_rect));
  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));
  if (GetOffScreenLength(monitor_rect, window_bounds, vertical) > 0) {
    BubbleBorder::ArrowLocation arrow = bubble_border()->arrow_location();
    // Mirror the arrow and get the new bounds.
    bubble_border_->set_arrow_location(
        vertical ? BubbleBorder::vertical_mirror(arrow) :
                   BubbleBorder::horizontal_mirror(arrow));
    gfx::Rect mirror_bounds =
        bubble_border_->GetBounds(anchor_rect, client_size);
    // Restore the original arrow if mirroring doesn't show more of the bubble.
    if (GetOffScreenLength(monitor_rect, mirror_bounds, vertical) >=
        GetOffScreenLength(monitor_rect, window_bounds, vertical))
      bubble_border_->set_arrow_location(arrow);
    else
      SchedulePaint();
  }
}

void BubbleFrameView::OffsetArrowIfOffScreen(const gfx::Rect& anchor_rect,
                                             const gfx::Size& client_size) {
  BubbleBorder::ArrowLocation arrow = bubble_border()->arrow_location();
  DCHECK(BubbleBorder::is_arrow_at_center(arrow));

  // Get the desired bubble bounds without adjustment.
  bubble_border_->set_arrow_offset(0);
  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));

  gfx::Rect monitor_rect(GetMonitorBounds(anchor_rect));
  if (monitor_rect.IsEmpty() || monitor_rect.Contains(window_bounds))
    return;

  // Calculate off-screen adjustment.
  const bool is_horizontal = BubbleBorder::is_arrow_on_horizontal(arrow);
  int offscreen_adjust = 0;
  if (is_horizontal) {
    if (window_bounds.x() < monitor_rect.x())
      offscreen_adjust = monitor_rect.x() - window_bounds.x();
    else if (window_bounds.right() > monitor_rect.right())
      offscreen_adjust = monitor_rect.right() - window_bounds.right();
  } else {
    if (window_bounds.y() < monitor_rect.y())
      offscreen_adjust = monitor_rect.y() - window_bounds.y();
    else if (window_bounds.bottom() > monitor_rect.bottom())
      offscreen_adjust = monitor_rect.bottom() - window_bounds.bottom();
  }

  // For center arrows, arrows are moved in the opposite direction of
  // |offscreen_adjust|, e.g. positive |offscreen_adjust| means bubble
  // window needs to be moved to the right and that means we need to move arrow
  // to the left, and that means negative offset.
  bubble_border_->set_arrow_offset(
      bubble_border_->GetArrowOffset(window_bounds.size()) - offscreen_adjust);
  if (offscreen_adjust)
    SchedulePaint();
}

}  // namespace views
