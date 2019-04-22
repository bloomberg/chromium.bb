// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_

#include <stdint.h>
#include <string>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

// Typemapped to network.mojom::URLRequest, see comments there for details of
// each field.
// Note: Please revise EqualsForTesting accordingly on any updates to this
// struct.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) ResourceRequest {
  ResourceRequest();
  ResourceRequest(const ResourceRequest& request);
  ~ResourceRequest();

  bool EqualsForTesting(const ResourceRequest& request) const;

  std::string method = "GET";
  GURL url;
  GURL site_for_cookies;
  base::Optional<url::Origin> top_frame_origin;
  bool attach_same_site_cookies = false;
  bool update_first_party_url_on_redirect = false;
  base::Optional<url::Origin> request_initiator;
  GURL referrer;
  net::URLRequest::ReferrerPolicy referrer_policy =
      net::URLRequest::NEVER_CLEAR_REFERRER;
  bool is_prerendering = false;
  net::HttpRequestHeaders headers;
  net::HttpRequestHeaders cors_exempt_headers;
  int load_flags = 0;
  bool allow_credentials = true;
  int plugin_child_id = -1;
  int resource_type = 0;
  net::RequestPriority priority = net::IDLE;
  int appcache_host_id = 0;
  bool should_reset_appcache = false;
  bool is_external_request = false;
  mojom::CorsPreflightPolicy cors_preflight_policy =
      mojom::CorsPreflightPolicy::kConsiderPreflight;
  bool originated_from_service_worker = false;
  bool skip_service_worker = false;
  mojom::FetchRequestMode fetch_request_mode = mojom::FetchRequestMode::kNoCors;
  mojom::FetchCredentialsMode fetch_credentials_mode =
      mojom::FetchCredentialsMode::kInclude;
  mojom::FetchRedirectMode fetch_redirect_mode =
      mojom::FetchRedirectMode::kFollow;
  std::string fetch_integrity;
  int fetch_request_context_type = 0;
  mojom::RequestContextFrameType fetch_frame_type =
      mojom::RequestContextFrameType::kAuxiliary;
  scoped_refptr<ResourceRequestBody> request_body;
  bool keepalive = false;
  bool has_user_gesture = false;
  bool enable_load_timing = false;
  bool enable_upload_progress = false;
  bool do_not_prompt_for_login = false;
  int render_frame_id = MSG_ROUTING_NONE;
  bool is_main_frame = false;
  int transition_type = 0;
  bool allow_download = false;
  bool report_raw_headers = false;
  int previews_state = 0;
  bool initiated_in_secure_context = false;
  bool upgrade_if_insecure = false;
  bool is_revalidating = false;
  base::Optional<base::UnguessableToken> throttling_profile_id;
  net::HttpRequestHeaders custom_proxy_pre_cache_headers;
  net::HttpRequestHeaders custom_proxy_post_cache_headers;
  bool custom_proxy_use_alternate_proxy_list = false;
  base::Optional<base::UnguessableToken> fetch_window_id;
  base::Optional<std::string> devtools_request_id;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_H_
