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

  auto custom_content = notification.delegate()->CreateCustomContent();

  contents_view_ = custom_content->view.release();
  DCHECK(contents_view_);
  AddChildView(contents_view_);

  contents_view_delegate_ = std::move(custom_content->delegate);
  DCHECK(contents_view_delegate_);

  if (contents_view_->background()) {
    background_view()->background()->SetNativeControlColor(
        contents_view_->background()->get_color());
  }

  AddChildView(small_image());
}

CustomNotificationView::~CustomNotificationView() {}

void CustomNotificationView::SetDrawBackgroundAsActive(bool active) {
  // Do nothing if |contents_view_| has a background.
  if (contents_view_->background())
    return;

  MessageView::SetDrawBackgroundAsActive(active);
}

bool CustomNotificationView::IsCloseButtonFocused() const {
  if (!contents_view_delegate_)
    return false;
  return contents_view_delegate_->IsCloseButtonFocused();
}

void CustomNotificationView::RequestFocusOnCloseButton() {
  if (contents_view_delegate_)
    contents_view_delegate_->RequestFocusOnCloseButton();
}

bool CustomNotificationView::IsPinned() const {
  if (!contents_view_delegate_)
    return false;
  return contents_view_delegate_->IsPinned();
}

gfx::Size CustomNotificationView::GetPreferredSize() const {
  const gfx::Insets insets = GetInsets();
  const int contents_width = kNotificationWidth - insets.width();
  const int contents_height = contents_view_->GetHeightForWidth(contents_width);

  constexpr int kMaxContentHeight = 256;
  constexpr int kMinContentHeight = 64;
  return gfx::Size(kNotificationWidth,
                   std::max(kMinContentHeight + insets.height(),
                            std::min(kMaxContentHeight + insets.height(),
                                     contents_height + insets.height())));
}

void CustomNotificationView::Layout() {
  MessageView::Layout();

  contents_view_->SetBoundsRect(GetContentsBounds());
}

}  // namespace message_center
