// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/navigator_auction.h"

#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/public/mojom/parakeet/ad_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_usvstring_usvstringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_properties.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_request_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_targeting.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ads.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_adproperties_adpropertiessequence.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/ad_auction/ads.h"
#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"
#include "third_party/blink/renderer/modules/geolocation/geolocation_coordinates.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Error string builders.

String ErrorInvalidInterestGroup(const AuctionAdInterestGroup& group,
                                 const String& field_name,
                                 const String& field_value,
                                 const String& error) {
  return String::Format(
      "%s '%s' for AuctionAdInterestGroup with owner '%s' and name '%s' %s",
      field_name.Utf8().c_str(), field_value.Utf8().c_str(),
      group.owner().Utf8().c_str(), group.name().Utf8().c_str(),
      error.Utf8().c_str());
}

String ErrorInvalidInterestGroupJson(const AuctionAdInterestGroup& group,
                                     const String& field_name) {
  return String::Format(
      "%s for AuctionAdInterestGroup with owner '%s' and name '%s' must be a "
      "JSON-serializable object.",
      field_name.Utf8().c_str(), group.owner().Utf8().c_str(),
      group.name().Utf8().c_str());
}

String ErrorInvalidAuctionConfig(const AuctionAdConfig& config,
                                 const String& field_name,
                                 const String& field_value,
                                 const String& error) {
  return String::Format("%s '%s' for AuctionAdConfig with seller '%s' %s",
                        field_name.Utf8().c_str(), field_value.Utf8().c_str(),
                        config.seller().Utf8().c_str(), error.Utf8().c_str());
}

String ErrorInvalidAuctionConfigJson(const AuctionAdConfig& config,
                                     const String& field_name) {
  return String::Format(
      "%s for AuctionAdConfig with seller '%s' must be a JSON-serializable "
      "object.",
      field_name.Utf8().c_str(), config.seller().Utf8().c_str());
}

String ErrorInvalidAdRequestConfig(const AdRequestConfig& config,
                                   const String& field_name,
                                   const String& field_value,
                                   const String& error) {
  return String::Format("%s '%s' for AdRequestConfig with URL '%s' %s",
                        field_name.Utf8().c_str(), field_value.Utf8().c_str(),
                        config.adRequestUrl().Utf8().c_str(),
                        error.Utf8().c_str());
}

String WarningPermissionsPolicy(const String& feature, const String& api) {
  return String::Format(
      "In the future, feature %s will not be enabled by default by Permissions "
      "Policy (thus calling %s will be rejected with NotAllowedError) in cross-"
      "origin iframes or same-origin iframes nested in cross-origin iframes",
      feature.Utf8().c_str(), api.Utf8().c_str());
}

// JSON and Origin conversion helpers.

bool Jsonify(const ScriptState& script_state,
             const v8::Local<v8::Value>& value,
             String& output) {
  v8::Local<v8::String> v8_string;
  if (!v8::JSON::Stringify(script_state.GetContext(), value)
           .ToLocal(&v8_string)) {
    return false;
  }
  output = ToCoreString(v8_string);
  // JSON.stringify can fail to produce a string value in one of two ways: it
  // can throw an exception (as with unserializable objects), or it can return
  // `undefined` (as with e.g. passing a function). If JSON.stringify returns
  // `undefined`, the v8 API then coerces it to the string value "undefined".
  // Check for this, and consider it a failure (since we didn't properly
  // serialize a value, and v8::JSON::Parse() rejects "undefined").
  return output != "undefined";
}

// Returns nullptr if |origin_string| couldn't be parsed into an acceptable
// origin.
scoped_refptr<const SecurityOrigin> ParseOrigin(const String& origin_string) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(origin_string);
  if (origin->Protocol() != url::kHttpsScheme)
    return nullptr;
  return origin;
}

// WebIDL -> Mojom copy functions -- each return true if successful (including
// the not present, nothing to copy case), returns false and throws JS exception
// for invalid input.

// joinAdInterestGroup() copy functions.

bool CopyOwnerFromIdlToMojo(const ExecutionContext& execution_context,
                            ExceptionState& exception_state,
                            const AuctionAdInterestGroup& input,
                            mojom::blink::InterestGroup& output) {
  scoped_refptr<const SecurityOrigin> owner = ParseOrigin(input.owner());
  if (!owner) {
    exception_state.ThrowTypeError(String::Format(
        "owner '%s' for AuctionAdInterestGroup with name '%s' must be a valid "
        "https origin.",
        input.owner().Utf8().c_str(), input.name().Utf8().c_str()));
    return false;
  }

  if (!execution_context.GetSecurityOrigin()->IsSameOriginWith(owner.get())) {
    exception_state.ThrowTypeError(String::Format(
        "owner '%s' for AuctionAdInterestGroup with name '%s' match frame "
        "origin '%s'.",
        input.owner().Utf8().c_str(), input.name().Utf8().c_str(),
        execution_context.GetSecurityOrigin()->ToRawString().Utf8().c_str()));
    return false;
  }

  output.owner = std::move(owner);
  return true;
}

