// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_IMPL_H_
#define SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_IMPL_H_

#include "services/ws/common/types.h"
#include "services/ws/top_level_proxy_window.h"

namespace ws {
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

  // TopLevelProxyWindowImpl:
  void OnWindowResizeLoopStarted() override;
  void OnWindowResizeLoopEnded() override;
  void RequestClose() override;

 private:
  mojom::WindowTreeClient* window_tree_client_;
  const Id window_transport_id_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelProxyWindowImpl);
};

}  // namespace ws

#endif  // SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_IMPL_H_
