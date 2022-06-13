// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_RELEASE_NOTES_RELEASE_NOTES_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_RELEASE_NOTES_RELEASE_NOTES_NOTIFICATION_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

class ReleaseNotesNotificationTest;

// Notifies user when release notes are available for their
// recently updated CrOS device.
class ReleaseNotesNotification {
 public:
  explicit ReleaseNotesNotification(Profile* profile);

  ReleaseNotesNotification(const ReleaseNotesNotification&) = delete;
  ReleaseNotesNotification& operator=(const ReleaseNotesNotification&) = delete;

  ~ReleaseNotesNotification();

  // Checks profile preferences and release notes storage to determine whether
  // to show release notes available notification to user.
  void MaybeShowReleaseNotes();

 private:
  friend class ReleaseNotesNotificationTest;

  // Creates notification that the CrOS device has been updated, shows the
  // notification, and registers that a notification was shown for the current
  // profile for the current Chrome release.
  void ShowReleaseNotesNotification();

  void HandleClickShowNotification();

  Profile* const profile_;
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage_;
  std::unique_ptr<message_center::Notification>
      release_notes_available_notification_;

  base::WeakPtrFactory<ReleaseNotesNotification> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromOS code migration is done.
namespace chromeos {
using ::ash::ReleaseNotesNotification;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_RELEASE_NOTES_RELEASE_NOTES_NOTIFICATION_H_
