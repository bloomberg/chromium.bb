// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONNECTION_OBSERVER_H_
#define CHROMEOS_NETWORK_NETWORK_CONNECTION_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// Observer class for network connection events.
class CHROMEOS_EXPORT NetworkConnectionObserver {
 public:
  NetworkConnectionObserver();

  // Called when a connection to network |service_path| is requested by
  // calling NetworkConnectionHandler::ConnectToNetwork.
  virtual void ConnectToNetworkRequested(const std::string& service_path);

  // Called when a connection request succeeds.
  virtual void ConnectSucceeded(const std::string& service_path);

  // Called when a connection request fails. Valid error names are defined in
  // NetworkConnectionHandler.
  virtual void ConnectFailed(const std::string& service_path,
                             const std::string& error_name);

  // Called when a disconnect to network |service_path| is requested by
  // calling NetworkConnectionHandler::DisconnectNetwork. Success or failure
  // for disconnect is not tracked here, observe NetworkStateHandler for state
  // changes instead.
  virtual void DisconnectRequested(const std::string& service_path);

 protected:
  virtual ~NetworkConnectionObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONNECTION_OBSERVER_H_
