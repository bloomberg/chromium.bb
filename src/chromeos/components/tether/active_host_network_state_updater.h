// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_
#define CHROMEOS_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/active_host.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

// Observes changes to the status of the active host, and relays these updates
// to the networking stack.
class ActiveHostNetworkStateUpdater final : public ActiveHost::Observer {
 public:
  ActiveHostNetworkStateUpdater(ActiveHost* active_host,
                                NetworkStateHandler* network_state_handler);
  ~ActiveHostNetworkStateUpdater();

  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  ActiveHost* active_host_;
  NetworkStateHandler* network_state_handler_;

  DISALLOW_COPY_AND_ASSIGN(ActiveHostNetworkStateUpdater);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_
