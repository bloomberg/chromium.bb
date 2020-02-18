// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NON_PERSISTENT_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NON_PERSISTENT_NOTIFICATION_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_handler.h"

// NotificationHandler implementation for non persistent notifications.
class NonPersistentNotificationHandler : public NotificationHandler {
 public:
  NonPersistentNotificationHandler();
  ~NonPersistentNotificationHandler() override;

  // NotificationHandler implementation
  void OnShow(Profile* profile, const std::string& notification_id) override;
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const base::Optional<int>& action_index,
               const base::Optional<base::string16>& reply,
               base::OnceClosure completed_closure) override;
  void DisableNotifications(Profile* profile, const GURL& origin) override;
  void OpenSettings(Profile* profile, const GURL& origin) override;

 private:
  // Called when the "click" event for non-persistent notification has been
  // dispatched. The |success| boolean indicates whether the click could be
  // delivered to the originating document as a JavaScript event.
  void DidDispatchClickEvent(Profile* profile,
                             const GURL& origin,
                             const std::string& notification_id,
                             base::OnceClosure completed_closure,
                             bool success);

  base::WeakPtrFactory<NonPersistentNotificationHandler> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(NonPersistentNotificationHandler);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NON_PERSISTENT_NOTIFICATION_HANDLER_H_
