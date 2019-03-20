// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_IMPL_H_
#define SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_IMPL_H_

#include <memory>

#include "services/ws/common/types.h"
#include "services/ws/top_level_proxy_window.h"

namespace ws {

class ClientRoot;

namespace mojom {
class WindowTreeClient;
}

// See superclass for details. This mostly just forwards calls to the remote
// client.
class TopLevelProxyWindowImpl : public TopLevelProxyWindow {
 public:
  TopLevelProxyWindowImpl(mojom::WindowTreeClient* window_tree_client,
                          Id window_transport_id);
  ~TopLevelProxyWindowImpl() override;

  void set_client_root(ClientRoot* client_root) { client_root_ = client_root; }

  // TopLevelProxyWindowImpl:
  void OnWindowResizeLoopStarted() override;
  void OnWindowResizeLoopEnded() override;
  void RequestClose() override;
  std::unique_ptr<ScopedForceVisible> ForceVisible() override;

 private:
  mojom::WindowTreeClient* window_tree_client_;
  const Id window_transport_id_;
  ClientRoot* client_root_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TopLevelProxyWindowImpl);
};

}  // namespace ws

#endif  // SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_IMPL_H_
