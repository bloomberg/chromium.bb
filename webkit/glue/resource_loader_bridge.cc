// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_loader_bridge.h"

#include "webkit/appcache/appcache_interfaces.h"
#include "net/http/http_response_headers.h"

namespace webkit_glue {

ResourceDevToolsInfo::ResourceDevToolsInfo()
    : http_status_code(0) {
}

ResourceDevToolsInfo::~ResourceDevToolsInfo() {}

ResourceResponseInfo::ResourceResponseInfo()
    : content_length(-1),
      encoded_data_length(-1),
      appcache_id(appcache::kNoCacheId),
      was_fetched_via_spdy(false),
      was_npn_negotiated(false),
      was_alternate_protocol_available(false),
      was_fetched_via_proxy(false) {
}

ResourceResponseInfo::~ResourceResponseInfo() {}

ResourceLoaderBridge::RequestInfo::RequestInfo()
    : referrer_policy(WebKit::WebReferrerPolicyDefault),
      load_flags(0),
      requestor_pid(0),
      request_type(ResourceType::MAIN_FRAME),
      priority(net::LOW),
      request_context(0),
      appcache_host_id(0),
      routing_id(0),
      download_to_file(false),
      has_user_gesture(false),
      extra_data(NULL) {
}

ResourceLoaderBridge::RequestInfo::~RequestInfo() {}

ResourceLoaderBridge::SyncLoadResponse::SyncLoadResponse() {}

ResourceLoaderBridge::SyncLoadResponse::~SyncLoadResponse() {}

ResourceLoaderBridge::ResourceLoaderBridge() {}

ResourceLoaderBridge::~ResourceLoaderBridge() {}

}  // namespace webkit_glue
