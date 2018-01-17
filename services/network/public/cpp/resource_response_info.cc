// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resource_response_info.h"

#include "net/http/http_response_headers.h"

namespace network {

ResourceResponseInfo::ResourceResponseInfo()
    : is_legacy_symantec_cert(false),
      content_length(-1),
      encoded_data_length(-1),
      encoded_body_length(-1),
      appcache_id(0),
      was_fetched_via_spdy(false),
      was_alpn_negotiated(false),
      was_alternate_protocol_available(false),
      connection_info(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      was_fetched_via_service_worker(false),
      was_fallback_required_by_service_worker(false),
      response_type_via_service_worker(mojom::FetchResponseType::kDefault),
      previews_state(0),
      effective_connection_type(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      cert_status(0),
      ssl_connection_status(0),
      ssl_key_exchange_group(0),
      did_service_worker_navigation_preload(false),
      blocked_cross_site_document(false) {}

ResourceResponseInfo::ResourceResponseInfo(const ResourceResponseInfo& other) =
    default;

ResourceResponseInfo::~ResourceResponseInfo() {}

}  // namespace network
