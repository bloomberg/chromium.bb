// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_manager.h"

#include <memory>
#include <utility>

#include "base/optional.h"
#include "net/http/http_util.h"
#include "services/network/origin_policy/origin_policy_constants.h"

namespace network {

OriginPolicyManager::OriginPolicyManager(
    mojom::URLLoaderFactoryPtr url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

OriginPolicyManager::~OriginPolicyManager() {}

void OriginPolicyManager::AddBinding(
    mojom::OriginPolicyManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OriginPolicyManager::RetrieveOriginPolicy(
    const url::Origin& origin,
    const std::string& header_value,
    RetrieveOriginPolicyCallback callback) {
  OriginPolicyHeaderValues header_info =
      GetRequestedPolicyAndReportGroupFromHeaderString(header_value);
  if (header_info.policy_version.empty()) {
    if (callback) {
      auto result = mojom::OriginPolicy::New();
      result->state = mojom::OriginPolicyState::kCannotLoadPolicy;
      std::move(callback).Run(std::move(result));
    }
    return;
  }

  // Here we might check the cache and only then start the fetch.
  StartPolicyFetch(origin, header_info, std::move(callback));
}

void OriginPolicyManager::RetrieveDefaultOriginPolicy(
    const url::Origin& origin,
    RetrieveOriginPolicyCallback callback) {
  // Here we might check the cache and only then start the fetch.
  StartPolicyFetch(origin, OriginPolicyHeaderValues(), std::move(callback));
}

void OriginPolicyManager::StartPolicyFetch(
    const url::Origin& origin,
    const OriginPolicyHeaderValues& header_info,
    RetrieveOriginPolicyCallback callback) {
  if (header_info.policy_version.empty()) {
    origin_policy_fetchers_.emplace(std::make_unique<OriginPolicyFetcher>(
        this, header_info.report_to, origin, url_loader_factory_.get(),
        std::move(callback)));
  } else {
    origin_policy_fetchers_.emplace(std::make_unique<OriginPolicyFetcher>(
        this, header_info.policy_version, header_info.report_to, origin,
        url_loader_factory_.get(), std::move(callback)));
  }
}

void OriginPolicyManager::FetcherDone(OriginPolicyFetcher* fetcher) {
  auto it = origin_policy_fetchers_.find(fetcher);
  DCHECK(it != origin_policy_fetchers_.end());
  origin_policy_fetchers_.erase(it);
}

// static
OriginPolicyManager::OriginPolicyHeaderValues
OriginPolicyManager::GetRequestedPolicyAndReportGroupFromHeaderString(
    const std::string& header_value) {
  if (net::HttpUtil::TrimLWS(header_value) == kOriginPolicyDeletePolicy)
    return OriginPolicyHeaderValues({kOriginPolicyDeletePolicy, ""});

  base::Optional<std::string> policy;
  base::Optional<std::string> report_to;
  bool valid = true;
  net::HttpUtil::NameValuePairsIterator iter(header_value.cbegin(),
                                             header_value.cend(), ',');
  while (iter.GetNext()) {
    std::string token_value = net::HttpUtil::TrimLWS(iter.value()).as_string();
    bool is_token = net::HttpUtil::IsToken(token_value);
    if (iter.name() == kOriginPolicyPolicy) {
      valid &= is_token && !policy.has_value();
      policy = token_value;
    } else if (iter.name() == kOriginPolicyReportTo) {
      valid &= is_token && !report_to.has_value();
      report_to = token_value;
    }
  }
  valid &= iter.valid();
  valid &= (policy.has_value() && policy->find('.') == std::string::npos);

  if (!valid)
    return OriginPolicyHeaderValues();
  return OriginPolicyHeaderValues({policy.value(), report_to.value_or("")});
}

}  // namespace network
