// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SERVICE_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SERVICE_H_

#include <memory>
#include "chrome/browser/ash/android_sms/android_sms_app_manager_impl.h"
#include "chrome/browser/ash/android_sms/android_sms_pairing_state_tracker_impl.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"

class HostContentSettingsMap;
class Profile;

namespace app_list {
class AppListSyncableService;
}  // namespace app_list

namespace web_app {
class WebAppProvider;
}  // namespace web_app

namespace ash {
namespace android_sms {

class AndroidSmsAppManager;
class AndroidSmsAppSetupController;
class ConnectionManager;

// KeyedService which manages Android Messages integration. This service
// has four main responsibilities:
//   (1) Maintaining a connection with the Messages ServiceWorker,
//   (2) Managing installation/launching of the Messages PWA,
//   (3) Tracking the pairing state of the PWA, and
//   (4) Notifying users when their phones need to be re-paired.
class AndroidSmsService : public KeyedService,
                          public session_manager::SessionManagerObserver {
 public:
  AndroidSmsService(
      Profile* profile,
      HostContentSettingsMap* host_content_settings_map,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      web_app::WebAppProvider* web_app_provider,
      app_list::AppListSyncableService* app_list_syncable_service);

  AndroidSmsService(const AndroidSmsService&) = delete;
  AndroidSmsService& operator=(const AndroidSmsService&) = delete;

  ~AndroidSmsService() override;

  AndroidSmsAppManager* android_sms_app_manager() {
    return android_sms_app_manager_.get();
  }

  multidevice_setup::AndroidSmsPairingStateTracker*
  android_sms_pairing_state_tracker() {
    return android_sms_pairing_state_tracker_.get();
  }

 private:
  // KeyedService:
  void Shutdown() override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

  Profile* profile_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;

  std::unique_ptr<AndroidSmsAppSetupController>
      andoid_sms_app_setup_controller_;
  std::unique_ptr<AndroidSmsAppManager> android_sms_app_manager_;
  std::unique_ptr<AndroidSmsPairingStateTrackerImpl>
      android_sms_pairing_state_tracker_;
  std::unique_ptr<ConnectionManager> connection_manager_;
};

}  // namespace android_sms
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace android_sms {
using ::ash::android_sms::AndroidSmsService;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SERVICE_H_
