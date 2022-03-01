// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_RESOURCE_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_RESOURCE_MANAGER_ASH_H_

#include <stdint.h>

#include "chromeos/crosapi/mojom/resource_manager.mojom.h"
#include "chromeos/dbus/resourced/resourced_client.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the ResourceManager crosapi interface.
// This class must only be used from the main thread.
class ResourceManagerAsh : public mojom::ResourceManager,
                           public chromeos::ResourcedClient::Observer {
 public:
  ResourceManagerAsh();
  ResourceManagerAsh(const ResourceManagerAsh&) = delete;
  ResourceManagerAsh& operator=(const ResourceManagerAsh&) = delete;
  ~ResourceManagerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ResourceManager> receiver);

  // chromeos::ResourcedClient::Observer:
  void OnMemoryPressure(chromeos::ResourcedClient::PressureLevel level,
                        uint64_t reclaim_target_kb) override;

  // crosapi::mojom::ResourceManager:
  void AddMemoryPressureObserver(
      mojo::PendingRemote<mojom::MemoryPressureObserver> observer) override;

 private:
  // Support any number of connections.
  mojo::ReceiverSet<mojom::ResourceManager> receivers_;

  // Support any number of memory pressure observers.
  mojo::RemoteSet<mojom::MemoryPressureObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_RESOURCE_MANAGER_ASH_H_
