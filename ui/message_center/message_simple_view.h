// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_SIMPLE_VIEW_H_
#define UI_MESSAGE_CENTER_MESSAGE_SIMPLE_VIEW_H_

#include "ui/message_center/message_view.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

// A simple view for a notification entry (icon + message + buttons).
class MessageSimpleView : public MessageView {
 public:
  MessageSimpleView(NotificationList::Delegate* list_delegate,
                    const NotificationList::Notification& notification);
  virtual ~MessageSimpleView();

  // Overridden from MessageView:
  virtual void SetUpView() OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 protected:
  MessageSimpleView();

 private:
  scoped_ptr<views::ImageButton> old_style_close_button_;

  DISALLOW_COPY_AND_ASSIGN(MessageSimpleView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_MESSAGE_SIMPLE_VIEW_H_
