// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_factory_impl.h"

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_errors.h"
#include "net/proxy/mojo_proxy_resolver_v8_tracing_bindings.h"
#include "net/proxy/proxy_resolver_factory.h"
#include "net/proxy/proxy_resolver_v8_tracing.h"
#include "services/proxy_resolver/proxy_resolver_impl.h"

namespace proxy_resolver {

class ProxyResolverFactoryImpl::Job {
 public:
  Job(ProxyResolverFactoryImpl* parent,
      const scoped_refptr<net::ProxyResolverScriptData>& pac_script,
      net::ProxyResolverV8TracingFactory* proxy_resolver_factory,
      mojo::InterfaceRequest<mojom::ProxyResolver> request,
      mojom::ProxyResolverFactoryRequestClientPtr client);
  ~Job();

 private:
  // Mojo error handler.
  void OnConnectionError();

  void OnProxyResolverCreated(int error);

  ProxyResolverFactoryImpl* const parent_;
  std::unique_ptr<net::ProxyResolverV8Tracing> proxy_resolver_impl_;
  mojo::InterfaceRequest<mojom::ProxyResolver> proxy_request_;
  net::ProxyResolverV8TracingFactory* factory_;
  std::unique_ptr<net::ProxyResolverFactory::Request> request_;
  mojom::ProxyResolverFactoryRequestClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

ProxyResolverFactoryImpl::Job::Job(
    ProxyResolverFactoryImpl* factory,
    const scoped_refptr<net::ProxyResolverScriptData>& pac_script,
    net::ProxyResolverV8TracingFactory* proxy_resolver_factory,
    mojo::InterfaceRequest<mojom::ProxyResolver> request,
    mojom::ProxyResolverFactoryRequestClientPtr client)
    : parent_(factory),
      proxy_request_(std::move(request)),
      factory_(proxy_resolver_factory),
      client_ptr_(std::move(client)) {
  client_ptr_.set_connection_error_handler(
      base::Bind(&ProxyResolverFactoryImpl::Job::OnConnectionError,
                 base::Unretained(this)));
  factory_->CreateProxyResolverV8Tracing(
      pac_script,
      std::make_unique<net::MojoProxyResolverV8TracingBindings<
          mojom::ProxyResolverFactoryRequestClient>>(client_ptr_.get()),
      &proxy_resolver_impl_,
      base::Bind(&ProxyResolverFactoryImpl::Job::OnProxyResolverCreated,
                 base::Unretained(this)),
      &request_);
}

ProxyResolverFactoryImpl::Job::~Job() = default;

void ProxyResolverFactoryImpl::Job::OnConnectionError() {
  client_ptr_->ReportResult(net::ERR_PAC_SCRIPT_TERMINATED);
  parent_->RemoveJob(this);
}

void ProxyResolverFactoryImpl::Job::OnProxyResolverCreated(int error) {
  if (error == net::OK) {
    mojo::MakeStrongBinding(
        std::make_unique<ProxyResolverImpl>(std::move(proxy_resolver_impl_)),
        std::move(proxy_request_));
  }
  client_ptr_->ReportResult(error);
  parent_->RemoveJob(this);
}

ProxyResolverFactoryImpl::ProxyResolverFactoryImpl()
    : ProxyResolverFactoryImpl(
          std::unique_ptr<service_manager::ServiceContextRef>(),
          net::ProxyResolverV8TracingFactory::Create()) {}

ProxyResolverFactoryImpl::ProxyResolverFactoryImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : ProxyResolverFactoryImpl(std::move(service_ref),
                               net::ProxyResolverV8TracingFactory::Create()) {}

ProxyResolverFactoryImpl::ProxyResolverFactoryImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref,
    std::unique_ptr<net::ProxyResolverV8TracingFactory> proxy_resolver_factory)
    : service_ref_(std::move(service_ref)),
      proxy_resolver_impl_factory_(std::move(proxy_resolver_factory)) {}

ProxyResolverFactoryImpl::~ProxyResolverFactoryImpl() {}

void ProxyResolverFactoryImpl::CreateResolver(
    const std::string& pac_script,
    mojo::InterfaceRequest<mojom::ProxyResolver> request,
    mojom::ProxyResolverFactoryRequestClientPtr client) {
  // The Job will call RemoveJob on |this| when either the create request
  // finishes or |request| or |client| encounters a connection error.
  std::unique_ptr<Job> job = std::make_unique<Job>(
      this, net::ProxyResolverScriptData::FromUTF8(pac_script),
      proxy_resolver_impl_factory_.get(), std::move(request),
      std::move(client));
  Job* job_ptr = job.get();
  jobs_[job_ptr] = std::move(job);
}

void ProxyResolverFactoryImpl::RemoveJob(Job* job) {
  size_t erased_count = jobs_.erase(job);
  DCHECK_EQ(1U, erased_count);
}

}  // namespace proxy_resolver
