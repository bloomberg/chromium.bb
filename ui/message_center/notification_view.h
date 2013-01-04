// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_VIEW_H_

#include "ui/message_center/message_view.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

// View that displays multiple-item notifications, and in the future will
// display all types of notification (simple/web, basic/base, image, and
// multiple-item/inbox).
class NotificationView : public MessageView {
 public:
  NotificationView(NotificationList::Delegate* list_delegate,
                   const NotificationList::Notification& notification);
  virtual ~NotificationView();

  // Overridden from MessageView.
  virtual void SetUpView() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_NOTIFICATION_VIEW_H_
