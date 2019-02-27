// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_H_
#define SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace ws {

// Interface exposed to clients to interact with top-level windows. This is
// indirectly owned by the aura::Window the TopLevelProxyWindow is associated
// with.
class COMPONENT_EXPORT(WINDOW_SERVICE) TopLevelProxyWindow {
 public:
  // See mojom for details on these. The functions effectively call to the
  // client.
  virtual void OnWindowResizeLoopStarted() = 0;
  virtual void OnWindowResizeLoopEnded() = 0;
  virtual void RequestClose() = 0;

 protected:
  virtual ~TopLevelProxyWindow() {}
};

}  // namespace ws

#endif  // SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_H_