bool CopyBiddingLogicUrlFromIdlToMojo(const ExecutionContext& context,
                                      ExceptionState& exception_state,
                                      const AuctionAdInterestGroup& input,
                                      mojom::blink::InterestGroup& output) {
  if (!input.hasBiddingLogicUrl())
    return true;
  KURL bidding_url = context.CompleteURL(input.biddingLogicUrl());
  if (!bidding_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "biddingLogicUrl", input.biddingLogicUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  // TODO(https://crbug.com/1271540): Validate against interest group owner
  // origin.
  output.bidding_url = bidding_url;
  return true;
}

bool CopyWasmHelperUrlFromIdlToMojo(const ExecutionContext& context,
                                    ExceptionState& exception_state,
                                    const AuctionAdInterestGroup& input,
                                    mojom::blink::InterestGroup& output) {
  if (!input.hasBiddingWasmHelperUrl())
    return true;
  KURL wasm_url = context.CompleteURL(input.biddingWasmHelperUrl());
  if (!wasm_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "biddingWasmHelperUrl", input.biddingWasmHelperUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  // ValidateBlinkInterestGroup will checks whether this follows all the rules.
  output.bidding_wasm_helper_url = wasm_url;
  return true;
}

bool CopyDailyUpdateUrlFromIdlToMojo(const ExecutionContext& context,
                                     ExceptionState& exception_state,
                                     const AuctionAdInterestGroup& input,
                                     mojom::blink::InterestGroup& output) {
  if (!input.hasDailyUpdateUrl())
    return true;
  KURL daily_update_url = context.CompleteURL(input.dailyUpdateUrl());
  if (!daily_update_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "dailyUpdateUrl", input.dailyUpdateUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  // TODO(https://crbug.com/1271540): Validate against interest group owner
  // origin.
  output.update_url = daily_update_url;
  return true;
}

bool CopyTrustedBiddingSignalsUrlFromIdlToMojo(
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  if (!input.hasTrustedBiddingSignalsUrl())
    return true;
  KURL trusted_bidding_signals_url =
      context.CompleteURL(input.trustedBiddingSignalsUrl());
  if (!trusted_bidding_signals_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "trustedBiddingSignalsUrl", input.trustedBiddingSignalsUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  // TODO(https://crbug.com/1271540): Validate against interest group owner
  // origin.
  output.trusted_bidding_signals_url = trusted_bidding_signals_url;
  return true;
}

bool CopyTrustedBiddingSignalsKeysFromIdlToMojo(
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  if (!input.hasTrustedBiddingSignalsKeys())
    return true;
  output.trusted_bidding_signals_keys.emplace();
  for (const auto& key : input.trustedBiddingSignalsKeys()) {
    output.trusted_bidding_signals_keys->push_back(key);
  }
  return true;
}

bool CopyUserBiddingSignalsFromIdlToMojo(const ScriptState& script_state,
                                         ExceptionState& exception_state,
                                         const AuctionAdInterestGroup& input,
                                         mojom::blink::InterestGroup& output) {
  if (!input.hasUserBiddingSignals())
    return true;
  if (!Jsonify(script_state, input.userBiddingSignals().V8Value(),
               output.user_bidding_signals)) {
    exception_state.ThrowTypeError(
        ErrorInvalidInterestGroupJson(input, "userBiddingSignals"));
    return false;
  }

  return true;
}

bool CopyAdsFromIdlToMojo(const ExecutionContext& context,
                          const ScriptState& script_state,
                          ExceptionState& exception_state,
                          const AuctionAdInterestGroup& input,
                          mojom::blink::InterestGroup& output) {
  if (!input.hasAds())
    return true;
  output.ads.emplace();
  for (const auto& ad : input.ads()) {
    auto mojo_ad = mojom::blink::InterestGroupAd::New();
    KURL render_url = context.CompleteURL(ad->renderUrl());
    if (!render_url.IsValid()) {
      exception_state.ThrowTypeError(
          ErrorInvalidInterestGroup(input, "ad renderUrl", ad->renderUrl(),
                                    "cannot be resolved to a valid URL."));
      return false;
    }
    mojo_ad->render_url = render_url;
    if (ad->hasMetadata()) {
      if (!Jsonify(script_state, ad->metadata().V8Value(), mojo_ad->metadata)) {
        exception_state.ThrowTypeError(
            ErrorInvalidInterestGroupJson(input, "ad metadata"));
        return false;
      }
    }
    output.ads->push_back(std::move(mojo_ad));
  }
  return true;
}

bool CopyAdComponentsFromIdlToMojo(const ExecutionContext& context,
                                   const ScriptState& script_state,
                                   ExceptionState& exception_state,
                                   const AuctionAdInterestGroup& input,
                                   mojom::blink::InterestGroup& output) {
  if (!input.hasAdComponents())
    return true;
  output.ad_components.emplace();
  for (const auto& ad : input.adComponents()) {
    auto mojo_ad = mojom::blink::InterestGroupAd::New();
    KURL render_url = context.CompleteURL(ad->renderUrl());
    if (!render_url.IsValid()) {
      exception_state.ThrowTypeError(
          ErrorInvalidInterestGroup(input, "ad renderUrl", ad->renderUrl(),
                                    "cannot be resolved to a valid URL."));
      return false;
    }
    mojo_ad->render_url = render_url;
    if (ad->hasMetadata()) {
      if (!Jsonify(script_state, ad->metadata().V8Value(), mojo_ad->metadata)) {
        exception_state.ThrowTypeError(
            ErrorInvalidInterestGroupJson(input, "ad metadata"));
        return false;
      }
    }
    output.ad_components->push_back(std::move(mojo_ad));
  }
  return true;
}

// createAdRequest copy functions.
bool CopyAdRequestUrlFromIdlToMojo(const ExecutionContext& context,
                                   ExceptionState& exception_state,
                                   const AdRequestConfig& input,
                                   mojom::blink::AdRequestConfig& output) {
  KURL ad_request_url = context.CompleteURL(input.adRequestUrl());
  if (!ad_request_url.IsValid() ||
      (ad_request_url.Protocol() != url::kHttpsScheme)) {
    exception_state.ThrowTypeError(
        String::Format("adRequestUrl '%s' for AdRequestConfig must "
                       "be a valid https origin.",
                       input.adRequestUrl().Utf8().c_str()));
    return false;
  }
  output.ad_request_url = ad_request_url;
  return true;
}

bool CopyAdPropertiesFromIdlToMojo(const ExecutionContext& context,
                                   ExceptionState& exception_state,
                                   const AdRequestConfig& input,
                                   mojom::blink::AdRequestConfig& output) {
  if (!input.hasAdProperties()) {
    exception_state.ThrowTypeError(
        ErrorInvalidAdRequestConfig(input, "adProperties", input.adRequestUrl(),
                                    "must be provided to createAdRequest."));
    return false;
  }

  // output.ad_properties = mojom::blink::AdProperties::New();
  switch (input.adProperties()->GetContentType()) {
    case V8UnionAdPropertiesOrAdPropertiesSequence::ContentType::
        kAdProperties: {
      const auto* ad_properties = input.adProperties()->GetAsAdProperties();
      auto mojo_ad_properties = mojom::blink::AdProperties::New();
      mojo_ad_properties->width =
          ad_properties->hasWidth() ? ad_properties->width() : "";
      mojo_ad_properties->height =
          ad_properties->hasHeight() ? ad_properties->height() : "";
      mojo_ad_properties->slot =
          ad_properties->hasSlot() ? ad_properties->slot() : "";
      mojo_ad_properties->lang =
          ad_properties->hasLang() ? ad_properties->lang() : "";
      mojo_ad_properties->ad_type =
          ad_properties->hasAdtype() ? ad_properties->adtype() : "";
      mojo_ad_properties->bid_floor =
          ad_properties->hasBidFloor() ? ad_properties->bidFloor() : 0.0;

      output.ad_properties.push_back(std::move(mojo_ad_properties));
      break;
    }
    case V8UnionAdPropertiesOrAdPropertiesSequence::ContentType::
        kAdPropertiesSequence: {
      if (input.adProperties()->GetAsAdPropertiesSequence().size() <= 0) {
        exception_state.ThrowTypeError(ErrorInvalidAdRequestConfig(
            input, "adProperties", input.adRequestUrl(),
            "must be non-empty to createAdRequest."));
        return false;
      }

      for (const auto& ad_properties :
           input.adProperties()->GetAsAdPropertiesSequence()) {
        auto mojo_ad_properties = mojom::blink::AdProperties::New();
        mojo_ad_properties->width =
            ad_properties->hasWidth() ? ad_properties->width() : "";
        mojo_ad_properties->height =
            ad_properties->hasHeight() ? ad_properties->height() : "";
        mojo_ad_properties->slot =
            ad_properties->hasSlot() ? ad_properties->slot() : "";
        mojo_ad_properties->lang =
            ad_properties->hasLang() ? ad_properties->lang() : "";
        mojo_ad_properties->ad_type =
            ad_properties->hasAdtype() ? ad_properties->adtype() : "";
        mojo_ad_properties->bid_floor =
            ad_properties->hasBidFloor() ? ad_properties->bidFloor() : 0.0;

        output.ad_properties.push_back(std::move(mojo_ad_properties));
      }
      break;
    }
  }
  return true;
}

bool CopyTargetingFromIdlToMojo(const ExecutionContext& context,
                                ExceptionState& exception_state,
                                const AdRequestConfig& input,
                                mojom::blink::AdRequestConfig& output) {
  if (!input.hasTargeting()) {
    // Targeting information is not required.
    return true;
  }

  output.targeting = mojom::blink::AdTargeting::New();

  if (input.targeting()->hasInterests()) {
    output.targeting->interests.emplace();
    for (const auto& interest : input.targeting()->interests()) {
      output.targeting->interests->push_back(interest);
    }
  }

  if (input.targeting()->hasGeolocation()) {
    output.targeting->geolocation = mojom::blink::AdGeolocation::New();
    output.targeting->geolocation->latitude =
        input.targeting()->geolocation()->latitude();
    output.targeting->geolocation->longitude =
        input.targeting()->geolocation()->longitude();
  }

  return true;
}

bool CopyAdSignalsFromIdlToMojo(const ExecutionContext& context,
                                ExceptionState& exception_state,
                                const AdRequestConfig& input,
                                mojom::blink::AdRequestConfig& output) {
  if (!input.hasAnonymizedProxiedSignals()) {
    // AdSignals information is not required.
    return true;
  }

  output.anonymized_proxied_signals.emplace();

  for (const auto& signal : input.anonymizedProxiedSignals()) {
    if (signal == "coarse-geolocation") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kCourseGeolocation);
    } else if (signal == "coarse-ua") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kCourseUserAgent);
    } else if (signal == "targeting") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kTargeting);
    } else if (signal == "user-ad-interests") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kUserAdInterests);
    }
  }
  return true;
}

