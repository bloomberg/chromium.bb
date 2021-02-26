// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_UPDATE_REQUIRED_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_UI_UPDATE_REQUIRED_NOTIFICATION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace chromeos {

// UpdateRequiredNotification manages in-session notifications informing the
// user that update is required as per admin policy but it cannot be initiated
// either due to network limitations or because of the device having reached its
// end-of-life state.
class UpdateRequiredNotification : public message_center::NotificationObserver {
 public:
  UpdateRequiredNotification();
  UpdateRequiredNotification(const UpdateRequiredNotification&) = delete;
  UpdateRequiredNotification& operator=(const UpdateRequiredNotification&) =
      delete;

  virtual ~UpdateRequiredNotification();

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;

  // Collects notification data like title, body, button text, priority on the
  // basis of |type| and |warning_time|. Sets the |button_click_callback| to be
  // invoked when the notification button is clicked and |close_callback| to be
  // invoked when the notification is closed.
  void Show(policy::MinimumVersionPolicyHandler::NotificationType type,
            base::TimeDelta warning_time,
            const std::string& domain_name,
            const base::string16& device_type,
            base::OnceClosure button_click_callback,
            base::OnceClosure close_callback);

  void Hide();

 private:
  // Creates and displays a new notification.
  void DisplayNotification(
      const base::string16& title,
      const base::string16& message,
      const base::string16& button_text,
      const message_center::SystemNotificationWarningLevel color_type,
      const message_center::NotificationPriority priority);

  // Callback to be invoked when the notification button is clicked.
  base::OnceClosure notification_button_click_callback_;

  // Callback to be invoked when the notification is closed.
  base::OnceClosure notification_close_callback_;

  base::WeakPtrFactory<UpdateRequiredNotification> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_UPDATE_REQUIRED_NOTIFICATION_H_
