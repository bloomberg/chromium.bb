// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/top_level_proxy_window_impl.h"

#include "services/ws/client_root.h"
#include "services/ws/public/mojom/window_tree.mojom.h"

namespace ws {

TopLevelProxyWindowImpl::TopLevelProxyWindowImpl(
    mojom::WindowTreeClient* window_tree_client,
    Id window_transport_id)
    : window_tree_client_(window_tree_client),
      window_transport_id_(window_transport_id) {}

TopLevelProxyWindowImpl::~TopLevelProxyWindowImpl() {}

void TopLevelProxyWindowImpl::OnWindowResizeLoopStarted() {
  window_tree_client_->OnWindowResizeLoopStarted(window_transport_id_);
}

void TopLevelProxyWindowImpl::OnWindowResizeLoopEnded() {
  window_tree_client_->OnWindowResizeLoopEnded(window_transport_id_);
}

void TopLevelProxyWindowImpl::RequestClose() {
  window_tree_client_->RequestClose(window_transport_id_);
}

std::unique_ptr<ScopedForceVisible> TopLevelProxyWindowImpl::ForceVisible() {
  return client_root_->ForceWindowVisible();
}

ScopedForceVisible::~ScopedForceVisible() {
  if (client_root_)
    client_root_->OnForceVisibleDestroyed();
}

ScopedForceVisible::ScopedForceVisible(ClientRoot* client_root)
    : client_root_(client_root) {}

void ScopedForceVisible::OnClientRootDestroyed() {
  client_root_ = nullptr;
}

}  // namespace ws