bool CopyFallbackSourceFromIdlToMojo(const ExecutionContext& context,
                                     ExceptionState& exception_state,
                                     const AdRequestConfig& input,
                                     mojom::blink::AdRequestConfig& output) {
  if (!input.hasFallbackSource()) {
    // FallbackSource information is not required.
    return true;
  }

  KURL fallback_source = context.CompleteURL(input.fallbackSource());
  if (!fallback_source.IsValid() ||
      (fallback_source.Protocol() != url::kHttpsScheme)) {
    exception_state.ThrowTypeError(
        String::Format("fallbackSource '%s' for AdRequestConfig must "
                       "be a valid https origin.",
                       input.fallbackSource().Utf8().c_str()));
    return false;
  }
  output.fallback_source = fallback_source;
  return true;
}

// runAdAuction() copy functions.

bool CopySellerFromIdlToMojo(ExceptionState& exception_state,
                             const AuctionAdConfig& input,
                             mojom::blink::AuctionAdConfig& output) {
  scoped_refptr<const SecurityOrigin> seller = ParseOrigin(input.seller());
  if (!seller) {
    exception_state.ThrowTypeError(String::Format(
        "seller '%s' for AuctionAdConfig must be a valid https origin.",
        input.seller().Utf8().c_str()));
    return false;
  }
  output.seller = seller;
  return true;
}

