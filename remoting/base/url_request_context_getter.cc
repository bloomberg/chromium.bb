// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/url_request_context_getter.h"

#include "base/single_thread_task_runner.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "remoting/base/vlog_net_log.h"

namespace remoting {

URLRequestContextGetter::URLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : network_task_runner_(network_task_runner) {
  proxy_config_service_.reset(net::ProxyService::CreateSystemProxyConfigService(
      network_task_runner_, file_task_runner));
}

net::URLRequestContext* URLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_.get()) {
    net::URLRequestContextBuilder builder;
    builder.set_net_log(new VlogNetLog());
    builder.DisableHttpCache();
    builder.set_proxy_config_service(proxy_config_service_.release());
    url_request_context_.reset(builder.Build());
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
URLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

URLRequestContextGetter::~URLRequestContextGetter() {
}

}  // namespace remoting
