// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_loader_bridge.h"

#include "webkit/appcache/appcache_interfaces.h"
#include "net/http/http_response_headers.h"

namespace webkit_glue {

ResourceLoadTimingInfo::ResourceLoadTimingInfo()
    : proxy_start(-1),
      proxy_end(-1),
      dns_start(-1),
      dns_end(-1),
      connect_start(-1),
      connect_end(-1),
      ssl_start(-1),
      ssl_end(-1),
      send_start(0),
      send_end(0),
      receive_headers_start(0),
      receive_headers_end(0) {
}

ResourceLoadTimingInfo::~ResourceLoadTimingInfo() {
}

ResourceDevToolsInfo::ResourceDevToolsInfo()
    : http_status_code(0) {
}

ResourceDevToolsInfo::~ResourceDevToolsInfo() {}

ResourceResponseInfo::ResourceResponseInfo()
    : content_length(-1),
      encoded_data_length(-1),
      appcache_id(appcache::kNoCacheId),
      connection_id(0),
      connection_reused(false),
      was_fetched_via_spdy(false),
      was_npn_negotiated(false),
      was_alternate_protocol_available(false),
      was_fetched_via_proxy(false) {
}

ResourceResponseInfo::~ResourceResponseInfo() {
}

ResourceLoaderBridge::RequestInfo::RequestInfo()
    : load_flags(0),
      requestor_pid(0),
      request_type(ResourceType::MAIN_FRAME),
      request_context(0),
      appcache_host_id(0),
      routing_id(0),
      download_to_file(false),
      has_user_gesture(false) {
}

ResourceLoaderBridge::RequestInfo::~RequestInfo() {
}

ResourceLoaderBridge::SyncLoadResponse::SyncLoadResponse() {
}

ResourceLoaderBridge::SyncLoadResponse::~SyncLoadResponse() {
}

ResourceLoaderBridge::ResourceLoaderBridge() {
}

ResourceLoaderBridge::~ResourceLoaderBridge() {
}

}  // namespace webkit_glue