bool CopyDecisionLogicUrlFromIdlToMojo(const ExecutionContext& context,
                                       ExceptionState& exception_state,
                                       const AuctionAdConfig& input,
                                       mojom::blink::AuctionAdConfig& output) {
  KURL decision_logic_url = context.CompleteURL(input.decisionLogicUrl());
  if (!decision_logic_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "decisionLogicUrl", input.decisionLogicUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  // TODO(https://crbug.com/1271540): Validate against seller origin.
  output.decision_logic_url = decision_logic_url;
  return true;
}

bool CopyTrustedScoringSignalsFromIdlToMojo(
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasTrustedScoringSignalsUrl())
    return true;
  KURL trusted_scoring_signals_url =
      context.CompleteURL(input.trustedScoringSignalsUrl());
  if (!trusted_scoring_signals_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "trustedScoringSignalsUrl", input.trustedScoringSignalsUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  // TODO(https://crbug.com/1271540): Validate against seller origin.
  output.trusted_scoring_signals_url = trusted_scoring_signals_url;
  return true;
}

bool CopyInterestGroupBuyersFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  DCHECK(!output.auction_ad_config_non_shared_params->interest_group_buyers);

  if (!input.hasInterestGroupBuyers())
    return true;

  Vector<scoped_refptr<const SecurityOrigin>> buyers;
  for (const auto& buyer_str : input.interestGroupBuyers()) {
    scoped_refptr<const SecurityOrigin> buyer = ParseOrigin(buyer_str);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "interestGroupBuyers buyer", buyer_str,
          "must be a valid https origin."));
      return false;
    }
    buyers.push_back(buyer);
  }
  output.auction_ad_config_non_shared_params->interest_group_buyers =
      std::move(buyers);
  return true;
}

