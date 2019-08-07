// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_H_
#define SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/aura/window_tracker.h"

namespace ws {

class ClientRoot;
class ScopedForceVisible;

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

  // Forces the client to think the window is visible while the return value is
  // valid.
  virtual std::unique_ptr<ScopedForceVisible> ForceVisible() = 0;

 protected:
  virtual ~TopLevelProxyWindow() {}
};

// See ForceVisible for details.
class COMPONENT_EXPORT(WINDOW_SERVICE) ScopedForceVisible {
 public:
  ~ScopedForceVisible();

 private:
  friend class ClientRoot;

  explicit ScopedForceVisible(ClientRoot* client_root);

  void OnClientRootDestroyed();

  ClientRoot* client_root_;

  DISALLOW_COPY_AND_ASSIGN(ScopedForceVisible);
};

}  // namespace ws

#endif  // SERVICES_WS_TOP_LEVEL_PROXY_WINDOW_H_
