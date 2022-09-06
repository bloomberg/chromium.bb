// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
#define ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_

#include "ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

namespace device_sync {

// Base DeviceSync implementation.
class DeviceSyncBase : public mojom::DeviceSync {
 public:
  DeviceSyncBase(const DeviceSyncBase&) = delete;
  DeviceSyncBase& operator=(const DeviceSyncBase&) = delete;

  ~DeviceSyncBase() override;

  // device_sync::mojom::DeviceSync:
  void AddObserver(mojo::PendingRemote<mojom::DeviceSyncObserver> observer,
                   AddObserverCallback callback) override;

  // Binds a receiver to this implementation. Should be called each time that
  // the service receives a receiver.
  void BindReceiver(mojo::PendingReceiver<mojom::DeviceSync> receiver);

  void CloseAllReceivers();

 protected:
  DeviceSyncBase();

  // Derived types should override this function to remove references to any
  // dependencies.
  virtual void Shutdown() {}

  void NotifyOnEnrollmentFinished();
  void NotifyOnNewDevicesSynced();

 private:
  void OnDisconnection();

  mojo::RemoteSet<mojom::DeviceSyncObserver> observers_;
  mojo::ReceiverSet<mojom::DeviceSync> receivers_;
};

}  // namespace device_sync

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::device_sync {
using ::ash::device_sync::DeviceSyncBase;
}

#endif  // ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