bool CopyAuctionSignalsFromIdlToMojo(const ScriptState& script_state,
                                     ExceptionState& exception_state,
                                     const AuctionAdConfig& input,
                                     mojom::blink::AuctionAdConfig& output) {
  if (!input.hasAuctionSignals())
    return true;
  if (!Jsonify(script_state, input.auctionSignals().V8Value(),
               output.auction_ad_config_non_shared_params->auction_signals)) {
    exception_state.ThrowTypeError(
        ErrorInvalidAuctionConfigJson(input, "auctionSignals"));
    return false;
  }
  return true;
}

bool CopySellerSignalsFromIdlToMojo(const ScriptState& script_state,
                                    ExceptionState& exception_state,
                                    const AuctionAdConfig& input,
                                    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasSellerSignals())
    return true;
  if (!Jsonify(script_state, input.sellerSignals().V8Value(),
               output.auction_ad_config_non_shared_params->seller_signals)) {
    exception_state.ThrowTypeError(
        ErrorInvalidAuctionConfigJson(input, "sellerSignals"));
    return false;
  }

  return true;
}

bool CopyPerBuyerSignalsFromIdlToMojo(const ScriptState& script_state,
                                      ExceptionState& exception_state,
                                      const AuctionAdConfig& input,
                                      mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerSignals())
    return true;
  output.auction_ad_config_non_shared_params->per_buyer_signals.emplace();
  for (const auto& per_buyer_signal : input.perBuyerSignals()) {
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_signal.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerSignals buyer", per_buyer_signal.first,
          "must be a valid https origin."));
      return false;
    }
    String buyer_signals_str;
    if (!Jsonify(script_state, per_buyer_signal.second.V8Value(),
                 buyer_signals_str)) {
      exception_state.ThrowTypeError(
          ErrorInvalidAuctionConfigJson(input, "perBuyerSignals"));
      return false;
    }
    output.auction_ad_config_non_shared_params->per_buyer_signals->insert(
        buyer, std::move(buyer_signals_str));
  }

  return true;
}

bool CopyPerBuyerTimeoutsFromIdlToMojo(const ScriptState& script_state,
                                       ExceptionState& exception_state,
                                       const AuctionAdConfig& input,
                                       mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerTimeouts())
    return true;
  output.auction_ad_config_non_shared_params->per_buyer_timeouts.emplace();
  for (const auto& per_buyer_timeout : input.perBuyerTimeouts()) {
    if (per_buyer_timeout.first == "*") {
      output.auction_ad_config_non_shared_params->all_buyers_timeout =
          base::Milliseconds(per_buyer_timeout.second);
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_timeout.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerTimeouts buyer", per_buyer_timeout.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return false;
    }
    output.auction_ad_config_non_shared_params->per_buyer_timeouts->insert(
        buyer, base::Milliseconds(per_buyer_timeout.second));
  }

  return true;
}

// Attempts to convert the AuctionAdConfig `config`, passed in via Javascript,
// to a `mojom::blink::AuctionAdConfig`. Throws a Javascript exception and
// return null on failure.
mojom::blink::AuctionAdConfigPtr IdlAuctionConfigToMojo(
    bool is_top_level,
    ScriptState& script_state,
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const AuctionAdConfig& config) {
  auto mojo_config = mojom::blink::AuctionAdConfig::New();
  mojo_config->auction_ad_config_non_shared_params =
      mojom::blink::AuctionAdConfigNonSharedParams::New();
  if (!CopySellerFromIdlToMojo(exception_state, config, *mojo_config) ||
      !CopyDecisionLogicUrlFromIdlToMojo(context, exception_state, config,
                                         *mojo_config) ||
      !CopyTrustedScoringSignalsFromIdlToMojo(context, exception_state, config,
                                              *mojo_config) ||
      !CopyInterestGroupBuyersFromIdlToMojo(exception_state, config,
                                            *mojo_config) ||
      !CopyAuctionSignalsFromIdlToMojo(script_state, exception_state, config,
                                       *mojo_config) ||
      !CopySellerSignalsFromIdlToMojo(script_state, exception_state, config,
                                      *mojo_config) ||
      !CopyPerBuyerSignalsFromIdlToMojo(script_state, exception_state, config,
                                        *mojo_config) ||
      !CopyPerBuyerTimeoutsFromIdlToMojo(script_state, exception_state, config,
                                         *mojo_config)) {
    return mojom::blink::AuctionAdConfigPtr();
  }

  // Recursively handle component auctions, if there are any.
  if (config.hasComponentAuctions()) {
    for (const auto& idl_component_auction : config.componentAuctions()) {
      // Component auctions may not have their own nested component auctions.
      if (!is_top_level) {
        exception_state.ThrowTypeError(
            "Auctions listed in componentAuctions may not have their own "
            "nested componentAuctions.");
        return mojom::blink::AuctionAdConfigPtr();
      }

      auto mojo_component_auction =
          IdlAuctionConfigToMojo(/*is_top_level=*/false, script_state, context,
                                 exception_state, *idl_component_auction);
      if (!mojo_component_auction)
        return mojom::blink::AuctionAdConfigPtr();
      mojo_config->component_auctions.emplace_back(
          std::move(mojo_component_auction));
    }
  }

  return mojo_config;
}

