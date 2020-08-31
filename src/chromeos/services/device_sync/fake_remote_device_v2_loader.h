// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_V2_LOADER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_V2_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/services/device_sync/cryptauth_device_registry.h"
#include "chromeos/services/device_sync/remote_device_v2_loader.h"
#include "chromeos/services/device_sync/remote_device_v2_loader_impl.h"

namespace chromeos {

namespace device_sync {

class FakeRemoteDeviceV2Loader : public RemoteDeviceV2Loader {
 public:
  FakeRemoteDeviceV2Loader();

  // Disallow copy and assign.
  FakeRemoteDeviceV2Loader(const FakeRemoteDeviceV2Loader&) = delete;
  FakeRemoteDeviceV2Loader& operator=(const FakeRemoteDeviceV2Loader&) = delete;

  ~FakeRemoteDeviceV2Loader() override;

  // Returns the Instance ID to device map that was passed into Load(). Returns
  // null if Load() has not been called.
  const base::Optional<CryptAuthDeviceRegistry::InstanceIdToDeviceMap>&
  id_to_device_map() const {
    return id_to_device_map_;
  }

  // Returns the user email that was passed into Load(). Returns null if Load()
  // has not been called.
  const base::Optional<std::string>& user_email() const { return user_email_; }

  // Returns the user private key that was passed into Load(). Returns null if
  // Load() has not been called.
  const base::Optional<std::string>& user_private_key() const {
    return user_private_key_;
  }

  LoadCallback& callback() { return callback_; }

 private:
  // RemoteDeviceV2Loader:
  void Load(
      const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& id_to_device_map,
      const std::string& user_email,
      const std::string& user_private_key,
      LoadCallback callback) override;

  base::Optional<CryptAuthDeviceRegistry::InstanceIdToDeviceMap>
      id_to_device_map_;
  base::Optional<std::string> user_email_;
  base::Optional<std::string> user_private_key_;
  LoadCallback callback_;
};

class FakeRemoteDeviceV2LoaderFactory
    : public RemoteDeviceV2LoaderImpl::Factory {
 public:
  FakeRemoteDeviceV2LoaderFactory();

  // Disallow copy and assign.
  FakeRemoteDeviceV2LoaderFactory(const FakeRemoteDeviceV2LoaderFactory&) =
      delete;
  FakeRemoteDeviceV2LoaderFactory& operator=(const FakeRemoteDeviceV2Loader&) =
      delete;

  ~FakeRemoteDeviceV2LoaderFactory() override;

  // Returns a vector of all FakeRemoteDeviceV2Loader instances created by
  // CreateInstance().
  const std::vector<FakeRemoteDeviceV2Loader*>& instances() const {
    return instances_;
  }

 private:
  // RemoteDeviceV2LoaderImpl::Factory:
  std::unique_ptr<RemoteDeviceV2Loader> CreateInstance() override;

  std::vector<FakeRemoteDeviceV2Loader*> instances_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_V2_LOADER_H_
