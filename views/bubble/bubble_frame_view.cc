// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_frame_view.h"

#include "grit/ui_resources.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "views/bubble/bubble_border.h"
#include "views/widget/widget_delegate.h"
#include "views/window/client_view.h"

namespace views {

BubbleFrameView::BubbleFrameView(Widget* frame,
                                 const gfx::Rect& bounds,
                                 SkColor color,
                                 BubbleBorder::ArrowLocation location)
    : frame_(frame),
      frame_bounds_(bounds),
      bubble_border_(new BubbleBorder(location)),
      bubble_background_(new BubbleBackground(bubble_border_)) {
  SetBoundsRect(bounds);
  bubble_border_->set_background_color(color);
  set_border(bubble_border_);
  set_background(bubble_background_);
}

BubbleFrameView::~BubbleFrameView() {}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  gfx::Insets insets;
  bubble_border_->GetInsets(&insets);
  return gfx::Rect(insets.left(), insets.top(),
                   frame_bounds_.width() - insets.left() - insets.right(),
                   frame_bounds_.height() - insets.top() - insets.bottom());
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return bubble_border_->GetBounds(client_bounds, client_bounds.size());
}

void BubbleFrameView::EnableClose(bool enable) {
}

void BubbleFrameView::ResetWindowControls() {
}

void BubbleFrameView::UpdateWindowIcon() {
}

void BubbleFrameView::OnPaint(gfx::Canvas* canvas) {
  border()->Paint(*this, canvas);
  bubble_background_->Paint(canvas, this);
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  return frame_->client_view()->NonClientHitTest(point);
}

void BubbleFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
}

gfx::Size BubbleFrameView::GetPreferredSize() {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()->GetWindowBoundsForClientBounds(
      bounds).size();
}
}  // namespace views
