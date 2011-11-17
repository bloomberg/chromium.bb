// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/border_contents_view.h"

#include <algorithm>

#include "ui/gfx/screen.h"

static const int kTopMargin = 6;
static const int kLeftMargin = 6;
static const int kBottomMargin = 6;
static const int kRightMargin = 6;

namespace {

// Computes how much |window_bounds| is off-screen of the monitor bounds
// |monitor_bounds| and puts the values in |offscreen_insets|.
// Returns false if |window_bounds| is actually contained in |monitor_bounds|,
// in which case |offscreen_insets| is not modified.
bool ComputeOffScreenInsets(const gfx::Rect& monitor_bounds,
                            const gfx::Rect& window_bounds,
                            gfx::Insets* offscreen_insets) {
  if (monitor_bounds.Contains(window_bounds))
    return false;

  if (!offscreen_insets)
    return true;

  //  window_bounds
  //  +-------------------------------+
  //  |             top               |
  //  |      +----------------+       |
  //  | left | monitor_bounds | right |
  //  |      +----------------+       |
  //  |            bottom             |
  //  +-------------------------------+
  int top = std::max(0, monitor_bounds.y() - window_bounds.y());
  int left = std::max(0, monitor_bounds.x() - window_bounds.x());
  int bottom = std::max(0, window_bounds.bottom() - monitor_bounds.bottom());
  int right = std::max(0, window_bounds.right() - monitor_bounds.right());

  offscreen_insets->Set(top, left, bottom, right);
  return true;
}

// Convenience method that returns the height of |insets| if |vertical| is
// true, its width otherwise.
int GetInsetsLength(const gfx::Insets& insets, bool vertical) {
  return vertical ? insets.height() : insets.width();
}

}  // namespace

namespace views {

BorderContentsView::BorderContentsView()
    : bubble_border_(NULL),
      content_margins_(kTopMargin, kLeftMargin, kBottomMargin, kRightMargin) {
}

BorderContentsView::BorderContentsView(int top_margin,
                                       int left_margin,
                                       int bottom_margin,
                                       int right_margin)
    : bubble_border_(NULL),
      content_margins_(top_margin, left_margin, bottom_margin, right_margin) {
}

BorderContentsView::~BorderContentsView() {}

void BorderContentsView::Init() {
  // Default arrow location.
  BubbleBorder::ArrowLocation arrow_location =
      BubbleBorder::TOP_LEFT;
  if (base::i18n::IsRTL())
    arrow_location = BubbleBorder::horizontal_mirror(arrow_location);
  DCHECK(!bubble_border_);

  // TODO(alicet): Expose the shadow option in BorderContentsView when we make
  // the fullscreen exit bubble use the new bubble code.
  bubble_border_ = new BubbleBorder(arrow_location,
                                    views::BubbleBorder::NO_SHADOW);
  set_border(bubble_border_);
  set_background(new BubbleBackground(bubble_border_));
}

void BorderContentsView::SetBackgroundColor(SkColor color) {
  bubble_border_->set_background_color(color);
}

void BorderContentsView::SetAlignment(
    views::BubbleBorder::BubbleAlignment alignment) {
  bubble_border_->set_alignment(alignment);
}

void BorderContentsView::SizeAndGetBounds(
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    bool allow_bubble_offscreen,
    const gfx::Size& contents_size,
    gfx::Rect* contents_bounds,
    gfx::Rect* window_bounds) {
  if (base::i18n::IsRTL())
    arrow_location = BubbleBorder::horizontal_mirror(arrow_location);
  bubble_border_->set_arrow_location(arrow_location);
  // Set the border.
  set_border(bubble_border_);

  // Give the contents a margin.
  gfx::Size local_contents_size(contents_size);
  local_contents_size.Enlarge(content_margins_.width(),
                              content_margins_.height());

  // Try putting the arrow in its initial location, and calculating the bounds.
  *window_bounds =
      bubble_border_->GetBounds(position_relative_to, local_contents_size);
  if (!allow_bubble_offscreen) {
    gfx::Rect monitor_bounds = GetMonitorBounds(position_relative_to);
    if (!monitor_bounds.IsEmpty()) {
      // Try to resize vertically if this does not fit on the screen.
      MirrorArrowIfOffScreen(true,  // |vertical|.
                             position_relative_to, monitor_bounds,
                             local_contents_size, &arrow_location,
                             window_bounds);
      // Then try to resize horizontally if it still does not fit on the screen.
      MirrorArrowIfOffScreen(false,  // |vertical|.
                             position_relative_to, monitor_bounds,
                             local_contents_size, &arrow_location,
                             window_bounds);
    }
  }

  // Calculate the bounds of the contained contents (in window coordinates) by
  // subtracting the border dimensions and margin amounts.
  *contents_bounds = gfx::Rect(gfx::Point(), window_bounds->size());
  gfx::Insets insets;
  bubble_border_->GetInsets(&insets);
  insets += content_margins_;
  contents_bounds->Inset(insets);
}

gfx::Rect BorderContentsView::GetMonitorBounds(const gfx::Rect& rect) {
  return gfx::Screen::GetMonitorWorkAreaNearestPoint(rect.CenterPoint());
}

void BorderContentsView::MirrorArrowIfOffScreen(
    bool vertical,
    const gfx::Rect& position_relative_to,
    const gfx::Rect& monitor_bounds,
    const gfx::Size& local_contents_size,
    BubbleBorder::ArrowLocation* arrow_location,
    gfx::Rect* window_bounds) {
  // If the bounds don't fit, move the arrow to its mirrored position to see if
  // it improves things.
  gfx::Insets offscreen_insets;
  if (ComputeOffScreenInsets(monitor_bounds, *window_bounds,
                             &offscreen_insets) &&
      GetInsetsLength(offscreen_insets, vertical) > 0) {
    BubbleBorder::ArrowLocation original_arrow_location =
        *arrow_location;
     *arrow_location =
         vertical ? BubbleBorder::vertical_mirror(*arrow_location) :
                    BubbleBorder::horizontal_mirror(*arrow_location);

    // Change the arrow and get the new bounds.
    bubble_border_->set_arrow_location(*arrow_location);
    *window_bounds = bubble_border_->GetBounds(position_relative_to,
                                               local_contents_size);
    gfx::Insets new_offscreen_insets;
    // If there is more of the window offscreen, we'll keep the old arrow.
    if (ComputeOffScreenInsets(monitor_bounds, *window_bounds,
                               &new_offscreen_insets) &&
        GetInsetsLength(new_offscreen_insets, vertical) >=
            GetInsetsLength(offscreen_insets, vertical)) {
      *arrow_location = original_arrow_location;
      bubble_border_->set_arrow_location(*arrow_location);
      *window_bounds = bubble_border_->GetBounds(position_relative_to,
                                                 local_contents_size);
    }
  }
}

}  // namespace views
