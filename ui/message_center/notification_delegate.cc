// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_delegate.h"

#include "base/bind.h"
#include "base/logging.h"

namespace message_center {

// NotificationDelegate:

void NotificationDelegate::Display() {}

void NotificationDelegate::Close(bool by_user) {}

void NotificationDelegate::Click() {}

void NotificationDelegate::ButtonClick(int button_index) {}

void NotificationDelegate::ButtonClickWithReply(int button_index,
                                                const base::string16& reply) {
  NOTIMPLEMENTED();
}

void NotificationDelegate::SettingsClick() {}

void NotificationDelegate::DisableNotification() {}

// HandleNotificationClickDelegate:

HandleNotificationClickDelegate::HandleNotificationClickDelegate(
    const base::Closure& callback) {
  if (!callback.is_null()) {
    // Create a callback that consumes and ignores the button index parameter,
    // and just runs the provided closure.
    callback_ = base::Bind(
        [](const base::Closure& closure, base::Optional<int> button_index) {
          DCHECK(!button_index);
          closure.Run();
        },
        callback);
  }
}

HandleNotificationClickDelegate::HandleNotificationClickDelegate(
    const ButtonClickCallback& callback)
    : callback_(callback) {}

HandleNotificationClickDelegate::~HandleNotificationClickDelegate() {}

void HandleNotificationClickDelegate::Click() {
  if (!callback_.is_null())
    callback_.Run(base::nullopt);
}

void HandleNotificationClickDelegate::ButtonClick(int button_index) {
  if (!callback_.is_null())
    callback_.Run(button_index);
}

}  // namespace message_center
