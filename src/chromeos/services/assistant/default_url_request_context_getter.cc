// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file comes from Google Home(cast) implementation.

#include "chromeos/services/assistant/default_url_request_context_getter.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace {

std::unique_ptr<::net::ProxyConfigServiceFixed> GetProxyConfigurationFromParams(
    const std::string& proxy_server,
    const std::string& bypass_list) {
  ::net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(proxy_server);
  proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
  ::net::ProxyConfigWithAnnotation annotated_config(proxy_config,
                                                    NO_TRAFFIC_ANNOTATION_YET);

  return std::make_unique<::net::ProxyConfigServiceFixed>(
      ::net::ProxyConfigServiceFixed(annotated_config));
}
}  // namespace

namespace chromeos {
namespace assistant {

DefaultURLRequestContextGetter::DefaultURLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : network_task_runner_(std::move(network_task_runner)) {
  DCHECK(network_task_runner_);
}

DefaultURLRequestContextGetter::~DefaultURLRequestContextGetter() = default;

void DefaultURLRequestContextGetter::CreateContext() {
  // Context must be created on network thread since its internal objects
  // create and enforce thread checkers.
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  ::net::URLRequestContextBuilder builder;
  // Set direct proxy configuration.
  builder.set_proxy_config_service(GetProxyConfigurationFromParams("", ""));
  builder.DisableHttpCache();
  request_context_ = builder.Build();
  CHECK(request_context_);
}

::net::URLRequestContext*
DefaultURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (!request_context_)
    CreateContext();

  return request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
DefaultURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

void DefaultURLRequestContextGetter::SetProxyConfiguration(
    const std::string& proxy_server,
    const std::string& bypass_list) {
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DefaultURLRequestContextGetter::SetProxyConfigurationInternal,
                 this, proxy_server, bypass_list));
}

void DefaultURLRequestContextGetter::SetProxyConfigurationInternal(
    const std::string& proxy_server,
    const std::string& bypass_list) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  GetURLRequestContext()->proxy_resolution_service()->ResetConfigService(
      GetProxyConfigurationFromParams(proxy_server, bypass_list));
}

}  // namespace assistant
}  // namespace chromeos
