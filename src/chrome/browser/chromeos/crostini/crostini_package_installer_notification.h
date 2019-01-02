// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_INSTALLER_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_INSTALLER_NOTIFICATION_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace message_center {
class Notification;
}

namespace crostini {

class CrostiniPackageInstallerService;

class CrostiniPackageInstallerNotification
    : public message_center::NotificationObserver {
 public:
  CrostiniPackageInstallerNotification(
      Profile* profile,
      const std::string& notification_id,
      CrostiniPackageInstallerService* installer_service);
  virtual ~CrostiniPackageInstallerNotification();

  void UpdateProgress(InstallLinuxPackageProgressStatus result,
                      int progress_percent,
                      const std::string& failure_reason);

  // message_center::NotificationObserver:
  void Close(bool by_user) override;

 private:
  void UpdateDisplayedNotification();

  // These notifications are owned by the installer service.
  CrostiniPackageInstallerService* installer_service_;
  Profile* profile_;

  std::unique_ptr<message_center::Notification> notification_;

  base::WeakPtrFactory<CrostiniPackageInstallerNotification> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniPackageInstallerNotification);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_INSTALLER_NOTIFICATION_H_
