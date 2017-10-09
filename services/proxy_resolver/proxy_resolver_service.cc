// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_service.h"

#include <utility>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"

namespace proxy_resolver {

namespace {

void OnProxyResolverFactoryRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    proxy_resolver::mojom::ProxyResolverFactoryRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<ProxyResolverFactoryImpl>(ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

ProxyResolverService::ProxyResolverService() = default;

ProxyResolverService::~ProxyResolverService() = default;

std::unique_ptr<service_manager::Service>
ProxyResolverService::CreateService() {
  return base::MakeUnique<ProxyResolverService>();
}

void ProxyResolverService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context())));
  registry_.AddInterface(
      base::Bind(&OnProxyResolverFactoryRequest, ref_factory_.get()));
}

void ProxyResolverService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace proxy_resolver