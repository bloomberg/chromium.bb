// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace blink {

const char* const kClientHintsHeaderMapping[] = {
    "device-memory",
    "dpr",
    "width",
    "viewport-width",
    "rtt",
    "downlink",
    "ect",
    "sec-ch-lang",
    "sec-ch-ua",
    "sec-ch-ua-arch",
    "sec-ch-ua-platform",
    "sec-ch-ua-model",
    "sec-ch-ua-mobile",
    "sec-ch-ua-full-version",
    "sec-ch-ua-platform-version",
};

const unsigned kClientHintsNumberOfLegacyHints = 4;

const mojom::FeaturePolicyFeature kClientHintsFeaturePolicyMapping[] = {
    // Legacy Hints that are sent cross-origin regardless of FeaturePolicy when
    // kAllowClientHintsToThirdParty is enabled
    mojom::FeaturePolicyFeature::kClientHintDeviceMemory,
    mojom::FeaturePolicyFeature::kClientHintDPR,
    mojom::FeaturePolicyFeature::kClientHintWidth,
    mojom::FeaturePolicyFeature::kClientHintViewportWidth,
    // End of legacy hints.
    mojom::FeaturePolicyFeature::kClientHintRTT,
    mojom::FeaturePolicyFeature::kClientHintDownlink,
    mojom::FeaturePolicyFeature::kClientHintECT,
    mojom::FeaturePolicyFeature::kClientHintLang,
    mojom::FeaturePolicyFeature::kClientHintUA,
    mojom::FeaturePolicyFeature::kClientHintUAArch,
    mojom::FeaturePolicyFeature::kClientHintUAPlatform,
    mojom::FeaturePolicyFeature::kClientHintUAModel,
    mojom::FeaturePolicyFeature::kClientHintUAMobile,
    mojom::FeaturePolicyFeature::kClientHintUAFullVersion,
    mojom::FeaturePolicyFeature::kClientHintUAPlatformVersion,
};

const size_t kClientHintsMappingsCount = base::size(kClientHintsHeaderMapping);

static_assert(
    base::size(kClientHintsHeaderMapping) ==
        (static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) + 1),
    "Client Hint name table size must match network::mojom::WebClientHintsType "
    "range");

static_assert(base::size(kClientHintsFeaturePolicyMapping) ==
                  kClientHintsMappingsCount,
              "Client Hint table sizes must be identical between names and "
              "feature policies");

const char* const kWebEffectiveConnectionTypeMapping[] = {
    "4g" /* Unknown */, "4g" /* Offline */, "slow-2g" /* Slow 2G */,
    "2g" /* 2G */,      "3g" /* 3G */,      "4g" /* 4G */
};

const size_t kWebEffectiveConnectionTypeMappingCount =
    base::size(kWebEffectiveConnectionTypeMapping);

std::string SerializeLangClientHint(const std::string& raw_language_list) {
  base::StringTokenizer t(raw_language_list, ",");
  std::string result;
  while (t.GetNext()) {
    if (!result.empty())
      result.append(", ");

    result.append("\"");
    result.append(t.token().c_str());
    result.append("\"");
  }
  return result;
}

base::Optional<std::vector<network::mojom::WebClientHintsType>> FilterAcceptCH(
    base::Optional<std::vector<network::mojom::WebClientHintsType>> in,
    bool permit_lang_hints,
    bool permit_ua_hints) {
  if (!in.has_value())
    return base::nullopt;

  std::vector<network::mojom::WebClientHintsType> result;
  for (network::mojom::WebClientHintsType hint : in.value()) {
    // Some hints are supported only conditionally.
    switch (hint) {
      case network::mojom::WebClientHintsType::kLang:
        if (permit_lang_hints)
          result.push_back(hint);
        break;
      case network::mojom::WebClientHintsType::kUA:
      case network::mojom::WebClientHintsType::kUAArch:
      case network::mojom::WebClientHintsType::kUAPlatform:
      case network::mojom::WebClientHintsType::kUAPlatformVersion:
      case network::mojom::WebClientHintsType::kUAModel:
      case network::mojom::WebClientHintsType::kUAMobile:
      case network::mojom::WebClientHintsType::kUAFullVersion:
        if (permit_ua_hints)
          result.push_back(hint);
        break;
      default:
        result.push_back(hint);
    }
  }
  return base::make_optional(std::move(result));
}

bool IsClientHintSentByDefault(network::mojom::WebClientHintsType type) {
  return (type == network::mojom::WebClientHintsType::kUA ||
          type == network::mojom::WebClientHintsType::kUAMobile);
}

// Add a list of Client Hints headers to be removed to the output vector, based
// on FeaturePolicy and the url's origin.
void FindClientHintsToRemove(const FeaturePolicy* feature_policy,
                             const GURL& url,
                             std::vector<std::string>* removed_headers) {
  DCHECK(removed_headers);
  url::Origin origin = url::Origin::Create(url);
  size_t startHint = 0;
  if (base::FeatureList::IsEnabled(features::kAllowClientHintsToThirdParty)) {
    // Do not remove any legacy Client Hints
    startHint = kClientHintsNumberOfLegacyHints;
  }
  for (size_t i = startHint; i < blink::kClientHintsMappingsCount; ++i) {
    // Remove the hint if any is true:
    // * Feature policy is null (we're in a sync XHR case) and the hint is not
    // sent by default.
    // * Feature policy exists and doesn't allow for the hint.
    if ((!feature_policy &&
         !IsClientHintSentByDefault(
             static_cast<network::mojom::WebClientHintsType>(i))) ||
        (feature_policy &&
         !feature_policy->IsFeatureEnabledForOrigin(
             blink::kClientHintsFeaturePolicyMapping[i], origin))) {
      removed_headers->push_back(blink::kClientHintsHeaderMapping[i]);
    }
  }
}

}  // namespace blink
