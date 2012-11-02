// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_VIEW_FACTORY_H_
#define UI_MESSAGE_CENTER_MESSAGE_VIEW_FACTORY_H_

#include "ui/message_center/notification_list.h"

namespace message_center {

class MessageView;

// Creates appropriate MessageViews for Notifications. The factory
// examines notification.type and uses that as a 1:1 mapping from type enum to
// concrete MessageView class.
class MessageViewFactory {
 public:
  static MessageView* ViewForNotification(
      const NotificationList::Notification& notification,
      NotificationList::Delegate* list_delegate);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_WEB_MESSAGE_VIEW_FACTORY_H_
