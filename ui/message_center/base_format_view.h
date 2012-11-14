// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_BASE_FORMAT_VIEW_H_
#define UI_MESSAGE_CENTER_BASE_FORMAT_VIEW_H_

#include "ui/message_center/message_view.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

// An early version of a more comprehensive message view.
//
// TODO: add remaining fields from prototype specification, and bring cosmetics
// to an acceptable level of polish.
class BaseFormatView : public MessageView {
 public:
  BaseFormatView(NotificationList::Delegate* list_delegate,
                 const NotificationList::Notification& notification);
  virtual ~BaseFormatView();

  // MessageView
  virtual void SetUpView() OVERRIDE;

 protected:
  BaseFormatView();

  DISALLOW_COPY_AND_ASSIGN(BaseFormatView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_BASE_FORMAT_VIEW_H_
