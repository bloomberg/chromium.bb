// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include <algorithm>

#include "ui/views/bubble/border_contents_view.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/client_view.h"
#include "views/widget/widget.h"

namespace views {

BubbleFrameView::BubbleFrameView(BubbleBorder::ArrowLocation location,
                                 const gfx::Size& client_size,
                                 SkColor color,
                                 bool allow_bubble_offscreen)
    : border_contents_(new BorderContentsView()),
      location_(location),
      allow_bubble_offscreen_(allow_bubble_offscreen) {
  border_contents_->Init();
  bubble_border()->set_arrow_location(location_);
  bubble_border()->set_background_color(color);
  SetLayoutManager(new views::FillLayout());
  AddChildView(border_contents_);
  gfx::Rect bounds(gfx::Point(), client_size);
  gfx::Rect windows_bounds = GetWindowBoundsForClientBounds(bounds);
  border_contents_->SetBoundsRect(
      gfx::Rect(gfx::Point(), windows_bounds.size()));
  SetBoundsRect(windows_bounds);
}

BubbleFrameView::~BubbleFrameView() {}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  gfx::Insets margin;
  bubble_border()->GetInsets(&margin);
  margin += border_contents_->content_margins();
  return gfx::Rect(margin.left(),
                   margin.top(),
                   std::max(width() - margin.width(), 0),
                   std::max(height() - margin.height(), 0));
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  // The |client_bounds| origin is the bubble arrow anchor point.
  gfx::Rect position_relative_to(client_bounds.origin(), gfx::Size());
  // The |client_bounds| size is the bubble client view size.
  gfx::Rect content_bounds;
  gfx::Rect window_bounds;
  border_contents_->SizeAndGetBounds(position_relative_to,
                                     location_,
                                     allow_bubble_offscreen_,
                                     client_bounds.size(),
                                     &content_bounds,
                                     &window_bounds);
  return window_bounds;
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  return GetWidget()->client_view()->NonClientHitTest(point);
}

gfx::Size BubbleFrameView::GetPreferredSize() {
  Widget* widget = GetWidget();
  gfx::Rect rect(gfx::Point(), widget->client_view()->GetPreferredSize());
  return widget->non_client_view()->GetWindowBoundsForClientBounds(rect).size();
}

BubbleBorder* BubbleFrameView::bubble_border() const {
  return static_cast<BubbleBorder*>(border_contents_->border());
}

}  // namespace views
