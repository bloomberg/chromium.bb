// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_FACTORY_IMPL_H_
#define SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_FACTORY_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "services/proxy_resolver/public/interfaces/proxy_resolver.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace net {
class ProxyResolverV8TracingFactory;
}  // namespace net

namespace proxy_resolver {

class ProxyResolverFactoryImpl : public mojom::ProxyResolverFactory {
 public:
  ProxyResolverFactoryImpl();
  explicit ProxyResolverFactoryImpl(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);

  ~ProxyResolverFactoryImpl() override;

 protected:
  // Visible for tests.
  ProxyResolverFactoryImpl(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref,
      std::unique_ptr<net::ProxyResolverV8TracingFactory>
          proxy_resolver_factory);

 private:
  class Job;

  // mojom::ProxyResolverFactory override.
  void CreateResolver(
      const std::string& pac_script,
      mojom::ProxyResolverRequest request,
      mojom::ProxyResolverFactoryRequestClientPtr client) override;

  void RemoveJob(Job* job);

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  const std::unique_ptr<net::ProxyResolverV8TracingFactory>
      proxy_resolver_impl_factory_;

  std::map<Job*, std::unique_ptr<Job>> jobs_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryImpl);
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_FACTORY_IMPL_H_
