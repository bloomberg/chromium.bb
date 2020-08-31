// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_status_provider_impl.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/multidevice_setup/eligible_host_devices_provider.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

static void RecordMultiDeviceHostStatus(mojom::HostStatus host_status) {
  UMA_HISTOGRAM_ENUMERATION("MultiDevice.Setup.HostStatus", host_status);
}

}  // namespace

// static
HostStatusProviderImpl::Factory*
    HostStatusProviderImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<HostStatusProvider> HostStatusProviderImpl::Factory::Create(
    EligibleHostDevicesProvider* eligible_host_devices_provider,
    HostBackendDelegate* host_backend_delegate,
    HostVerifier* host_verifier,
    device_sync::DeviceSyncClient* device_sync_client) {
  if (test_factory_) {
    return test_factory_->CreateInstance(eligible_host_devices_provider,
                                         host_backend_delegate, host_verifier,
                                         device_sync_client);
  }

  return base::WrapUnique(new HostStatusProviderImpl(
      eligible_host_devices_provider, host_backend_delegate, host_verifier,
      device_sync_client));
}

// static
void HostStatusProviderImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

HostStatusProviderImpl::Factory::~Factory() = default;

HostStatusProviderImpl::HostStatusProviderImpl(
    EligibleHostDevicesProvider* eligible_host_devices_provider,
    HostBackendDelegate* host_backend_delegate,
    HostVerifier* host_verifier,
    device_sync::DeviceSyncClient* device_sync_client)
    : eligible_host_devices_provider_(eligible_host_devices_provider),
      host_backend_delegate_(host_backend_delegate),
      host_verifier_(host_verifier),
      device_sync_client_(device_sync_client),
      current_status_and_device_(mojom::HostStatus::kNoEligibleHosts,
                                 base::nullopt /* host_device */) {
  host_backend_delegate_->AddObserver(this);
  host_verifier_->AddObserver(this);
  device_sync_client_->AddObserver(this);

  CheckForUpdatedStatusAndNotifyIfChanged();
  RecordMultiDeviceHostStatus(current_status_and_device_.host_status());
}

HostStatusProviderImpl::~HostStatusProviderImpl() {
  host_backend_delegate_->RemoveObserver(this);
  host_verifier_->RemoveObserver(this);
  device_sync_client_->RemoveObserver(this);
}

HostStatusProvider::HostStatusWithDevice
HostStatusProviderImpl::GetHostWithStatus() const {
  return current_status_and_device_;
}

void HostStatusProviderImpl::OnHostChangedOnBackend() {
  CheckForUpdatedStatusAndNotifyIfChanged();
}

void HostStatusProviderImpl::OnPendingHostRequestChange() {
  CheckForUpdatedStatusAndNotifyIfChanged();
}

void HostStatusProviderImpl::OnHostVerified() {
  CheckForUpdatedStatusAndNotifyIfChanged();
}

void HostStatusProviderImpl::OnNewDevicesSynced() {
  CheckForUpdatedStatusAndNotifyIfChanged();
}

void HostStatusProviderImpl::CheckForUpdatedStatusAndNotifyIfChanged() {
  HostStatusWithDevice current_status_and_device = GetCurrentStatus();
  if (current_status_and_device == current_status_and_device_)
    return;

  PA_LOG(VERBOSE) << "HostStatusProviderImpl::"
                  << "CheckForUpdatedStatusAndNotifyIfChanged(): Host status "
                  << "changed. New status: "
                  << current_status_and_device.host_status() << ", Old status: "
                  << current_status_and_device_.host_status()
                  << ", Host device: "
                  << (current_status_and_device.host_device()
                          ? current_status_and_device.host_device()
                                ->GetInstanceIdDeviceIdForLogs()
                          : "[no host]");

  current_status_and_device_ = current_status_and_device;
  NotifyHostStatusChange(current_status_and_device_.host_status(),
                         current_status_and_device_.host_device());
  RecordMultiDeviceHostStatus(current_status_and_device_.host_status());
}

HostStatusProvider::HostStatusWithDevice
HostStatusProviderImpl::GetCurrentStatus() {
  if (host_verifier_->IsHostVerified()) {
    return HostStatusWithDevice(
        mojom::HostStatus::kHostVerified,
        *host_backend_delegate_->GetMultiDeviceHostFromBackend());
  }

  if (host_backend_delegate_->GetMultiDeviceHostFromBackend() &&
      !host_backend_delegate_->HasPendingHostRequest()) {
    return HostStatusWithDevice(
        mojom::HostStatus::kHostSetButNotYetVerified,
        *host_backend_delegate_->GetMultiDeviceHostFromBackend());
  }

  if (host_backend_delegate_->HasPendingHostRequest() &&
      host_backend_delegate_->GetPendingHostRequest()) {
    return HostStatusWithDevice(
        mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
        *host_backend_delegate_->GetPendingHostRequest());
  }

  if (!eligible_host_devices_provider_->GetEligibleHostDevices().empty()) {
    return HostStatusWithDevice(
        mojom::HostStatus::kEligibleHostExistsButNoHostSet,
        base::nullopt /* host_device */);
  }

  return HostStatusWithDevice(mojom::HostStatus::kNoEligibleHosts,
                              base::nullopt /* host_device */);
}

}  // namespace multidevice_setup

}  // namespace chromeos
