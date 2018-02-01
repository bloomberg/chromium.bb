// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_request_context_owner.h"

#include "base/message_loop/message_loop.h"
#include "components/prefs/pref_service.h"
#include "net/url_request/url_request_context.h"

namespace network {

NetworkURLRequestContextGetter::NetworkURLRequestContextGetter(
    std::unique_ptr<net::URLRequestContext> url_request_context)
    : url_request_context_(std::move(url_request_context)),
      network_task_runner_(base::MessageLoop::current()->task_runner()) {}

// net::URLRequestContextGetter implementation:
net::URLRequestContext* NetworkURLRequestContextGetter::GetURLRequestContext() {
  return shutdown_ ? nullptr : url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
NetworkURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

void NetworkURLRequestContextGetter::NotifyContextShuttingDown() {
  shutdown_ = true;
  URLRequestContextGetter::NotifyContextShuttingDown();
  url_request_context_.reset();
}

NetworkURLRequestContextGetter::~NetworkURLRequestContextGetter() {}

URLRequestContextOwner::URLRequestContextOwner() = default;

URLRequestContextOwner::URLRequestContextOwner(
    std::unique_ptr<PrefService> pref_service_in,
    std::unique_ptr<net::URLRequestContext> url_request_context_in) {
  pref_service = std::move(pref_service_in);
  url_request_context_getter =
      new NetworkURLRequestContextGetter(std::move(url_request_context_in));
}

URLRequestContextOwner::~URLRequestContextOwner() {
  if (url_request_context_getter)
    url_request_context_getter.get()->NotifyContextShuttingDown();
}

URLRequestContextOwner::URLRequestContextOwner(URLRequestContextOwner&& other)
    : pref_service(std::move(other.pref_service)),
      url_request_context_getter(std::move(other.url_request_context_getter)) {}

URLRequestContextOwner& URLRequestContextOwner::operator=(
    URLRequestContextOwner&& other) {
  pref_service = std::move(other.pref_service);
  url_request_context_getter = std::move(other.url_request_context_getter);
  return *this;
}

}  // namespace network
