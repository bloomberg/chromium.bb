// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_CONTROLLER_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_CONTROLLER_H_

#include <string>

#include "ui/message_center/notifier_settings.h"

namespace message_center {

// Interface used by views to report clicks and other user actions. The views
// by themselves do not know how to perform those operations, they ask
// MessageCenterController to do them. Implemented by MessageCeneterView and
// MessagePopupCollection.
class MessageCenterController {
 public:
  virtual void ClickOnNotification(const std::string& notification_id) = 0;
  virtual void RemoveNotification(const std::string& notification_id,
                                  bool by_user) = 0;
  virtual void DisableNotificationsFromThisSource(
      const NotifierId& notifier_id) = 0;
  virtual void ShowNotifierSettingsBubble() = 0;
  virtual bool HasClickedListener(const std::string& notification_id) = 0;
  virtual void ClickOnNotificationButton(const std::string& notification_id,
                                         int button_index) = 0;
  virtual void ExpandNotification(const std::string& notification_id) = 0;
  virtual void GroupBodyClicked(const std::string& last_notification_id) = 0;
  virtual void ExpandGroup(const NotifierId& notifier_id) = 0;
  virtual void RemoveGroup(const NotifierId& notifier_id) = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_CONTROLLER_H_