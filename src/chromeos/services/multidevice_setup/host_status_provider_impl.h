// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"
#include "chromeos/services/multidevice_setup/host_verifier.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

class EligibleHostDevicesProvider;

// Concrete HostStatusProvider implementation. This class listens for events
// from HostBackendDelegate, HostVerifier, and DeviceSyncClient to determine
// when the status of the host has changed.
class HostStatusProviderImpl : public HostStatusProvider,
                               public HostBackendDelegate::Observer,
                               public HostVerifier::Observer,
                               public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<HostStatusProvider> BuildInstance(
        EligibleHostDevicesProvider* eligible_host_devices_provider,
        HostBackendDelegate* host_backend_delegate,
        HostVerifier* host_verifier,
        device_sync::DeviceSyncClient* device_sync_client);

   private:
    static Factory* test_factory_;
  };

  ~HostStatusProviderImpl() override;

 private:
  HostStatusProviderImpl(
      EligibleHostDevicesProvider* eligible_host_devices_provider,
      HostBackendDelegate* host_backend_delegate,
      HostVerifier* host_verifier,
      device_sync::DeviceSyncClient* device_sync_client);

  // HostStatusProvider:
  HostStatusWithDevice GetHostWithStatus() const override;

  // HostBackendDelegate::Observer:
  void OnHostChangedOnBackend() override;
  void OnPendingHostRequestChange() override;

  // HostVerifier::Observer:
  void OnHostVerified() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  void CheckForUpdatedStatusAndNotifyIfChanged();
  HostStatusWithDevice GetCurrentStatus();

  EligibleHostDevicesProvider* eligible_host_devices_provider_;
  HostBackendDelegate* host_backend_delegate_;
  HostVerifier* host_verifier_;
  device_sync::DeviceSyncClient* device_sync_client_;

  HostStatusWithDevice current_status_and_device_;

  DISALLOW_COPY_AND_ASSIGN(HostStatusProviderImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_IMPL_H_
