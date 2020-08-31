// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_request_mojom_traits.h"

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/http_request_headers_mojom_traits.h"
#include "services/network/public/cpp/network_ipc_param_traits.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace network {
namespace {

TEST(URLRequestMojomTraitsTest, Roundtrips_URLRequestReferrerPolicy) {
  for (auto referrer_policy :
       {net::URLRequest::ReferrerPolicy::
            CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::URLRequest::ReferrerPolicy::
            REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
        net::URLRequest::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
        net::URLRequest::ReferrerPolicy::NEVER_CLEAR_REFERRER,
        net::URLRequest::ReferrerPolicy::ORIGIN,
        net::URLRequest::ReferrerPolicy::
            CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN,
        net::URLRequest::ReferrerPolicy::
            ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::URLRequest::ReferrerPolicy::NO_REFERRER}) {
    int32_t serialized = -1;
    using URLRequestReferrerPolicySerializer =
        mojo::internal::Serializer<mojom::URLRequestReferrerPolicy,
                                   net::URLRequest::ReferrerPolicy>;
    URLRequestReferrerPolicySerializer::Serialize(referrer_policy, &serialized);
    EXPECT_EQ(referrer_policy, serialized);
    net::URLRequest::ReferrerPolicy deserialized;
    URLRequestReferrerPolicySerializer::Deserialize(serialized, &deserialized);
    EXPECT_EQ(serialized, deserialized);
  }
}

TEST(URLRequestMojomTraitsTest, Roundtrips_ResourceRequest) {
  network::ResourceRequest original;
  original.method = "POST";
  original.url = GURL("https://example.com/resources/dummy.xml");
  original.site_for_cookies =
      net::SiteForCookies::FromUrl(GURL("https://example.com/index.html"));
  original.force_ignore_site_for_cookies = true;
  original.update_first_party_url_on_redirect = false;
  original.request_initiator = url::Origin::Create(original.url);
  original.isolated_world_origin =
      url::Origin::Create(GURL("chrome-extension://blah"));
  original.referrer = GURL("https://referrer.com/");
  original.referrer_policy =
      net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
  original.headers.SetHeader("Accept", "text/xml");
  original.cors_exempt_headers.SetHeader("X-Requested-With", "ForTesting");
  original.load_flags = 3;
  original.resource_type = 2;
  original.priority = net::IDLE;
  original.should_reset_appcache = true;
  original.is_external_request = false;
  original.cors_preflight_policy =
      mojom::CorsPreflightPolicy::kConsiderPreflight;
  original.originated_from_service_worker = false;
  original.skip_service_worker = false;
  original.mode = mojom::RequestMode::kNoCors;
  original.credentials_mode = mojom::CredentialsMode::kInclude;
  original.redirect_mode = mojom::RedirectMode::kFollow;
  original.fetch_integrity = "dummy_fetch_integrity";
  original.keepalive = true;
  original.has_user_gesture = false;
  original.enable_load_timing = true;
  original.enable_upload_progress = false;
  original.do_not_prompt_for_login = true;
  original.render_frame_id = 5;
  original.is_main_frame = true;
  original.transition_type = 0;
  original.report_raw_headers = true;
  original.previews_state = 0;
  original.upgrade_if_insecure = true;
  original.is_revalidating = false;
  original.throttling_profile_id = base::UnguessableToken::Create();
  original.fetch_window_id = base::UnguessableToken::Create();

  original.trusted_params = ResourceRequest::TrustedParams();
  original.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RedirectMode::kUpdateTopFrame,
      url::Origin::Create(original.url), url::Origin::Create(original.url),
      original.site_for_cookies);
  original.trusted_params->disable_secure_dns = true;

  original.trust_token_params = network::mojom::TrustTokenParams();
  original.trust_token_params->issuer =
      url::Origin::Create(GURL("https://issuer.com"));
  original.trust_token_params->type =
      mojom::TrustTokenOperationType::kRedemption;
  original.trust_token_params->include_timestamp_header = true;
  original.trust_token_params->sign_request_data =
      mojom::TrustTokenSignRequestData::kInclude;
  original.trust_token_params->additional_signed_headers.push_back(
      "some_header");

  network::ResourceRequest copied;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::URLRequest>(&original,
                                                                     &copied));
  EXPECT_TRUE(original.EqualsForTesting(copied));
}

}  // namespace
}  // namespace network
