// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_view_factory.h"

#include "ui/message_center/base_format_view.h"
#include "ui/message_center/message_simple_view.h"
#include "ui/message_center/message_view.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_view.h"
#include "ui/notifications/notification_types.h"

namespace message_center {

// static
MessageView* MessageViewFactory::ViewForNotification(
    const NotificationList::Notification& notification,
    NotificationList::Delegate* list_delegate) {
  switch (notification.type) {
    case ui::notifications::NOTIFICATION_TYPE_BASE_FORMAT:
      return new BaseFormatView(list_delegate, notification);
    case ui::notifications::NOTIFICATION_TYPE_IMAGE:
      return new NotificationView(list_delegate, notification);
    case ui::notifications::NOTIFICATION_TYPE_MULTIPLE:
      return new NotificationView(list_delegate, notification);
    case ui::notifications::NOTIFICATION_TYPE_SIMPLE:
      return new MessageSimpleView(list_delegate, notification);

    default:
      // If the caller asks for an unrecognized kind of view (entirely possible
      // if an application is running on an older version of this code that
      // doesn't have the requested kind of notification template), we'll fall
      // back to the simplest kind of notification.
      LOG(WARNING) << "Unable to fulfill request for unrecognized "
                   << "notification type " << notification.type << ". "
                   << "Falling back to simple notification type.";
      return new MessageSimpleView(list_delegate, notification);
  }
  NOTREACHED();
}

}  // namespace message_center
