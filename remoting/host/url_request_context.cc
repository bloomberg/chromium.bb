// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/url_request_context.h"

#include "base/message_loop_proxy.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "remoting/host/vlog_net_log.h"

namespace remoting {

// TODO(willchan): This is largely copied from service_url_request_context.cc,
// which is in turn copied from some test code. Move it somewhere reusable.
URLRequestContext::URLRequestContext(
    scoped_ptr<net::ProxyConfigService> proxy_config_service)
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  scoped_ptr<VlogNetLog> net_log(new VlogNetLog());
  storage_.set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    net::HostResolver::kDefaultRetryAttempts,
                                    net_log.get()));
  storage_.set_proxy_service(net::ProxyService::CreateUsingSystemProxyResolver(
      proxy_config_service.release(), 0u, net_log.get()));
  storage_.set_cert_verifier(net::CertVerifier::CreateDefault());
  storage_.set_ssl_config_service(new net::SSLConfigServiceDefaults);
  storage_.set_http_auth_handler_factory(
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  storage_.set_http_server_properties(new net::HttpServerPropertiesImpl);

  net::HttpNetworkSession::Params session_params;
  session_params.host_resolver = host_resolver();
  session_params.cert_verifier = cert_verifier();
  session_params.proxy_service = proxy_service();
  session_params.ssl_config_service = ssl_config_service();
  session_params.http_auth_handler_factory = http_auth_handler_factory();
  session_params.http_server_properties = http_server_properties();
  session_params.net_log = net_log.get();
  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(session_params));
  storage_.set_http_transaction_factory(
      new net::HttpNetworkLayer(network_session));
  storage_.set_net_log(net_log.release());
}

URLRequestContext::~URLRequestContext() {
}

URLRequestContextGetter::URLRequestContextGetter(
    MessageLoop* io_message_loop,
    MessageLoop* file_message_loop)
    : io_message_loop_proxy_(io_message_loop->message_loop_proxy()) {
  proxy_config_service_.reset(
      net::ProxyService::CreateSystemProxyConfigService(
          io_message_loop, file_message_loop));
}

net::URLRequestContext* URLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_) {
    url_request_context_ =
        new URLRequestContext(proxy_config_service_.Pass());
  }
  return url_request_context_;
}

scoped_refptr<base::MessageLoopProxy>
URLRequestContextGetter::GetIOMessageLoopProxy() const {
  return io_message_loop_proxy_;
}

URLRequestContextGetter::~URLRequestContextGetter() {}

}  // namespace remoting
