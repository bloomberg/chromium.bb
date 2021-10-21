// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/gurl.h"

namespace blink {

namespace {

using ::network::mojom::WebClientHintsType;

bool IsDisabledByFeature(const WebClientHintsType type) {
  switch (type) {
    case WebClientHintsType::kUA:
    case WebClientHintsType::kUAArch:
    case WebClientHintsType::kUAPlatform:
    case WebClientHintsType::kUAPlatformVersion:
    case WebClientHintsType::kUAModel:
    case WebClientHintsType::kUAMobile:
    case WebClientHintsType::kUAFullVersion:
    case WebClientHintsType::kUABitness:
      if (!base::FeatureList::IsEnabled(features::kUserAgentClientHint))
        return true;
      break;
    case WebClientHintsType::kPrefersColorScheme:
      if (!base::FeatureList::IsEnabled(
              features::kPrefersColorSchemeClientHintHeader))
        return true;
      break;
    case WebClientHintsType::kViewportHeight:
      if (!base::FeatureList::IsEnabled(
              features::kViewportHeightClientHintHeader))
        return true;
      break;
    case WebClientHintsType::kDeviceMemory:
      if (!base::FeatureList::IsEnabled(features::kClientHintsDeviceMemory))
        return true;
      break;
    case WebClientHintsType::kDpr:
      if (!base::FeatureList::IsEnabled(features::kClientHintsDPR))
        return true;
      break;
    case WebClientHintsType::kResourceWidth:
      if (!base::FeatureList::IsEnabled(features::kClientHintsResourceWidth))
        return true;
      break;
    case WebClientHintsType::kViewportWidth:
      if (!base::FeatureList::IsEnabled(features::kClientHintsViewportWidth))
        return true;
      break;
    case WebClientHintsType::kDeviceMemory_DEPRECATED:
      if (!base::FeatureList::IsEnabled(
              features::kClientHintsDeviceMemory_DEPRECATED))
        return true;
      break;
    case WebClientHintsType::kDpr_DEPRECATED:
      if (!base::FeatureList::IsEnabled(features::kClientHintsDPR_DEPRECATED))
        return true;
      break;
    case WebClientHintsType::kResourceWidth_DEPRECATED:
      if (!base::FeatureList::IsEnabled(
              features::kClientHintsResourceWidth_DEPRECATED))
        return true;
      break;
    case WebClientHintsType::kViewportWidth_DEPRECATED:
      if (!base::FeatureList::IsEnabled(
              features::kClientHintsViewportWidth_DEPRECATED))
        return true;
      break;
    default:
      break;
  }
  return false;
}

bool IsUaReducedClientHintEnabled(
    const GURL& url,
    const GURL* third_party_url,
    const net::HttpResponseHeaders* response_headers) {
  blink::TrialTokenValidator validator;
  base::Time now = base::Time::Now();
  if (third_party_url == nullptr) {
    // It's not a third-party embed request, validate the UserAgentReduction OT
    // token as normal.
    return validator.RequestEnablesFeature(url, response_headers,
                                           "UserAgentReduction", now);
  }

  // Validate the third-party OT token.
  bool enabled = false;
  // Iterate through all of the Origin-Trial headers and validate if any of
  // them are valid third-party OT tokens for the UserAgentReduction trial.
  if (validator.IsTrialPossibleOnOrigin(*third_party_url)) {
    url::Origin origin = url::Origin::Create(url);
    url::Origin third_party_origin = url::Origin::Create(*third_party_url);
    size_t iter = 0;
    std::string token;
    while (response_headers->EnumerateHeader(&iter, "Origin-Trial", &token)) {
      blink::TrialTokenResult result =
          validator.ValidateToken(token, origin, &third_party_origin, now);
      if (result.Status() == blink::OriginTrialTokenStatus::kSuccess) {
        if (result.ParsedToken()->feature_name() == "UserAgentReduction") {
          enabled = true;
          break;
        }
      }
    }
  }
  return enabled;
}

}  // namespace

bool EnabledClientHints::IsEnabled(const WebClientHintsType type) const {
  return enabled_types_[static_cast<int>(type)];
}

void EnabledClientHints::SetIsEnabled(const WebClientHintsType type,
                                      const bool should_send) {
  enabled_types_[static_cast<int>(type)] =
      IsDisabledByFeature(type) ? false : should_send;
}

void EnabledClientHints::SetIsEnabled(
    const GURL& url,
    const GURL* third_party_url,
    const net::HttpResponseHeaders* response_headers,
    const network::mojom::WebClientHintsType type,
    const bool should_send) {
  bool enabled = should_send;
  if (enabled && type == WebClientHintsType::kUAReduced) {
    enabled =
        IsUaReducedClientHintEnabled(url, third_party_url, response_headers);
  }
  SetIsEnabled(type, enabled);
}

std::vector<WebClientHintsType> EnabledClientHints::GetEnabledHints() const {
  std::vector<WebClientHintsType> hints;
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& type = elem.first;
    if (IsEnabled(type))
      hints.push_back(type);
  }
  return hints;
}

}  // namespace blink
