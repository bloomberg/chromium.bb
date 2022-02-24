// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_
#define ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_

#include <memory>

#include "ash/components/phonehub/phone_hub_manager.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/services/secure_channel/public/cpp/client/connection_manager.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "base/callback.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace ash {
namespace phonehub {

class BrowserTabsModelController;
class BrowserTabsModelProvider;
class CameraRollDownloadManager;
class CameraRollManager;
class CrosStateSender;
class InvalidConnectionDisconnector;
class MessageReceiver;
class MessageSender;
class MultideviceSetupStateUpdater;
class MutablePhoneModel;
class NotificationProcessor;
class PhoneStatusProcessor;
class UserActionRecorder;

// Implemented as a KeyedService which is keyed by the primary Profile.
class PhoneHubManagerImpl : public PhoneHubManager, public KeyedService {
 public:
  PhoneHubManagerImpl(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<BrowserTabsModelProvider> browser_tabs_model_provider,
      std::unique_ptr<CameraRollDownloadManager> camera_roll_download_manager,
      const base::RepeatingClosure& show_multidevice_setup_dialog_callback);

  ~PhoneHubManagerImpl() override;

  // PhoneHubManager:
  BrowserTabsModelProvider* GetBrowserTabsModelProvider() override;
  CameraRollManager* GetCameraRollManager() override;
  ConnectionScheduler* GetConnectionScheduler() override;
  DoNotDisturbController* GetDoNotDisturbController() override;
  FeatureStatusProvider* GetFeatureStatusProvider() override;
  FindMyDeviceController* GetFindMyDeviceController() override;
  NotificationAccessManager* GetNotificationAccessManager() override;
  NotificationInteractionHandler* GetNotificationInteractionHandler() override;
  NotificationManager* GetNotificationManager() override;
  OnboardingUiTracker* GetOnboardingUiTracker() override;
  PhoneModel* GetPhoneModel() override;
  RecentAppsInteractionHandler* GetRecentAppsInteractionHandler() override;
  ScreenLockManager* GetScreenLockManager() override;
  TetherController* GetTetherController() override;
  UserActionRecorder* GetUserActionRecorder() override;

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<secure_channel::ConnectionManager> connection_manager_;
  std::unique_ptr<FeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<UserActionRecorder> user_action_recorder_;
  std::unique_ptr<MessageReceiver> message_receiver_;
  std::unique_ptr<MessageSender> message_sender_;
  std::unique_ptr<MutablePhoneModel> phone_model_;
  std::unique_ptr<CrosStateSender> cros_state_sender_;
  std::unique_ptr<DoNotDisturbController> do_not_disturb_controller_;
  std::unique_ptr<ConnectionScheduler> connection_scheduler_;
  std::unique_ptr<FindMyDeviceController> find_my_device_controller_;
  std::unique_ptr<NotificationAccessManager> notification_access_manager_;
  std::unique_ptr<ScreenLockManager> screen_lock_manager_;
  std::unique_ptr<NotificationInteractionHandler>
      notification_interaction_handler_;
  std::unique_ptr<NotificationManager> notification_manager_;
  std::unique_ptr<OnboardingUiTracker> onboarding_ui_tracker_;
  std::unique_ptr<NotificationProcessor> notification_processor_;
  std::unique_ptr<RecentAppsInteractionHandler>
      recent_apps_interaction_handler_;
  std::unique_ptr<PhoneStatusProcessor> phone_status_processor_;
  std::unique_ptr<TetherController> tether_controller_;
  std::unique_ptr<BrowserTabsModelProvider> browser_tabs_model_provider_;
  std::unique_ptr<BrowserTabsModelController> browser_tabs_model_controller_;
  std::unique_ptr<MultideviceSetupStateUpdater>
      multidevice_setup_state_updater_;
  std::unique_ptr<InvalidConnectionDisconnector>
      invalid_connection_disconnector_;
  std::unique_ptr<CameraRollManager> camera_roll_manager_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_
