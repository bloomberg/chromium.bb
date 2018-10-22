// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_PLATFORM_NOTIFICATION_SERVICE_H_
#define CONTENT_TEST_MOCK_PLATFORM_NOTIFICATION_SERVICE_H_

#include <stdint.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"
#include "url/gurl.h"

namespace content {

struct NotificationResources;
struct PlatformNotificationData;

// Responsible for tracking active notifications and allowed origins for the
// Web Notification API when running layout and content tests.
class MockPlatformNotificationService : public PlatformNotificationService {
 public:
  MockPlatformNotificationService();
  ~MockPlatformNotificationService() override;

  // Simulates a click on the notification titled |title|. |action_index|
  // indicates which action was clicked. |reply| indicates the user reply.
  // Must be called on the UI thread.
  void SimulateClick(const std::string& title,
                     const base::Optional<int>& action_index,
                     const base::Optional<base::string16>& reply);

  // Simulates the closing a notification titled |title|. Must be called on
  // the UI thread.
  void SimulateClose(const std::string& title, bool by_user);

  // PlatformNotificationService implementation.
  void DisplayNotification(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources) override;
  void DisplayPersistentNotification(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& service_worker_scope,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources) override;
  void CloseNotification(BrowserContext* browser_context,
                         const std::string& notification_id) override;
  void ClosePersistentNotification(BrowserContext* browser_context,
                                   const std::string& notification_id) override;
  void GetDisplayedNotifications(
      BrowserContext* browser_context,
      const DisplayedNotificationsCallback& callback) override;
  int64_t ReadNextPersistentNotificationId(
      BrowserContext* browser_context) override;
  void RecordNotificationUkmEvent(
      BrowserContext* browser_context,
      const NotificationDatabaseData& data) override;

 private:
  // Structure to represent the information of a persistent notification.
  struct PersistentNotification {
    BrowserContext* browser_context = nullptr;
    GURL origin;
  };

  // Fakes replacing the notification identified by |notification_id|. Both
  // persistent and non-persistent notifications will be considered for this.
  void ReplaceNotificationIfNeeded(const std::string& notification_id);

  std::unordered_map<std::string, PersistentNotification>
      persistent_notifications_;
  std::unordered_set<std::string> non_persistent_notifications_;

  // Mapping of titles to notification ids giving test a usable identifier.
  std::unordered_map<std::string, std::string> notification_id_map_;

  int64_t next_persistent_notification_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(MockPlatformNotificationService);
};

}  // content

#endif  // CONTENT_TEST_MOCK_PLATFORM_NOTIFICATION_SERVICE_H_
