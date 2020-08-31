// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_CACHE_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/components/multidevice/remote_device_ref.h"

namespace chromeos {

namespace multidevice {

// A simple cache of RemoteDeviceRefs. Note that if multiple calls to
// SetRemoteDevices() are provided different sets of devices, the set of devices
// returned by GetRemoteDevices() is the union of those different sets (i.e.,
// devices are not deleted from the cache).
//
// All devices in the cache will have a unique Instance ID, if one exists,
// and/or a unique legacy device ID, RemoteDevice::GetDeviceId(), if one exists.
// Every device is guaranteed to have at least one non-trivial ID. If a device
// is added with either ID matching an existing device, the existing device is
// overwritten.
//
// Note: Even though CryptAuth v2 DeviceSync guarantees that all devices have an
// Instance ID, there may still be uses of RemoteDeviceCache in multi-device
// application code that solely uses the legacy device ID.
class RemoteDeviceCache {
 public:
  class Factory {
   public:
    static std::unique_ptr<RemoteDeviceCache> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<RemoteDeviceCache> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  virtual ~RemoteDeviceCache();

  void SetRemoteDevices(const RemoteDeviceList& remote_devices);

  RemoteDeviceRefList GetRemoteDevices() const;

  // Looks up device in cache by Instance ID, |instance_id|, and by the legacy
  // device ID from RemoteDevice::GetDeviceId(), |legacy_device_id|. Returns the
  // first device that matches either ID. Returns null if no such device exists.
  //
  // For best results, pass in both IDs when available since the device could
  // have been written to the cache with one of the IDs missing.
  base::Optional<RemoteDeviceRef> GetRemoteDevice(
      const base::Optional<std::string>& instance_id,
      const base::Optional<std::string>& legacy_device_id) const;

 private:
  RemoteDeviceCache();

  std::shared_ptr<RemoteDevice> GetRemoteDeviceFromCache(
      const base::Optional<std::string>& instance_id,
      const base::Optional<std::string>& legacy_device_id) const;

  std::vector<std::shared_ptr<RemoteDevice>> cached_remote_devices_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceCache);
};

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_CACHE_H_
