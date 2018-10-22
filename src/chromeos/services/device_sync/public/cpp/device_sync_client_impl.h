// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/cryptauth/remote_device_ref.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace cryptauth {
class ExpiringRemoteDeviceCache;
}  // namespace cryptauth

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {

namespace device_sync {

// Concrete implementation of DeviceSyncClient.
class DeviceSyncClientImpl : public DeviceSyncClient,
                             public device_sync::mojom::DeviceSyncObserver {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetInstanceForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<DeviceSyncClient> BuildInstance(
        service_manager::Connector* connector);

   private:
    static Factory* test_factory_;
  };

  explicit DeviceSyncClientImpl(service_manager::Connector* connector);
  ~DeviceSyncClientImpl() override;

  // DeviceSyncClient:
  void ForceEnrollmentNow(
      mojom::DeviceSync::ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(mojom::DeviceSync::ForceSyncNowCallback callback) override;
  cryptauth::RemoteDeviceRefList GetSyncedDevices() override;
  base::Optional<cryptauth::RemoteDeviceRef> GetLocalDeviceMetadata() override;
  void SetSoftwareFeatureState(
      const std::string public_key,
      cryptauth::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) override;
  void FindEligibleDevices(cryptauth::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void GetDebugInfo(mojom::DeviceSync::GetDebugInfoCallback callback) override;

  // device_sync::mojom::DeviceSyncObserver:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

 private:
  friend class DeviceSyncClientImplTest;

  DeviceSyncClientImpl(service_manager::Connector* connector,
                       scoped_refptr<base::TaskRunner> task_runner);

  void AttemptToBecomeReady();

  void LoadSyncedDevices();
  void LoadLocalDeviceMetadata();

  void OnGetSyncedDevicesCompleted(
      const base::Optional<std::vector<cryptauth::RemoteDevice>>&
          remote_devices);
  void OnGetLocalDeviceMetadataCompleted(
      const base::Optional<cryptauth::RemoteDevice>& local_device_metadata);
  void OnFindEligibleDevicesCompleted(
      FindEligibleDevicesCallback callback,
      mojom::NetworkRequestResult result_code,
      mojom::FindEligibleDevicesResponsePtr response);

  mojom::DeviceSyncObserverPtr GenerateInterfacePtr();

  void FlushForTesting();

  mojom::DeviceSyncPtr device_sync_ptr_;
  mojo::Binding<mojom::DeviceSyncObserver> binding_;
  std::unique_ptr<cryptauth::ExpiringRemoteDeviceCache> expiring_device_cache_;

  bool waiting_for_synced_devices_ = true;
  bool waiting_for_local_device_metadata_ = true;

  bool pending_notify_enrollment_finished_ = false;
  bool pending_notify_new_synced_devices_ = false;

  base::Optional<std::string> local_device_id_;

  base::WeakPtrFactory<DeviceSyncClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncClientImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_IMPL_H_
