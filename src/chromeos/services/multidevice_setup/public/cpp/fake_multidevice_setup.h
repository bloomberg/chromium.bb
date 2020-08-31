// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_H_

#include <tuple>
#include <utility>
#include <vector>

#include "chromeos/services/multidevice_setup/multidevice_setup_base.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

namespace multidevice_setup {

// Test MultiDeviceSetup implementation.
class FakeMultiDeviceSetup : public MultiDeviceSetupBase {
 public:
  FakeMultiDeviceSetup();
  ~FakeMultiDeviceSetup() override;

  void FlushForTesting();

  bool HasAtLeastOneHostStatusObserver();
  bool HasAtLeastOneFeatureStateObserver();

  void NotifyHostStatusChanged(
      mojom::HostStatus host_status,
      const base::Optional<multidevice::RemoteDevice>& host_device);
  void NotifyFeatureStateChanged(
      const base::flat_map<mojom::Feature, mojom::FeatureState>&
          feature_states);

  mojo::Remote<mojom::AccountStatusChangeDelegate>& delegate() {
    return delegate_;
  }

  std::vector<GetEligibleHostDevicesCallback>& get_eligible_hosts_args() {
    return get_eligible_hosts_args_;
  }

  std::vector<std::tuple<std::string, std::string, SetHostDeviceCallback>>&
  set_host_args() {
    return set_host_args_;
  }

  size_t num_remove_host_calls() { return num_remove_host_calls_; }

  std::vector<GetHostStatusCallback>& get_host_args() { return get_host_args_; }

  std::vector<std::tuple<mojom::Feature,
                         bool,
                         base::Optional<std::string>,
                         SetFeatureEnabledStateCallback>>&
  set_feature_enabled_args() {
    return set_feature_enabled_args_;
  }

  std::vector<GetFeatureStatesCallback>& get_feature_states_args() {
    return get_feature_states_args_;
  }

  std::vector<RetrySetHostNowCallback>& retry_set_host_now_args() {
    return retry_set_host_now_args_;
  }

  std::vector<std::pair<mojom::EventTypeForDebugging,
                        TriggerEventForDebuggingCallback>>&
  triggered_debug_events() {
    return triggered_debug_events_;
  }

  std::vector<
      std::pair<std::string,
                mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback>>&
  set_host_without_auth_args() {
    return set_host_without_auth_args_;
  }

 private:
  // mojom::MultiDeviceSetup:
  void SetAccountStatusChangeDelegate(
      mojo::PendingRemote<mojom::AccountStatusChangeDelegate> delegate)
      override;
  void AddHostStatusObserver(
      mojo::PendingRemote<mojom::HostStatusObserver> observer) override;
  void AddFeatureStateObserver(
      mojo::PendingRemote<mojom::FeatureStateObserver> observer) override;
  void GetEligibleHostDevices(GetEligibleHostDevicesCallback callback) override;
  void GetEligibleActiveHostDevices(
      GetEligibleActiveHostDevicesCallback callback) override;
  void SetHostDevice(const std::string& host_instance_id_or_legacy_device_id,
                     const std::string& auth_token,
                     SetHostDeviceCallback callback) override;
  void RemoveHostDevice() override;
  void GetHostStatus(GetHostStatusCallback callback) override;
  void SetFeatureEnabledState(mojom::Feature feature,
                              bool enabled,
                              const base::Optional<std::string>& auth_token,
                              SetFeatureEnabledStateCallback callback) override;
  void GetFeatureStates(GetFeatureStatesCallback callback) override;
  void RetrySetHostNow(RetrySetHostNowCallback callback) override;
  void TriggerEventForDebugging(
      mojom::EventTypeForDebugging type,
      TriggerEventForDebuggingCallback callback) override;

  // MultiDeviceSetupBase:
  void SetHostDeviceWithoutAuthToken(
      const std::string& host_instance_id_or_legacy_device_id,
      mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback callback)
      override;

  mojo::Remote<mojom::AccountStatusChangeDelegate> delegate_;
  mojo::RemoteSet<mojom::HostStatusObserver> host_status_observers_;
  mojo::RemoteSet<mojom::FeatureStateObserver> feature_state_observers_;

  std::vector<GetEligibleHostDevicesCallback> get_eligible_hosts_args_;
  std::vector<GetEligibleActiveHostDevicesCallback>
      get_eligible_active_hosts_args_;
  std::vector<std::tuple<std::string, std::string, SetHostDeviceCallback>>
      set_host_args_;
  size_t num_remove_host_calls_ = 0u;
  std::vector<GetHostStatusCallback> get_host_args_;
  std::vector<std::tuple<mojom::Feature,
                         bool,
                         base::Optional<std::string>,
                         SetFeatureEnabledStateCallback>>
      set_feature_enabled_args_;
  std::vector<GetFeatureStatesCallback> get_feature_states_args_;
  std::vector<RetrySetHostNowCallback> retry_set_host_now_args_;
  std::vector<
      std::pair<mojom::EventTypeForDebugging, TriggerEventForDebuggingCallback>>
      triggered_debug_events_;
  std::vector<
      std::pair<std::string,
                mojom::PrivilegedHostDeviceSetter::SetHostDeviceCallback>>
      set_host_without_auth_args_;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiDeviceSetup);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_MULTIDEVICE_SETUP_H_
