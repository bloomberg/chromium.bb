// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_TETHER_DISCONNECTOR_H_
#define ASH_COMPONENTS_TETHER_TETHER_DISCONNECTOR_H_

#include <string>

#include "ash/components/tether/disconnect_tethering_operation.h"
#include "ash/components/tether/tether_session_completion_logger.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_connection_handler.h"

namespace chromeos {

namespace tether {

// Disconnects from an active Tether connection.
class TetherDisconnector {
 public:
  using StringErrorCallback =
      NetworkConnectionHandler::TetherDelegate::StringErrorCallback;

  TetherDisconnector() {}

  TetherDisconnector(const TetherDisconnector&) = delete;
  TetherDisconnector& operator=(const TetherDisconnector&) = delete;

  virtual ~TetherDisconnector() {}

  // Disconnects from the network with GUID |tether_network_guid|. This GUID
  // must correspond to an active (i.e., connecting/connected) Tether network.
  // If disconnection fails, |error_callback| is invoked with a
  // NetworkConnectionHandler error value.
  virtual void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      base::OnceClosure success_callback,
      StringErrorCallback error_callback,
      const TetherSessionCompletionLogger::SessionCompletionReason&
          session_completion_reason) = 0;
};

}  // namespace tether

}  // namespace chromeos

#endif  // ASH_COMPONENTS_TETHER_TETHER_DISCONNECTOR_H_
