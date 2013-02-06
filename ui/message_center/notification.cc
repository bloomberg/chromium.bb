// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification.h"

namespace message_center {

NotificationItem::NotificationItem(string16 title, string16 message)
 : title(title),
   message(message) {
}

Notification::Notification()
 : is_read(false),
   shown_as_popup(false) {
}

Notification::~Notification() {
}

}  // namespace message_center
