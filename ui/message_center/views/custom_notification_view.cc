// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/custom_notification_view.h"

#include <algorithm>

#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"

namespace message_center {

CustomNotificationView::CustomNotificationView(
    MessageCenterController* controller,
    const Notification& notification)
    : MessageView(controller, notification) {
  DCHECK_EQ(NOTIFICATION_TYPE_CUSTOM, notification.type());

  contents_view_ = notification.delegate()->CreateCustomContent().release();
  DCHECK(contents_view_);
  AddChildView(contents_view_);

  if (contents_view_->background()) {
    background_view()->background()->SetNativeControlColor(
        contents_view_->background()->get_color());
  }

  AddChildView(small_image());

  CreateOrUpdateCloseButtonView(notification);

  // Use a layer for close button so that custom content does not eclipse it.
  if (close_button())
    close_button()->SetPaintToLayer(true);
}

CustomNotificationView::~CustomNotificationView() {}

void CustomNotificationView::SetDrawBackgroundAsActive(bool active) {
  // Do nothing if |contents_view_| has a background.
  if (contents_view_->background())
    return;

  MessageView::SetDrawBackgroundAsActive(active);
}

gfx::Size CustomNotificationView::GetPreferredSize() const {
  const gfx::Insets insets = GetInsets();
  const int contents_width = kNotificationWidth - insets.width();
  const int contents_height = contents_view_->GetHeightForWidth(contents_width);

  const int kMaxHeight = 240;
  const int kMinHeight = 100;
  return gfx::Size(
      kNotificationWidth,
      std::max(kMinHeight,
               std::min(kMaxHeight, contents_height + insets.height())));
}

void CustomNotificationView::Layout() {
  MessageView::Layout();

  contents_view_->SetBoundsRect(GetContentsBounds());
}

}  // namespace message_center
