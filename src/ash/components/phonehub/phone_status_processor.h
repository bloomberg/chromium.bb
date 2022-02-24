// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_
#define ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_

#include <google/protobuf/repeated_field.h>

#include "ash/components/phonehub/feature_status_provider.h"
#include "ash/components/phonehub/message_receiver.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace ash {
namespace phonehub {

using ::google::protobuf::RepeatedPtrField;

class DoNotDisturbController;
class FindMyDeviceController;
class MutablePhoneModel;
class NotificationAccessManager;
class NotificationProcessor;
class ScreenLockManager;
class RecentAppsInteractionHandler;

// Responsible for receiving incoming protos and calling on clients to update
// their models.
class PhoneStatusProcessor
    : public MessageReceiver::Observer,
      public FeatureStatusProvider::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  PhoneStatusProcessor(
      DoNotDisturbController* do_not_disturb_controller,
      FeatureStatusProvider* feature_status_provider,
      MessageReceiver* message_receiver,
      FindMyDeviceController* find_my_device_controller,
      NotificationAccessManager* notification_access_manager,
      ScreenLockManager* screen_lock_manager,
      NotificationProcessor* notification_processor_,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      MutablePhoneModel* phone_model,
      RecentAppsInteractionHandler* recent_apps_interaction_handler);
  ~PhoneStatusProcessor() override;

  PhoneStatusProcessor(const PhoneStatusProcessor&) = delete;
  PhoneStatusProcessor& operator=(const PhoneStatusProcessor&) = delete;

 private:
  friend class PhoneStatusProcessorTest;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // MessageReceiver::Observer:
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override;
  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override;

  // MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;

  void ProcessReceivedNotifications(
      const RepeatedPtrField<proto::Notification>& notification_protos);

  void SetReceivedPhoneStatusModelStates(
      const proto::PhoneProperties& phone_properties);

  void MaybeSetPhoneModelName(
      const absl::optional<multidevice::RemoteDeviceRef>& remote_device);

  void SetDoNotDisturbState(proto::NotificationMode mode);

  DoNotDisturbController* do_not_disturb_controller_;
  FeatureStatusProvider* feature_status_provider_;
  MessageReceiver* message_receiver_;
  FindMyDeviceController* find_my_device_controller_;
  NotificationAccessManager* notification_access_manager_;
  ScreenLockManager* screen_lock_manager_;
  NotificationProcessor* notification_processor_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  MutablePhoneModel* phone_model_;
  RecentAppsInteractionHandler* recent_apps_interaction_handler_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_PROCESSOR_H_
