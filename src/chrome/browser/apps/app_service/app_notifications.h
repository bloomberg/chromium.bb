// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_NOTIFICATIONS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_NOTIFICATIONS_H_

#include <map>
#include <set>
#include <string>

#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps {

// AppNotifications records the notification id for each app.
class AppNotifications {
 public:
  AppNotifications();
  ~AppNotifications();

  AppNotifications(const AppNotifications&) = delete;
  AppNotifications& operator=(const AppNotifications&) = delete;

  // Records that |app_id| has a new notification identified by
  // |notification_id|.
  void AddNotification(const std::string& app_id,
                       const std::string& notification_id);

  // Removes the notification for the given |notification_id|.
  void RemoveNotification(const std::string& notification_id);

  // Returns true, if the app has notifications. Otherwise, returns false.
  bool HasNotification(const std::string& app_id);

  // Returns the app id for the given |notification_id|, if |notification_id|
  // exists in |notification_id_to_app_id_|. Otherwise, return an empty string.
  const std::string GetAppIdForNotification(const std::string& notification_id);

  apps::mojom::AppPtr GetAppWithHasBadgeStatus(apps::mojom::AppType app_type,
                                               const std::string& app_id);

 private:
  // Maps one app id to a set of all matching notification ids.
  std::map<std::string, std::set<std::string>> app_id_to_notification_id_;

  // Maps one notification id to one app id. When the notification has been
  // delivered, the MessageCenter has already deleted the notification, so we
  // can't fetch the corresponding app id when the notification is removed. So
  // we need a record of this notification, and erase it from both
  // |app_id_to_notification_id_| and |notification_id_to_app_id_|.
  std::map<std::string, std::string> notification_id_to_app_id_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_NOTIFICATIONS_H_
