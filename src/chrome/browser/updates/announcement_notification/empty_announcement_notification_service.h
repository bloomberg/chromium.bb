// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_EMPTY_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_EMPTY_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

// Dummy AnnouncementNotificationService that does nothing.
class EmptyAnnouncementNotificationService
    : public AnnouncementNotificationService {
 public:
  EmptyAnnouncementNotificationService();
  ~EmptyAnnouncementNotificationService() override;

 private:
  // AnnouncementNotificationService overrides.
  void MaybeShowNotification() override;

  DISALLOW_COPY_AND_ASSIGN(EmptyAnnouncementNotificationService);
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_EMPTY_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_
