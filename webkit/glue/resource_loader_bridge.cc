// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_loader_bridge.h"

#include "webkit/appcache/appcache_interfaces.h"
#include "net/http/http_response_headers.h"

namespace webkit_glue {

ResourceLoaderBridge::RequestInfo::RequestInfo()
    : load_flags(0),
      requestor_pid(0),
      request_type(ResourceType::MAIN_FRAME),
      request_context(0),
      appcache_host_id(0),
      routing_id(0) {
}

ResourceLoaderBridge::RequestInfo::~RequestInfo() {
}

ResourceLoaderBridge::ResponseInfo::ResponseInfo() {
  content_length = -1;
  appcache_id = appcache::kNoCacheId;
}

ResourceLoaderBridge::ResponseInfo::~ResponseInfo() {
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
