// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_socket_factory_mojo.h"

#include <utility>

#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "services/network/proxy_resolving_client_socket.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "services/network/proxy_resolving_socket_mojo.h"
#include "url/gurl.h"

namespace network {

ProxyResolvingSocketFactoryMojo::ProxyResolvingSocketFactoryMojo(
    net::URLRequestContext* request_context)
    : factory_impl_(std::make_unique<ProxyResolvingClientSocketFactory>(
          request_context->GetNetworkSessionContext()->client_socket_factory,
          request_context)),
      ssl_config_service_(request_context->ssl_config_service()) {}

ProxyResolvingSocketFactoryMojo::~ProxyResolvingSocketFactoryMojo() {}

void ProxyResolvingSocketFactoryMojo::CreateProxyResolvingSocket(
    const GURL& url,
    bool use_tls,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::ProxyResolvingSocketRequest request,
    CreateProxyResolvingSocketCallback callback) {
  net::SSLConfig ssl_config;
  ssl_config_service_->GetSSLConfig(&ssl_config);
  auto socket = std::make_unique<ProxyResolvingSocketMojo>(
      factory_impl_->CreateSocket(ssl_config, url, use_tls),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation));
  ProxyResolvingSocketMojo* socket_raw = socket.get();
  proxy_resolving_socket_bindings_.AddBinding(std::move(socket),
                                              std::move(request));
  socket_raw->Connect(std::move(callback));
}

}  // namespace network
