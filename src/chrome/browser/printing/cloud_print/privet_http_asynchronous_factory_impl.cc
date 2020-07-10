// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/local_discovery/endpoint_resolver.h"
#include "chrome/browser/printing/cloud_print/privet_http_impl.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace cloud_print {

PrivetHTTPAsynchronousFactoryImpl::PrivetHTTPAsynchronousFactoryImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

PrivetHTTPAsynchronousFactoryImpl::~PrivetHTTPAsynchronousFactoryImpl() {}

std::unique_ptr<PrivetHTTPResolution>
PrivetHTTPAsynchronousFactoryImpl::CreatePrivetHTTP(
    const std::string& service_name) {
  return std::make_unique<ResolutionImpl>(service_name, url_loader_factory_);
}

PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::ResolutionImpl(
    const std::string& service_name,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : name_(service_name),
      url_loader_factory_(url_loader_factory),
      endpoint_resolver_(
          std::make_unique<local_discovery::EndpointResolver>()) {}

PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::~ResolutionImpl() {}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::Start(
    const net::HostPortPair& address,
    ResultCallback callback) {
  endpoint_resolver_->Start(
      address, base::BindOnce(&ResolutionImpl::ResolveComplete,
                              base::Unretained(this), std::move(callback)));
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::ResolveComplete(
    ResultCallback callback,
    const net::IPEndPoint& endpoint) {
  if (endpoint.address().empty())
    return std::move(callback).Run(nullptr);

  net::HostPortPair new_address = net::HostPortPair::FromIPEndPoint(endpoint);
  std::move(callback).Run(std::make_unique<PrivetHTTPClientImpl>(
      name_, new_address, url_loader_factory_));
}

}  // namespace cloud_print
