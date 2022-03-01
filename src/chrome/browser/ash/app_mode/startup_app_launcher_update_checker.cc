// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/startup_app_launcher_update_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/syslog_logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/updater/extension_downloader.h"

namespace ash {

StartupAppLauncherUpdateChecker::StartupAppLauncherUpdateChecker(
    Profile* profile)
    : profile_(profile) {}

StartupAppLauncherUpdateChecker::~StartupAppLauncherUpdateChecker() = default;

bool StartupAppLauncherUpdateChecker::Run(UpdateCheckCallback callback) {
  if (callback_) {
    DLOG(WARNING) << "Running multiple update check is not supported.";
    return false;
  }

  extensions::ExtensionUpdater* updater =
      extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->updater();
  if (!updater)
    return false;

  callback_ = std::move(callback);

  update_found_ = false;
  registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
                 content::NotificationService::AllSources());

  extensions::ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  params.callback =
      base::BindOnce(&StartupAppLauncherUpdateChecker::OnExtensionUpdaterDone,
                     weak_ptr_factory_.GetWeakPtr());
  updater->CheckNow(std::move(params));
  return true;
}

void StartupAppLauncherUpdateChecker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND, type);

  const content::Details<extensions::UpdateDetails> update_details(details);
  SYSLOG(INFO) << "Found extension update id=" << update_details->id
               << " version=" << update_details->version.GetString();
  update_found_ = true;
}

void StartupAppLauncherUpdateChecker::OnExtensionUpdaterDone() {
  registrar_.Remove(this, extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
                    content::NotificationService::AllSources());

  // It is not safe to use |this| after the callback has been run.
  std::move(callback_).Run(update_found_);
}

}  // namespace ash
