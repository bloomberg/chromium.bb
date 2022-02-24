// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/url_loader_context_for_tests.h"

namespace network {

URLLoaderContextForTests::URLLoaderContextForTests() = default;
URLLoaderContextForTests::~URLLoaderContextForTests() = default;

bool URLLoaderContextForTests::ShouldRequireNetworkIsolationKey() const {
  return false;
}

const cors::OriginAccessList& URLLoaderContextForTests::GetOriginAccessList()
    const {
  return origin_access_list_;
}

const mojom::URLLoaderFactoryParams&
URLLoaderContextForTests::GetFactoryParams() const {
  return factory_params_;
}

mojom::CookieAccessObserver* URLLoaderContextForTests::GetCookieAccessObserver()
    const {
  return nullptr;
}

mojom::CrossOriginEmbedderPolicyReporter*
URLLoaderContextForTests::GetCoepReporter() const {
  return nullptr;
}

mojom::DevToolsObserver* URLLoaderContextForTests::GetDevToolsObserver() const {
  return nullptr;
}

mojom::NetworkContextClient* URLLoaderContextForTests::GetNetworkContextClient()
    const {
  return network_context_client_;
}

mojom::OriginPolicyManager* URLLoaderContextForTests::GetOriginPolicyManager()
    const {
  return origin_policy_manager_;
}

mojom::TrustedURLLoaderHeaderClient*
URLLoaderContextForTests::GetUrlLoaderHeaderClient() const {
  return nullptr;
}

mojom::URLLoaderNetworkServiceObserver*
URLLoaderContextForTests::GetURLLoaderNetworkServiceObserver() const {
  return nullptr;
}

net::URLRequestContext* URLLoaderContextForTests::GetUrlRequestContext() const {
  return url_request_context_;
}

scoped_refptr<ResourceSchedulerClient>
URLLoaderContextForTests::GetResourceSchedulerClient() const {
  return resource_scheduler_client_;
}

uintptr_t URLLoaderContextForTests::GetFactoryId() const {
  return 0;
}

corb::PerFactoryState& URLLoaderContextForTests::GetMutableCorbState() {
  return corb_state_;
}

}  // namespace network
