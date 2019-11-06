// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_url_request_context_getter.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/cookies/cookie_store.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

WebEngineURLRequestContextGetter::WebEngineURLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    net::NetLog* net_log,
    content::ProtocolHandlerMap protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors,
    base::FilePath data_dir_path)
    : network_task_runner_(std::move(network_task_runner)),
      net_log_(net_log),
      protocol_handlers_(std::move(protocol_handlers)),
      request_interceptors_(std::move(request_interceptors)),
      data_dir_path_(data_dir_path) {}

WebEngineURLRequestContextGetter::~WebEngineURLRequestContextGetter() = default;

net::URLRequestContext*
WebEngineURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_) {
    net::URLRequestContextBuilder builder;
    builder.set_net_log(net_log_);
    builder.set_data_enabled(true);

    for (auto& protocol_handler : protocol_handlers_) {
      builder.SetProtocolHandler(protocol_handler.first,
                                 std::move(protocol_handler.second));
    }
    protocol_handlers_.clear();

    builder.SetInterceptors(std::move(request_interceptors_));

    if (data_dir_path_.empty()) {
      // Set up an in-memory (ephemeral) CookieStore.
      builder.SetCookieStore(
          content::CreateCookieStore(content::CookieStoreConfig(), nullptr));
    } else {
      // Set up a persistent CookieStore under |data_dir_path|.
      content::CookieStoreConfig cookie_config(
          data_dir_path_.Append(FILE_PATH_LITERAL("Cookies")), false, false,
          NULL);

      // Fuchsia protects the local data at rest so there is no need to encrypt
      // cookie store.
      builder.SetCookieStore(
          content::CreateCookieStore(cookie_config, nullptr));
    }

    url_request_context_ = builder.Build();
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebEngineURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}
