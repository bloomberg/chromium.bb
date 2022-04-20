// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_IMPL_H_
#define ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_IMPL_H_

#include "ash/components/phonehub/multidevice_feature_access_manager.h"

#include "ash/components/phonehub/feature_status_provider.h"
#include "ash/components/phonehub/message_receiver.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace phonehub {

class ConnectionScheduler;
class MessageSender;

// Implements MultideviceFeatureAccessManager by persisting the last-known
// notification access status and camera roll access status to user prefs.
class MultideviceFeatureAccessManagerImpl
    : public MultideviceFeatureAccessManager,
      public FeatureStatusProvider::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit MultideviceFeatureAccessManagerImpl(
      PrefService* pref_service,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      FeatureStatusProvider* feature_status_provider,
      MessageSender* message_sender,
      ConnectionScheduler* connection_scheduler);
  ~MultideviceFeatureAccessManagerImpl() override;

 private:
  friend class MultideviceFeatureAccessManagerImplTest;

  // MultideviceFeatureAccessManager:
  AccessStatus GetNotificationAccessStatus() const override;
  AccessProhibitedReason GetNotificationAccessProhibitedReason() const override;
  void SetNotificationAccessStatusInternal(
      AccessStatus access_status,
      AccessProhibitedReason reason) override;
  void SetCameraRollAccessStatusInternal(AccessStatus access_status) override;
  AccessStatus GetCameraRollAccessStatus() const override;
  AccessStatus GetAppsAccessStatus() const override;
  bool IsAccessRequestAllowed(
      multidevice_setup::mojom::Feature feature) override;
  bool GetFeatureSetupRequestSupported() const override;
  void SetFeatureSetupRequestSupportedInternal(bool supported) override;
  void OnNotificationSetupRequested() override;
  void OnCombinedSetupRequested(bool camera_roll, bool notifications) override;

  bool HasMultideviceFeatureSetupUiBeenDismissed() const override;
  void DismissSetupRequiredUi() override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  void FeatureStatusChangedNotificationAccessSetup();
  void FeatureStatusChangedCombinedAccessSetup();

  void SendShowNotificationAccessSetupRequest();
  void SendShowCombinedAccessSetupRequest();

  bool HasAccessStatusChanged(AccessStatus access_status,
                              AccessProhibitedReason reason);

  FeatureStatus current_feature_status_;
  PrefService* pref_service_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  FeatureStatusProvider* feature_status_provider_;
  MessageSender* message_sender_;
  ConnectionScheduler* connection_scheduler_;

  // Registers preference value change listeners.
  PrefChangeRegistrar pref_change_registrar_;

  bool combined_setup_notifications_pending_ = false;
  bool combined_setup_camera_roll_pending_ = false;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_IMPL_H_
