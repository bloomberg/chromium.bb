// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_NETWORK_LIST_SORTER_H_
#define CHROMEOS_COMPONENTS_TETHER_NETWORK_LIST_SORTER_H_

#include "base/macros.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

// Sorts networks in the order that they should be displayed in network
// configuration UI. The sort order is be determined by network properties
// (e.g., connected devices before unconnected; higher signal strength before
// lower signal strength).
class NetworkListSorter : public NetworkStateHandler::TetherSortDelegate {
 public:
  NetworkListSorter();
  virtual ~NetworkListSorter();

  // NetworkStateHandler::TetherNetworkListSorter:
  void SortTetherNetworkList(
      NetworkStateHandler::ManagedStateList* tether_networks) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkListSorter);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_NETWORK_LIST_SORTER_H_
