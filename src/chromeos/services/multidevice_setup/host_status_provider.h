// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// Provides the status of the current MultiDevice host, if it exists.
// Additionally, provides an observer interface for being notified when the host
// status changes.
class HostStatusProvider {
 public:
  class HostStatusWithDevice {
   public:
    HostStatusWithDevice(
        mojom::HostStatus host_status,
        const base::Optional<multidevice::RemoteDeviceRef>& host_device);
    HostStatusWithDevice(const HostStatusWithDevice& other);
    ~HostStatusWithDevice();

    bool operator==(const HostStatusWithDevice& other) const;
    bool operator!=(const HostStatusWithDevice& other) const;

    mojom::HostStatus host_status() const { return host_status_; }

    // If host_status() is kNoEligibleHosts or
    // kEligibleHostExistsButNoHostSet, host_device() is null.
    const base::Optional<multidevice::RemoteDeviceRef>& host_device() const {
      return host_device_;
    }

   private:
    mojom::HostStatus host_status_;
    base::Optional<multidevice::RemoteDeviceRef> host_device_;
  };

  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnHostStatusChange(
        const HostStatusWithDevice& host_status_with_device) = 0;
  };

  virtual ~HostStatusProvider();

  virtual HostStatusWithDevice GetHostWithStatus() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  HostStatusProvider();

  void NotifyHostStatusChange(
      mojom::HostStatus host_status,
      const base::Optional<multidevice::RemoteDeviceRef>& host_device);

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(HostStatusProvider);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_H_
