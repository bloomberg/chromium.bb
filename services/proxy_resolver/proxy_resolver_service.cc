// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_service.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace proxy_resolver {

ProxyResolverService::ProxyResolverService(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)
    : service_binding_(this, std::move(receiver)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

ProxyResolverService::~ProxyResolverService() = default;

void ProxyResolverService::OnConnect(
    const service_manager::ConnectSourceInfo& source,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  if (interface_name == mojom::ProxyResolverFactory::Name_) {
    proxy_resolver_factory_.BindReceiver(
        mojo::PendingReceiver<mojom::ProxyResolverFactory>(
            std::move(receiver_pipe)),
        &service_keepalive_);
  }
}

}  // namespace proxy_resolver
