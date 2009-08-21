// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_loader_bridge.h"

#include "webkit/appcache/appcache_interfaces.h"
#include "net/http/http_response_headers.h"

namespace webkit_glue {

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
