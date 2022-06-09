// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_

#include <vector>

#include "chromeos/services/device_sync/cryptauth_device_syncer.h"
#include "chromeos/services/device_sync/cryptauth_device_syncer_impl.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace chromeos {

namespace device_sync {

class CryptAuthDeviceSyncResult;

// Implementation of CryptAuthDeviceSyncer for use in tests.
class FakeCryptAuthDeviceSyncer : public CryptAuthDeviceSyncer {
 public:
  FakeCryptAuthDeviceSyncer();

  FakeCryptAuthDeviceSyncer(const FakeCryptAuthDeviceSyncer&) = delete;
  FakeCryptAuthDeviceSyncer& operator=(const FakeCryptAuthDeviceSyncer&) =
      delete;

  ~FakeCryptAuthDeviceSyncer() override;

  const absl::optional<cryptauthv2::ClientMetadata>& client_metadata() const {
    return client_metadata_;
  }

  const absl::optional<cryptauthv2::ClientAppMetadata>& client_app_metadata()
      const {
    return client_app_metadata_;
  }

  void FinishAttempt(const CryptAuthDeviceSyncResult& device_sync_result);

 private:
  // CryptAuthDeviceSyncer:
  void OnAttemptStarted(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata) override;

  absl::optional<cryptauthv2::ClientMetadata> client_metadata_;
  absl::optional<cryptauthv2::ClientAppMetadata> client_app_metadata_;
};

class FakeCryptAuthDeviceSyncerFactory
    : public CryptAuthDeviceSyncerImpl::Factory {
 public:
  FakeCryptAuthDeviceSyncerFactory();

  FakeCryptAuthDeviceSyncerFactory(const FakeCryptAuthDeviceSyncerFactory&) =
      delete;
  FakeCryptAuthDeviceSyncerFactory& operator=(
      const FakeCryptAuthDeviceSyncerFactory&) = delete;

  ~FakeCryptAuthDeviceSyncerFactory() override;

  const std::vector<FakeCryptAuthDeviceSyncer*>& instances() const {
    return instances_;
  }

  const CryptAuthDeviceRegistry* last_device_registry() const {
    return last_device_registry_;
  }

  const CryptAuthKeyRegistry* last_key_registry() const {
    return last_key_registry_;
  }

  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

  const PrefService* last_pref_service() const { return last_pref_service_; }

 private:
  // CryptAuthDeviceSyncerImpl::Factory:
  std::unique_ptr<CryptAuthDeviceSyncer> CreateInstance(
      CryptAuthDeviceRegistry* device_registry,
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      SyncedBluetoothAddressTracker* synced_bluetooth_address_tracker,
      PrefService* pref_service,
      std::unique_ptr<base::OneShotTimer> timer) override;

  std::vector<FakeCryptAuthDeviceSyncer*> instances_;
  CryptAuthDeviceRegistry* last_device_registry_ = nullptr;
  CryptAuthKeyRegistry* last_key_registry_ = nullptr;
  CryptAuthClientFactory* last_client_factory_ = nullptr;
  SyncedBluetoothAddressTracker* last_synced_bluetooth_address_tracker_ =
      nullptr;
  PrefService* last_pref_service_ = nullptr;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_
