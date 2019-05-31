// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PROXY_RESOLVER_SERVICE_H_
#define SERVICES_PROXY_RESOLVER_PROXY_RESOLVER_SERVICE_H_

#include <memory>
#include <string>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace proxy_resolver {

class ProxyResolverService : public service_manager::Service {
 public:
  explicit ProxyResolverService(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);
  ~ProxyResolverService() override;

  // Lifescycle events that occur after the service has started to spinup.
  void OnConnect(const service_manager::ConnectSourceInfo& source,
                 const std::string& interface_name,
                 mojo::ScopedMessagePipeHandle receiver_pipe) override;

 private:
  service_manager::ServiceBinding service_binding_;
  service_manager::ServiceKeepalive service_keepalive_;
  ProxyResolverFactoryImpl proxy_resolver_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverService);
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_PROXY_RESOLVER_SERVICE_H_
