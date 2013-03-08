// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_CHANGE_OBSERVER_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_CHANGE_OBSERVER_H_

#include <string>

#include "ui/gfx/native_widget_types.h"

namespace message_center {

// For classes that need to handle or propagate notification changes.
class MESSAGE_CENTER_EXPORT NotificationChangeObserver {
 public:
  virtual ~NotificationChangeObserver() {};

  virtual void OnRemoveNotification(const std::string& id, bool by_user) {};
  virtual void OnRemoveAllNotifications(bool by_user) {};

  virtual void OnDisableNotificationsByExtension(const std::string& id) {};
  virtual void OnDisableNotificationsByUrl(const std::string& id) {};

  virtual void OnShowNotificationSettings(const std::string& id) {};
  virtual void OnShowNotificationSettingsDialog(gfx::NativeView context) {};

  virtual void OnExpanded(const std::string& id) {};
  virtual void OnClicked(const std::string& id) {};
  virtual void OnButtonClicked(const std::string& id, int button_index) {};
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_NOTIFICATION_CHANGE_OBSERVER_H_
