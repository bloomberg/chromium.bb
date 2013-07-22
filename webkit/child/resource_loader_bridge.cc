// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/child/resource_loader_bridge.h"

#include "net/http/http_response_headers.h"
#include "webkit/common/appcache/appcache_interfaces.h"
#include "webkit/common/resource_response_info.h"

namespace webkit_glue {

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
