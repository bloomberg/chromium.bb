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

BubbleFrameView::BubbleFrameView(BubbleBorder::ArrowLocation arrow_location,
                                 SkColor color,
                                 int margin)
    : bubble_border_(NULL),
      content_margins_(margin, margin, margin, margin) {
  if (base::i18n::IsRTL())
    arrow_location = BubbleBorder::horizontal_mirror(arrow_location);
  // TODO(alicet): Expose the shadow option in BorderContentsView when we make
  // the fullscreen exit bubble use the new bubble code.
  SetBubbleBorder(new BubbleBorder(arrow_location, BubbleBorder::NO_SHADOW));
  set_background(new BubbleBackground(bubble_border_));
  bubble_border()->set_background_color(color);
}

BubbleFrameView::~BubbleFrameView() {}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  gfx::Insets margin;
  bubble_border()->GetInsets(&margin);
  margin += content_margins();
  return gfx::Rect(margin.left(), margin.top(),
                   std::max(width() - margin.width(), 0),
                   std::max(height() - margin.height(), 0));
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
                                                  bool try_mirroring_arrow) {
  // Give the contents a margin.
  client_size.Enlarge(content_margins_.width(), content_margins_.height());

  if (try_mirroring_arrow) {
    // Try to mirror the anchoring if the bubble does not fit on the screen.
    MirrorArrowIfOffScreen(true, anchor_rect, client_size);
    MirrorArrowIfOffScreen(false, anchor_rect, client_size);
  }

  // Calculate the bounds with the arrow in its updated location.
  return bubble_border_->GetBounds(anchor_rect, client_size);
}

void BubbleFrameView::SetBubbleBorder(BubbleBorder* border) {
  bubble_border_ = border;
  set_border(bubble_border_);
}

gfx::Rect BubbleFrameView::GetMonitorBounds(const gfx::Rect& rect) {
  return gfx::Screen::GetDisplayNearestPoint(rect.CenterPoint()).work_area();
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

}  // namespace views
