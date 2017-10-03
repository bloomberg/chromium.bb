// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_PROXY_RESOLVER_FACTORY_H_
#define SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_PROXY_RESOLVER_FACTORY_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/interfaces/host_resolver_service.mojom.h"
#include "services/proxy_resolver/public/interfaces/proxy_resolver.mojom.h"

namespace proxy_resolver {

// Factory for connecting to Mojo ProxyResolver services.
class MojoProxyResolverFactory {
 public:
  // Connect to a new ProxyResolver service using request |req|, using
  // |host_resolver| as the DNS resolver. The return value should be released
  // when the connection to |req| is no longer needed.
  // Note: The connection request |req| may be resolved asynchronously.
  virtual std::unique_ptr<base::ScopedClosureRunner> CreateResolver(
      const std::string& pac_script,
      mojo::InterfaceRequest<mojom::ProxyResolver> req,
      mojom::ProxyResolverFactoryRequestClientPtr client) = 0;

 protected:
  virtual ~MojoProxyResolverFactory() = default;
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_PROXY_RESOLVER_FACTORY_H_