// finalizeAd() validation methods
bool ValidateAdsObject(ExceptionState& exception_state, const Ads* ads) {
  if (!ads || !ads->IsValid()) {
    exception_state.ThrowTypeError(
        "Ads used for finalizeAds() must be a valid Ads object from "
        "navigator.createAdRequest.");
    return false;
  }
  return true;
}

// Modified from LocalFrame::CountUseIfFeatureWouldBeBlockedByPermissionsPolicy.
// Checks if a feature, which is currently available in all frames, would be
// blocked by our restricted permissions policy EnableForSelf.
// Returns true if the frame is cross-origin relative to the top-level document,
// or if it is same-origin with the top level, but is embedded in any way
// through a cross-origin frame. (A->B->A embedding)
bool FeatureWouldBeBlockedByRestrictedPermissionsPolicy(Navigator& navigator) {
  const Frame* frame = navigator.DomWindow()->GetFrame();
  // Get the origin of the top-level document
  const SecurityOrigin* top_origin =
      frame->Tree().Top().GetSecurityContext()->GetSecurityOrigin();

  // Walk up the frame tree looking for any cross-origin embeds. Even if this
  // frame is same-origin with the top-level, if it is embedded by a cross-
  // origin frame (like A->B->A) it would be blocked without a permissions
  // policy.
  while (!frame->IsMainFrame()) {
    if (!frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            top_origin)) {
      return true;
    }
    frame = frame->Tree().Parent();
  }
  return false;
}

void AddWarningMessageToConsole(ScriptState* script_state,
                                const String& feature,
                                const String& api) {
  auto* window = To<LocalDOMWindow>(ExecutionContext::From(script_state));
  WebLocalFrameImpl::FromFrame(window->GetFrame())
      ->AddMessageToConsole(
          WebConsoleMessage(mojom::ConsoleMessageLevel::kWarning,
                            WarningPermissionsPolicy(feature, api)),
          /*discard_duplicates=*/true);
}

}  // namespace

NavigatorAuction::NavigatorAuction(Navigator& navigator)
    : Supplement(navigator),
      ad_auction_service_(navigator.GetExecutionContext()) {
  navigator.GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      ad_auction_service_.BindNewPipeAndPassReceiver(
          navigator.GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI)));
}

NavigatorAuction& NavigatorAuction::From(ExecutionContext* context,
                                         Navigator& navigator) {
  NavigatorAuction* supplement =
      Supplement<Navigator>::From<NavigatorAuction>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorAuction>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

const char NavigatorAuction::kSupplementName[] = "NavigatorAuction";

void NavigatorAuction::joinAdInterestGroup(ScriptState* script_state,
                                           const AuctionAdInterestGroup* group,
                                           double duration_seconds,
                                           ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);

  auto mojo_group = mojom::blink::InterestGroup::New();
  mojo_group->expiry = base::Time::Now() + base::Seconds(duration_seconds);
  if (!CopyOwnerFromIdlToMojo(*context, exception_state, *group, *mojo_group))
    return;
  mojo_group->name = group->name();
  if (!CopyBiddingLogicUrlFromIdlToMojo(*context, exception_state, *group,
                                        *mojo_group)) {
    return;
  }
  if (!CopyWasmHelperUrlFromIdlToMojo(*context, exception_state, *group,
                                      *mojo_group)) {
    return;
  }
  if (!CopyDailyUpdateUrlFromIdlToMojo(*context, exception_state, *group,
                                       *mojo_group)) {
    return;
  }
  if (!CopyTrustedBiddingSignalsUrlFromIdlToMojo(*context, exception_state,
                                                 *group, *mojo_group)) {
    return;
  }
  if (!CopyTrustedBiddingSignalsKeysFromIdlToMojo(*group, *mojo_group))
    return;
  if (!CopyUserBiddingSignalsFromIdlToMojo(*script_state, exception_state,
                                           *group, *mojo_group)) {
    return;
  }
  if (!CopyAdsFromIdlToMojo(*context, *script_state, exception_state, *group,
                            *mojo_group)) {
    return;
  }
  if (!CopyAdComponentsFromIdlToMojo(*context, *script_state, exception_state,
                                     *group, *mojo_group)) {
    return;
  }

  String error_field_name;
  String error_field_value;
  String error;
  if (!ValidateBlinkInterestGroup(*mojo_group, error_field_name,
                                  error_field_value, error)) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        *group, error_field_name, error_field_value, error));
    return;
  }

  ad_auction_service_->JoinInterestGroup(std::move(mojo_group));
}

