// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_INTERFACE_H_

#include "base/callback.h"
#include "base/macros.h"

namespace chromeos {
namespace file_system_provider {

// Handles notifications related to provided the file system.
class NotificationManagerInterface {
 public:
  // Result of a notification.
  enum NotificationResult { ABORT, CONTINUE };

  // Callback for handling result of a notification.
  typedef base::Callback<void(NotificationResult)> NotificationCallback;

  NotificationManagerInterface() {}
  virtual ~NotificationManagerInterface() {}

  // Shows a notification about the request being unresponsive. The |callback|
  // is called when the notification is closed.
  virtual void ShowUnresponsiveNotification(
      int id,
      const NotificationCallback& callback) = 0;

  // Hides a notification previously shown with |id|.
  virtual void HideUnresponsiveNotification(int id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationManagerInterface);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_INTERFACE_H_
