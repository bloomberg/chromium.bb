// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_delegate.h"

#include "ui/message_center/views/message_view.h"

namespace message_center {

std::unique_ptr<MessageView> NotificationDelegate::CreateCustomMessageView(
    MessageCenterController* controller,
    const Notification& notification) {
  return nullptr;
}

}  // namespace message_center
