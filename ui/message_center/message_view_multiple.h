// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_VIEW_MULTIPLE_H_
#define UI_MESSAGE_CENTER_MESSAGE_VIEW_MULTIPLE_H_

#include "ui/message_center/message_view.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

// An early version of the multiple message notification view.
//
// TODO(dharcourt): Rename MessageCenter and MessageView* to NotificationCenter
// and NotificationView*?
class MessageViewMultiple : public MessageView {
 public:
  MessageViewMultiple(NotificationList::Delegate* list_delegate,
                      const NotificationList::Notification& notification);
  virtual ~MessageViewMultiple();

  // MessageView
  virtual void SetUpView() OVERRIDE;

 protected:
  MessageViewMultiple();

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageViewMultiple);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_MESSAGE_VIEW_MULTIPLE_H_