/* static */
void NavigatorAuction::joinAdInterestGroup(ScriptState* script_state,
                                           Navigator& navigator,
                                           const AuctionAdInterestGroup* group,
                                           double duration_seconds,
                                           ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return;
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(script_state, "join-ad-interest-group",
                               "joinAdInterestGroup");
  }

  return From(ExecutionContext::From(script_state), navigator)
      .joinAdInterestGroup(script_state, group, duration_seconds,
                           exception_state);
}

void NavigatorAuction::leaveAdInterestGroup(ScriptState* script_state,
                                            const AuctionAdInterestGroup* group,
                                            ExceptionState& exception_state) {
  scoped_refptr<const SecurityOrigin> owner = ParseOrigin(group->owner());
  if (!owner) {
    exception_state.ThrowTypeError("owner '" + group->owner() +
                                   "' for AuctionAdInterestGroup with name '" +
                                   group->name() +
                                   "' must be a valid https origin.");
    return;
  }
  ad_auction_service_->LeaveInterestGroup(owner, group->name());
}

/* static */
void NavigatorAuction::leaveAdInterestGroup(ScriptState* script_state,
                                            Navigator& navigator,
                                            const AuctionAdInterestGroup* group,
                                            ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return;
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(script_state, "join-ad-interest-group",
                               "leaveAdInterestGroup");
  }

  return From(ExecutionContext::From(script_state), navigator)
      .leaveAdInterestGroup(script_state, group, exception_state);
}

void NavigatorAuction::updateAdInterestGroups() {
  ad_auction_service_->UpdateAdInterestGroups();
}

/* static */
void NavigatorAuction::updateAdInterestGroups(ScriptState* script_state,
                                              Navigator& navigator,
                                              ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return;
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(script_state, "join-ad-interest-group",
                               "updateAdInterestGroups");
  }

  return From(context, navigator).updateAdInterestGroups();
}

ScriptPromise NavigatorAuction::runAdAuction(ScriptState* script_state,
                                             const AuctionAdConfig* config,
                                             ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_config = IdlAuctionConfigToMojo(
      /*is_top_level=*/true, *script_state, *context, exception_state, *config);
  if (!mojo_config)
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->RunAdAuction(
      std::move(mojo_config),
      WTF::Bind(&NavigatorAuction::AuctionComplete, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::runAdAuction(ScriptState* script_state,
                                             Navigator& navigator,
                                             const AuctionAdConfig* config,
                                             ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature run-ad-auction is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(script_state, "run-ad-auction", "runAdAuction");
  }

  return From(ExecutionContext::From(script_state), navigator)
      .runAdAuction(script_state, config, exception_state);
}

/* static */
Vector<String> NavigatorAuction::adAuctionComponents(
    ScriptState* script_state,
    Navigator& navigator,
    uint16_t num_ad_components,
    ExceptionState& exception_state) {
  const auto& ad_auction_components =
      navigator.DomWindow()->document()->Loader()->AdAuctionComponents();
  Vector<String> out;
  if (!ad_auction_components) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This frame was not loaded with the "
                                      "result of an interest group auction.");
    return out;
  }

  // Clamp the number of ad components at blink::kMaxAdAuctionAdComponents.
  if (num_ad_components >
      static_cast<int16_t>(blink::kMaxAdAuctionAdComponents)) {
    num_ad_components = blink::kMaxAdAuctionAdComponents;
  }

  DCHECK_EQ(kMaxAdAuctionAdComponents, ad_auction_components->size());

  for (int i = 0; i < num_ad_components; ++i) {
    out.push_back((*ad_auction_components)[i].GetString());
  }
  return out;
}

