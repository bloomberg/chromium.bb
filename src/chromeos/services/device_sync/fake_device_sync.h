// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_

#include <queue>

#include "base/macros.h"
#include "chromeos/services/device_sync/device_sync_base.h"

namespace chromeos {

namespace device_sync {

// Test double DeviceSync implementation.
class FakeDeviceSync : public DeviceSyncBase {
 public:
  FakeDeviceSync();
  ~FakeDeviceSync() override;

  using DeviceSyncBase::NotifyOnEnrollmentFinished;
  using DeviceSyncBase::NotifyOnNewDevicesSynced;

  void set_force_enrollment_now_completed_success(
      bool force_enrollment_now_completed_success) {
    force_enrollment_now_completed_success_ =
        force_enrollment_now_completed_success;
  }

  void set_force_sync_now_completed_success(
      bool force_sync_now_completed_success) {
    force_sync_now_completed_success_ = force_sync_now_completed_success;
  }

  void InvokePendingGetLocalDeviceMetadataCallback(
      const base::Optional<multidevice::RemoteDevice>& local_device_metadata);
  void InvokePendingGetSyncedDevicesCallback(
      const base::Optional<std::vector<multidevice::RemoteDevice>>&
          remote_devices);
  void InvokePendingSetSoftwareFeatureStateCallback(
      mojom::NetworkRequestResult result_code);
  void InvokePendingSetFeatureStatusCallback(
      mojom::NetworkRequestResult result_code);
  void InvokePendingFindEligibleDevicesCallback(
      mojom::NetworkRequestResult result_code,
      mojom::FindEligibleDevicesResponsePtr find_eligible_devices_response_ptr);
  void InvokePendingNotifyDevicesCallback(
      mojom::NetworkRequestResult result_code);
  void InvokePendingGetDevicesActivityStatusCallback(
      mojom::NetworkRequestResult result_code,
      base::Optional<std::vector<mojom::DeviceActivityStatusPtr>>
          get_devices_activity_status_response);
  void InvokePendingGetDebugInfoCallback(mojom::DebugInfoPtr debug_info_ptr);

 protected:
  // mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(ForceSyncNowCallback callback) override;
  void GetLocalDeviceMetadata(GetLocalDeviceMetadataCallback callback) override;
  void GetSyncedDevices(GetSyncedDevicesCallback callback) override;
  void SetSoftwareFeatureState(
      const std::string& device_public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      SetSoftwareFeatureStateCallback callback) override;
  void SetFeatureStatus(const std::string& device_instance_id,
                        multidevice::SoftwareFeature feature,
                        FeatureStatusChange status_change,
                        SetFeatureStatusCallback callback) override;
  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void NotifyDevices(const std::vector<std::string>& device_instance_ids,
                     cryptauthv2::TargetService target_service,
                     multidevice::SoftwareFeature feature,
                     NotifyDevicesCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void GetDevicesActivityStatus(
      GetDevicesActivityStatusCallback callback) override;

 private:
  bool force_enrollment_now_completed_success_ = true;
  bool force_sync_now_completed_success_ = true;

  std::queue<GetLocalDeviceMetadataCallback>
      get_local_device_metadata_callback_queue_;
  std::queue<GetSyncedDevicesCallback> get_synced_devices_callback_queue_;
  std::queue<SetSoftwareFeatureStateCallback>
      set_software_feature_state_callback_queue_;
  std::queue<SetFeatureStatusCallback> set_feature_status_callback_queue_;
  std::queue<FindEligibleDevicesCallback> find_eligible_devices_callback_queue_;
  std::queue<NotifyDevicesCallback> notify_devices_callback_queue_;
  std::queue<GetDevicesActivityStatusCallback>
      get_devices_activity_status_callback_queue_;
  std::queue<GetDebugInfoCallback> get_debug_info_callback_queue_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceSync);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_
