// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/network/network_connection_handler.h"

namespace chromeos {

namespace tether {

// Connects to a tether network. When the user initiates a connection via the
// UI, TetherConnector receives a callback from NetworkConnectionHandler and
// initiates a connection by starting a ConnectTetheringOperation. When a
// response has been received from the tether host, TetherConnector connects to
// the associated Wi-Fi network.
class TetherConnector {
 public:
  TetherConnector() {}
  virtual ~TetherConnector() {}

  virtual void ConnectToNetwork(
      const std::string& tether_network_guid,
      base::OnceClosure success_callback,
      const network_handler::StringResultCallback& error_callback) = 0;

  // Returns whether the connection attempt was successfully canceled.
  virtual bool CancelConnectionAttempt(
      const std::string& tether_network_guid) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherConnector);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_H_
