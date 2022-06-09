// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_NOTIFICATION_CONTROLLER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

namespace multidevice_setup {

// Pref to track whether the announcement notification can be shown the next
// time the device is unlocked with a verified host and wi-fi sync supported but
// disabled.
extern const char kCanShowWifiSyncAnnouncementPrefName[];

class AccountStatusChangeDelegateNotifier;
class GlobalStateFeatureManager;
class HostStatusProvider;

// Controls the setup notification for Wifi Sync.
class WifiSyncNotificationController
    : public base::PowerSuspendObserver,
      public session_manager::SessionManagerObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<WifiSyncNotificationController> Create(
        GlobalStateFeatureManager* wifi_sync_feature_manager,
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        AccountStatusChangeDelegateNotifier* delegate_notifier);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<WifiSyncNotificationController> CreateInstance(
        GlobalStateFeatureManager* wifi_sync_feature_manager,
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        AccountStatusChangeDelegateNotifier* delegate_notifier) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~WifiSyncNotificationController() override;
  WifiSyncNotificationController(const WifiSyncNotificationController&) =
      delete;
  WifiSyncNotificationController& operator=(
      const WifiSyncNotificationController&) = delete;

 private:
  WifiSyncNotificationController(
      GlobalStateFeatureManager* wifi_sync_feature_manager,
      HostStatusProvider* host_status_provider,
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      AccountStatusChangeDelegateNotifier* delegate_notifier);

  // SessionManagerObserver:
  void OnSessionStateChanged() override;

  // PowerSuspendObserver:
  void OnResume() override;

  void ShowAnnouncementNotificationIfEligible();
  bool IsWifiSyncSupported();

  GlobalStateFeatureManager* wifi_sync_feature_manager_;
  HostStatusProvider* host_status_provider_;
  PrefService* pref_service_;
  device_sync::DeviceSyncClient* device_sync_client_;
  AccountStatusChangeDelegateNotifier* delegate_notifier_;

  bool did_register_session_observers_ = false;

  base::WeakPtrFactory<WifiSyncNotificationController> weak_ptr_factory_{this};
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_NOTIFICATION_CONTROLLER_H_
