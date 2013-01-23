// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_VIEW_H_

#include "ui/message_center/message_view.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

// View that displays image and list notifications, and in the future will
// display all types of notification (web/simple, basic/base, image, and
// list/multiple-item/inbox/digest).
class NotificationView : public MessageView {
 public:
  NotificationView(NotificationList::Delegate* list_delegate,
                   const NotificationList::Notification& notification);
  virtual ~NotificationView();

  // Overridden from View.
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from MessageView.
  virtual void SetUpView() OVERRIDE;

 private:
  views::View* MakeContentView();

  views::View* content_view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_NOTIFICATION_VIEW_H_
