// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_
#define ASH_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_

#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_connection_handler.h"

namespace ash {

namespace tether {

class ActiveHost;
class TetherConnector;
class TetherDisconnector;

// Handles connect/disconnect requests for Tether networks.
class NetworkConnectionHandlerTetherDelegate
    : public NetworkConnectionHandler::TetherDelegate {
 public:
  NetworkConnectionHandlerTetherDelegate(
      NetworkConnectionHandler* network_connection_handler,
      ActiveHost* active_host,
      TetherConnector* tether_connector,
      TetherDisconnector* tether_disconnector);

  NetworkConnectionHandlerTetherDelegate(
      const NetworkConnectionHandlerTetherDelegate&) = delete;
  NetworkConnectionHandlerTetherDelegate& operator=(
      const NetworkConnectionHandlerTetherDelegate&) = delete;

  ~NetworkConnectionHandlerTetherDelegate() override;

  // NetworkConnectionHandler::TetherDelegate:
  void DisconnectFromNetwork(const std::string& tether_network_guid,
                             base::OnceClosure success_callback,
                             StringErrorCallback error_callback) override;
  void ConnectToNetwork(const std::string& tether_network_guid,
                        base::OnceClosure success_callback,
                        StringErrorCallback error_callback) override;

 private:
  struct Callbacks {
   public:
    Callbacks(base::OnceClosure success_callback,
              StringErrorCallback error_callback);
    Callbacks(Callbacks&&);
    ~Callbacks();

    base::OnceClosure success_callback;
    StringErrorCallback error_callback;
  };

  void OnRequestSuccess(int request_num);
  void OnRequestError(int request_num, const std::string& error_name);

  NetworkConnectionHandler* network_connection_handler_;
  ActiveHost* active_host_;
  TetherConnector* tether_connector_;
  TetherDisconnector* tether_disconnector_;

  // Cache request callbacks in a map so that if the callbacks do not occur by
  // the time the object is deleted, all callbacks are invoked.
  int next_request_num_ = 0;
  std::unordered_map<int, Callbacks> request_num_to_callbacks_map_;

  base::WeakPtrFactory<NetworkConnectionHandlerTetherDelegate>
      weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // ASH_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_
