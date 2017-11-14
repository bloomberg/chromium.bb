// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/GlobalScopeCreationParams.h"

#include <memory>
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

GlobalScopeCreationParams::GlobalScopeCreationParams(
    const KURL& script_url,
    const String& user_agent,
    const String& source_code,
    std::unique_ptr<Vector<char>> cached_meta_data,
    const Vector<CSPHeaderAndType>* content_security_policy_parsed_headers,
    ReferrerPolicy referrer_policy,
    const SecurityOrigin* starter_origin,
    WorkerClients* worker_clients,
    WebAddressSpace address_space,
    const Vector<String>* origin_trial_tokens,
    std::unique_ptr<WorkerSettings> worker_settings,
    V8CacheOptions v8_cache_options,
    service_manager::mojom::blink::InterfaceProviderPtrInfo
        interface_provider_info)
    : script_url(script_url.Copy()),
      user_agent(user_agent.IsolatedCopy()),
      source_code(source_code.IsolatedCopy()),
      cached_meta_data(std::move(cached_meta_data)),
      referrer_policy(referrer_policy),
      starter_origin(starter_origin ? starter_origin->IsolatedCopy() : nullptr),
      worker_clients(worker_clients),
      address_space(address_space),
      worker_settings(std::move(worker_settings)),
      v8_cache_options(v8_cache_options),
      interface_provider(std::move(interface_provider_info)) {
  this->content_security_policy_parsed_headers =
      std::make_unique<Vector<CSPHeaderAndType>>();
  if (content_security_policy_parsed_headers) {
    for (const auto& header : *content_security_policy_parsed_headers) {
      CSPHeaderAndType copied_header(header.first.IsolatedCopy(),
                                     header.second);
      this->content_security_policy_parsed_headers->push_back(copied_header);
    }
  }

  this->origin_trial_tokens = std::make_unique<Vector<String>>();
  if (origin_trial_tokens) {
    for (const String& token : *origin_trial_tokens)
      this->origin_trial_tokens->push_back(token.IsolatedCopy());
  }
}

}  // namespace blink
