// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/bounded_scroll_view.h"

#include <algorithm>

#include "ui/gfx/insets.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/background.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

namespace message_center {

BoundedScrollView::BoundedScrollView(int min_height, int max_height)
    : min_height_(min_height), max_height_(max_height) {
  set_notify_enter_exit_on_child(true);
  set_background(
      views::Background::CreateSolidBackground(kMessageCenterBackgroundColor));
  SetVerticalScrollBar(new views::OverlayScrollBar(false));
}

gfx::Size BoundedScrollView::GetPreferredSize() {
  gfx::Size size = contents()->GetPreferredSize();
  size.SetToMax(gfx::Size(size.width(), min_height_));
  size.SetToMin(gfx::Size(size.width(), max_height_));
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

int BoundedScrollView::GetHeightForWidth(int width) {
  gfx::Insets insets = GetInsets();
  width = std::max(0, width - insets.width());
  int height = contents()->GetHeightForWidth(width) + insets.height();
  return std::min(std::max(height, min_height_), max_height_);
}

void BoundedScrollView::Layout() {
  int content_width = width();
  int content_height = contents()->GetHeightForWidth(content_width);
  if (content_height > height()) {
    content_width = std::max(content_width - GetScrollBarWidth(), 0);
    content_height = contents()->GetHeightForWidth(content_width);
  }
  if (contents()->bounds().size() != gfx::Size(content_width, content_height))
    contents()->SetBounds(0, 0, content_width, content_height);
  views::ScrollView::Layout();
}

}  // namespace
