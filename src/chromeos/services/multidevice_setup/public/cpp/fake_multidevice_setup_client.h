// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_CLIENT_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_CLIENT_H_

#include <memory>
#include <queue>
#include <string>
#include <tuple>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// Test double implementation of MultiDeviceSetupClient.
class FakeMultiDeviceSetupClient : public MultiDeviceSetupClient {
 public:
  FakeMultiDeviceSetupClient();
  ~FakeMultiDeviceSetupClient() override;

  void SetHostStatusWithDevice(
      const HostStatusWithDevice& host_status_with_device);
  void SetFeatureStates(const FeatureStatesMap& feature_states_map);
  void SetFeatureState(mojom::Feature feature,
                       mojom::FeatureState feature_state);

  void InvokePendingGetEligibleHostDevicesCallback(
      const multidevice::RemoteDeviceRefList& eligible_devices);
  void InvokePendingSetHostDeviceCallback(
      const std::string& expected_instance_id_or_legacy_device_id,
      const std::string& expected_auth_token,
      bool success);
  void InvokePendingSetFeatureEnabledStateCallback(
      mojom::Feature expected_feature,
      bool expected_enabled,
      const base::Optional<std::string>& expected_auth_token,
      bool success);
  void InvokePendingRetrySetHostNowCallback(bool success);
  void InvokePendingTriggerEventForDebuggingCallback(
      mojom::EventTypeForDebugging expected_type,
      bool success);

  size_t num_remove_host_device_called() {
    return num_remove_host_device_called_;
  }

  // MultiDeviceSetupClient:
  const HostStatusWithDevice& GetHostStatus() const override;
  const FeatureStatesMap& GetFeatureStates() const override;

 private:
  // MultiDeviceSetupClient:
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void SetHostDevice(
      const std::string& host_instance_id_or_legacy_device_id,
      const std::string& auth_token,
      mojom::MultiDeviceSetup::SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void SetFeatureEnabledState(
      mojom::Feature feature,
      bool enabled,
      const base::Optional<std::string>& auth_token,
      mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback)
      override;
  void RetrySetHostNow(
      mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback)
      override;

  size_t num_remove_host_device_called_ = 0u;

  std::queue<GetEligibleHostDevicesCallback>
      get_eligible_host_devices_callback_queue_;
  std::queue<std::tuple<std::string,
                        std::string,
                        mojom::MultiDeviceSetup::SetHostDeviceCallback>>
      set_host_args_queue_;
  std::queue<
      std::tuple<mojom::Feature,
                 bool,
                 base::Optional<std::string>,
                 mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback>>
      set_feature_enabled_state_args_queue_;
  std::queue<mojom::MultiDeviceSetup::RetrySetHostNowCallback>
      retry_set_host_now_callback_queue_;
  std::queue<
      std::pair<mojom::EventTypeForDebugging,
                mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback>>
      trigger_event_for_debugging_type_and_callback_queue_;

  HostStatusWithDevice host_status_with_device_;
  FeatureStatesMap feature_states_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiDeviceSetupClient);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_CLIENT_H_