ScriptPromise NavigatorAuction::deprecatedURNToURL(
    ScriptState* script_state,
    const String& uuid_url_string,
    ExceptionState& exception_state) {
  if (!uuid_url_string.StartsWithIgnoringCase("urn:uuid:")) {
    exception_state.ThrowTypeError(
        String::Format("Passed URL must start with 'urn:uuid:'."));
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  KURL uuid_url(uuid_url_string);
  ad_auction_service_->DeprecatedGetURLFromURN(
      std::move(uuid_url),
      WTF::Bind(&NavigatorAuction::GetURLFromURNComplete, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise NavigatorAuction::deprecatedURNToURL(
    ScriptState* script_state,
    Navigator& navigator,
    const String& uuid_url,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .deprecatedURNToURL(script_state, uuid_url, exception_state);
}

ScriptPromise NavigatorAuction::createAdRequest(
    ScriptState* script_state,
    const AdRequestConfig* config,
    ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_config = mojom::blink::AdRequestConfig::New();

  if (!CopyAdRequestUrlFromIdlToMojo(*context, exception_state, *config,
                                     *mojo_config))
    return ScriptPromise();

  if (!CopyAdPropertiesFromIdlToMojo(*context, exception_state, *config,
                                     *mojo_config))
    return ScriptPromise();

  if (config->hasPublisherCode()) {
    mojo_config->publisher_code = config->publisherCode();
  }

  if (!CopyTargetingFromIdlToMojo(*context, exception_state, *config,
                                  *mojo_config))
    return ScriptPromise();

  if (!CopyAdSignalsFromIdlToMojo(*context, exception_state, *config,
                                  *mojo_config))
    return ScriptPromise();

  if (!CopyFallbackSourceFromIdlToMojo(*context, exception_state, *config,
                                       *mojo_config))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->CreateAdRequest(
      std::move(mojo_config),
      WTF::Bind(&NavigatorAuction::AdsRequested, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::createAdRequest(
    ScriptState* script_state,
    Navigator& navigator,
    const AdRequestConfig* config,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .createAdRequest(script_state, config, exception_state);
}

void NavigatorAuction::AdsRequested(ScriptPromiseResolver* resolver,
                                    const WTF::String&) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // TODO(https://crbug.com/1249186): Add full impl of methods.
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError,
      "createAdRequest API not yet implemented"));
}

ScriptPromise NavigatorAuction::finalizeAd(ScriptState* script_state,
                                           const Ads* ads,
                                           const AuctionAdConfig* config,
                                           ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_config = mojom::blink::AuctionAdConfig::New();

  // For finalizing an Ad PARAKEET only really cares about the decisionLogicUrl,
  // auctionSignals, sellerSignals, and perBuyerSignals. We can ignore
  // copying/validating other fields on AuctionAdConfig.
  if (!CopyDecisionLogicUrlFromIdlToMojo(*context, exception_state, *config,
                                         *mojo_config))
    return ScriptPromise();
  if (!CopyAuctionSignalsFromIdlToMojo(*script_state, exception_state, *config,
                                       *mojo_config))
    return ScriptPromise();
  if (!CopySellerSignalsFromIdlToMojo(*script_state, exception_state, *config,
                                      *mojo_config))
    return ScriptPromise();
  if (!CopyPerBuyerSignalsFromIdlToMojo(*script_state, exception_state, *config,
                                        *mojo_config))
    return ScriptPromise();

  if (!ValidateAdsObject(exception_state, ads))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->FinalizeAd(
      ads->GetGuid(), std::move(mojo_config),
      WTF::Bind(&NavigatorAuction::FinalizeAdComplete, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::finalizeAd(ScriptState* script_state,
                                           Navigator& navigator,
                                           const Ads* ads,
                                           const AuctionAdConfig* config,
                                           ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .finalizeAd(script_state, ads, config, exception_state);
}

void NavigatorAuction::FinalizeAdComplete(
    ScriptPromiseResolver* resolver,
    const absl::optional<KURL>& creative_url) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  if (creative_url) {
    resolver->Resolve(creative_url);
  } else {
    // TODO(https://crbug.com/1249186): Add full impl of methods.
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "finalizeAd API not yet implemented"));
  }
}

void NavigatorAuction::AuctionComplete(ScriptPromiseResolver* resolver,
                                       const absl::optional<KURL>& result_url) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  if (result_url) {
    resolver->Resolve(result_url);
  } else {
    resolver->Resolve(v8::Null(resolver->GetScriptState()->GetIsolate()));
  }
}

void NavigatorAuction::GetURLFromURNComplete(
    ScriptPromiseResolver* resolver,
    const absl::optional<KURL>& decoded_url) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  if (decoded_url) {
    resolver->Resolve(*decoded_url);
  } else {
    resolver->Resolve(v8::Null(resolver->GetScriptState()->GetIsolate()));
  }
}

}  // namespace blink
