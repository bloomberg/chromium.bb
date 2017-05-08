// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_delegate.h"

#include "ui/arc/notification/arc_custom_notification_view.h"
#include "ui/arc/notification/arc_notification_item.h"

namespace arc {

ArcNotificationDelegate::ArcNotificationDelegate(
    base::WeakPtr<ArcNotificationItem> item)
    : item_(item) {
  DCHECK(item_);
}

ArcNotificationDelegate::~ArcNotificationDelegate() {}

std::unique_ptr<message_center::CustomContent>
ArcNotificationDelegate::CreateCustomContent() {
  DCHECK(item_);
  auto view = base::MakeUnique<ArcCustomNotificationView>(item_.get());
  auto content_view_delegate = view->CreateContentViewDelegate();
  return base::MakeUnique<message_center::CustomContent>(
      std::move(view), std::move(content_view_delegate));
}

void ArcNotificationDelegate::Close(bool by_user) {
  DCHECK(item_);
  item_->Close(by_user);
}

void ArcNotificationDelegate::Click() {
  DCHECK(item_);
  item_->Click();
}

}  // namespace arc
