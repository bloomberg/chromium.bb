// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resource_request.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"

namespace network {

namespace {

mojo::PendingRemote<mojom::CookieAccessObserver> Clone(
    mojo::PendingRemote<mojom::CookieAccessObserver>* observer) {
  if (!*observer)
    return mojo::NullRemote();
  mojo::Remote<mojom::CookieAccessObserver> remote(std::move(*observer));
  mojo::PendingRemote<mojom::CookieAccessObserver> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  *observer = remote.Unbind();
  return new_remote;
}

}  // namespace

ResourceRequest::TrustedParams::TrustedParams() = default;
ResourceRequest::TrustedParams::~TrustedParams() = default;

ResourceRequest::TrustedParams::TrustedParams(const TrustedParams& other) {
  *this = other;
}

ResourceRequest::TrustedParams& ResourceRequest::TrustedParams::operator=(
    const TrustedParams& other) {
  isolation_info = other.isolation_info;
  disable_secure_dns = other.disable_secure_dns;
  has_user_activation = other.has_user_activation;
  cookie_observer =
      Clone(&const_cast<mojo::PendingRemote<mojom::CookieAccessObserver>&>(
          other.cookie_observer));
  return *this;
}

bool ResourceRequest::TrustedParams::EqualsForTesting(
    const TrustedParams& trusted_params) const {
  return isolation_info.IsEqualForTesting(trusted_params.isolation_info) &&
         disable_secure_dns == trusted_params.disable_secure_dns &&
         has_user_activation == trusted_params.has_user_activation;
}

ResourceRequest::ResourceRequest() {}
ResourceRequest::ResourceRequest(const ResourceRequest& request) = default;
ResourceRequest::~ResourceRequest() {}

bool ResourceRequest::EqualsForTesting(const ResourceRequest& request) const {
  if ((trusted_params && !request.trusted_params) ||
      (!trusted_params && request.trusted_params)) {
    return false;
  }
  if (trusted_params && request.trusted_params) {
    if (!trusted_params->EqualsForTesting(*request.trusted_params))
      return false;
  }
  return method == request.method && url == request.url &&
         site_for_cookies.IsEquivalent(request.site_for_cookies) &&
         force_ignore_site_for_cookies ==
             request.force_ignore_site_for_cookies &&
         update_first_party_url_on_redirect ==
             request.update_first_party_url_on_redirect &&
         request_initiator == request.request_initiator &&
         isolated_world_origin == request.isolated_world_origin &&
         referrer == request.referrer &&
         referrer_policy == request.referrer_policy &&
         headers.ToString() == request.headers.ToString() &&
         cors_exempt_headers.ToString() ==
             request.cors_exempt_headers.ToString() &&
         load_flags == request.load_flags &&
         resource_type == request.resource_type &&
         priority == request.priority &&
         should_reset_appcache == request.should_reset_appcache &&
         is_external_request == request.is_external_request &&
         cors_preflight_policy == request.cors_preflight_policy &&
         originated_from_service_worker ==
             request.originated_from_service_worker &&
         skip_service_worker == request.skip_service_worker &&
         corb_detachable == request.corb_detachable &&
         corb_excluded == request.corb_excluded && mode == request.mode &&
         credentials_mode == request.credentials_mode &&
         redirect_mode == request.redirect_mode &&
         fetch_integrity == request.fetch_integrity &&
         destination == request.destination &&
         request_body == request.request_body &&
         keepalive == request.keepalive &&
         has_user_gesture == request.has_user_gesture &&
         enable_load_timing == request.enable_load_timing &&
         enable_upload_progress == request.enable_upload_progress &&
         do_not_prompt_for_login == request.do_not_prompt_for_login &&
         render_frame_id == request.render_frame_id &&
         is_main_frame == request.is_main_frame &&
         transition_type == request.transition_type &&
         report_raw_headers == request.report_raw_headers &&
         previews_state == request.previews_state &&
         upgrade_if_insecure == request.upgrade_if_insecure &&
         is_revalidating == request.is_revalidating &&
         throttling_profile_id == request.throttling_profile_id &&
         custom_proxy_pre_cache_headers.ToString() ==
             request.custom_proxy_pre_cache_headers.ToString() &&
         custom_proxy_post_cache_headers.ToString() ==
             request.custom_proxy_post_cache_headers.ToString() &&
         fetch_window_id == request.fetch_window_id &&
         devtools_request_id == request.devtools_request_id &&
         is_signed_exchange_prefetch_cache_enabled ==
             request.is_signed_exchange_prefetch_cache_enabled &&
         obey_origin_policy == request.obey_origin_policy &&
         recursive_prefetch_token == request.recursive_prefetch_token &&
         trust_token_params == request.trust_token_params;
}

bool ResourceRequest::SendsCookies() const {
  return credentials_mode == network::mojom::CredentialsMode::kInclude &&
         !(load_flags & net::LOAD_DO_NOT_SEND_COOKIES);
}

bool ResourceRequest::SavesCookies() const {
  return credentials_mode == network::mojom::CredentialsMode::kInclude &&
         !(load_flags & net::LOAD_DO_NOT_SAVE_COOKIES);
}

net::URLRequest::ReferrerPolicy ReferrerPolicyForUrlRequest(
    mojom::ReferrerPolicy referrer_policy) {
  switch (referrer_policy) {
    case mojom::ReferrerPolicy::kAlways:
      return net::URLRequest::NEVER_CLEAR_REFERRER;
    case mojom::ReferrerPolicy::kNever:
      return net::URLRequest::NO_REFERRER;
    case mojom::ReferrerPolicy::kOrigin:
      return net::URLRequest::ORIGIN;
    case mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return net::URLRequest::
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
    case mojom::ReferrerPolicy::kSameOrigin:
      return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN;
    case mojom::ReferrerPolicy::kStrictOrigin:
      return net::URLRequest::
          ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case mojom::ReferrerPolicy::kDefault:
      CHECK(false);
      return net::URLRequest::NO_REFERRER;
    case mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return net::URLRequest::
          REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  }
  return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
}

}  // namespace network
