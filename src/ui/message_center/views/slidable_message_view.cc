// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/slidable_message_view.h"

#include "ui/message_center/public/cpp/features.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace message_center {

SlidableMessageView::SlidableMessageView(MessageView* message_view)
    : message_view_(message_view) {
  SetFocusBehavior(views::View::FocusBehavior::NEVER);

  // Draw on its own layer to allow bound animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  if (base::FeatureList::IsEnabled(message_center::kNotificationSwipeControl))
    SetBackground(views::CreateSolidBackground(kSwipeControlBackgroundColor));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // TODO(crbug.com/840497): Add button container child view.

  message_view_->AddSlideObserver(this);
  AddChildView(message_view);
}

SlidableMessageView::~SlidableMessageView() = default;

void SlidableMessageView::OnSlideChanged(const std::string& notification_id) {
  // TODO(crbug.com/840497): Show/hide control buttons.
}

void SlidableMessageView::CloseSwipeControl() {
  message_view_->CloseSwipeControl();
}

void SlidableMessageView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
  InvalidateLayout();
}

void SlidableMessageView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
  InvalidateLayout();
}

void SlidableMessageView::UpdateWithNotification(
    const Notification& notification) {
  message_view_->UpdateWithNotification(notification);
}

gfx::Size SlidableMessageView::CalculatePreferredSize() const {
  return message_view_->GetPreferredSize();
}

int SlidableMessageView::GetHeightForWidth(int width) const {
  return message_view_->GetHeightForWidth(width);
}

}  // namespace message_center
