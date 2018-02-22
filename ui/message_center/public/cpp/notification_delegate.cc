// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/public/cpp/notification_delegate.h"

#include "base/bind.h"
#include "base/logging.h"

namespace message_center {

// ThunkNotificationDelegate:

ThunkNotificationDelegate::ThunkNotificationDelegate(
    base::WeakPtr<NotificationObserver> impl)
    : impl_(impl) {}

void ThunkNotificationDelegate::Close(bool by_user) {
  if (impl_)
    impl_->Close(by_user);
}

void ThunkNotificationDelegate::Click() {
  if (impl_)
    impl_->Click();
}

void ThunkNotificationDelegate::ButtonClick(int button_index) {
  if (impl_)
    impl_->ButtonClick(button_index);
}

void ThunkNotificationDelegate::ButtonClickWithReply(
    int button_index,
    const base::string16& reply) {
  if (impl_)
    impl_->ButtonClickWithReply(button_index, reply);
}

void ThunkNotificationDelegate::SettingsClick() {
  if (impl_)
    impl_->SettingsClick();
}

void ThunkNotificationDelegate::DisableNotification() {
  if (impl_)
    impl_->DisableNotification();
}

ThunkNotificationDelegate::~ThunkNotificationDelegate() = default;

// HandleNotificationClickDelegate:

HandleNotificationClickDelegate::HandleNotificationClickDelegate(
    const base::RepeatingClosure& callback) {
  if (!callback.is_null()) {
    // Create a callback that consumes and ignores the button index parameter,
    // and just runs the provided closure.
    callback_ = base::BindRepeating(
        [](const base::RepeatingClosure& closure,
           base::Optional<int> button_index) {
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
