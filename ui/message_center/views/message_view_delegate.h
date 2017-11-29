// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_DELEGATE_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_DELEGATE_H_

#include <memory>
#include <string>

#include "ui/base/models/menu_model.h"

namespace message_center {

// Interface used by views to report clicks and other user actions. The views
// by themselves do not know how to perform those operations, they ask
// MessageViewDelegate to do them. Implemented by MessageCenterView and
// MessagePopupCollection.
class MessageViewDelegate {
 public:
  virtual void ClickOnNotification(const std::string& notification_id) = 0;
  virtual void RemoveNotification(const std::string& notification_id,
                                  bool by_user) = 0;
  virtual void ClickOnNotificationButton(const std::string& notification_id,
                                         int button_index) = 0;
  virtual void ClickOnNotificationButtonWithReply(
      const std::string& notification_id,
      int button_index,
      const base::string16& reply) = 0;
  virtual void ClickOnSettingsButton(const std::string& notification_id) = 0;
  // For ArcCustomNotificationView, size changes might come after the
  // notification update over Mojo. Provide a method here to notify when
  // a notification changed in some way and needs to be laid out again.
  // See crbug.com/690813.
  virtual void UpdateNotificationSize(const std::string& notification_id) = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_DELEGATE_H_
