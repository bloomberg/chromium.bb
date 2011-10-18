// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_frame_view.h"

#include "ui/gfx/canvas.h"
#include "views/bubble/bubble_border.h"
#include "views/widget/widget.h"
#include "views/window/client_view.h"

namespace views {

BubbleFrameView::BubbleFrameView(BubbleBorder::ArrowLocation location,
                                 const gfx::Size& client_size,
                                 SkColor color) {
  BubbleBorder* bubble_border = new BubbleBorder(location);
  bubble_border->set_background_color(color);
  set_border(bubble_border);
  set_background(new BubbleBackground(bubble_border));
  // Calculate the frame size from the client size.
  gfx::Rect bounds(gfx::Point(), client_size);
  SetBoundsRect(GetWindowBoundsForClientBounds(bounds));
}

BubbleFrameView::~BubbleFrameView() {}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds(gfx::Point(), size());
  client_bounds.Inset(GetInsets());
  return client_bounds;
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  // The |client_bounds| origin is the bubble arrow anchor point.
  gfx::Rect position_relative_to(client_bounds.origin(), gfx::Size());
  // The |client_bounds| size is the bubble client view size.
  return static_cast<const BubbleBorder*>(border())->GetBounds(
      position_relative_to, client_bounds.size());
}

void BubbleFrameView::OnPaint(gfx::Canvas* canvas) {
  border()->Paint(*this, canvas);
  background()->Paint(canvas, this);
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  return GetWidget()->client_view()->NonClientHitTest(point);
}

gfx::Size BubbleFrameView::GetPreferredSize() {
  Widget* widget = GetWidget();
  gfx::Rect rect(gfx::Point(), widget->client_view()->GetPreferredSize());
  return widget->non_client_view()->GetWindowBoundsForClientBounds(rect).size();
}

}  // namespace views
