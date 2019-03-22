// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net_helpers.h"

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "base/logging.h"
#include "base/macros.h"
#include "net/base/load_flags.h"

namespace android_webview {

int GetCacheModeForClient(AwContentsIoThreadClient* client) {
  AwContentsIoThreadClient::CacheMode cache_mode = client->GetCacheMode();
  switch (cache_mode) {
    case AwContentsIoThreadClient::LOAD_CACHE_ELSE_NETWORK:
      // If the resource is in the cache (even if expired), load from cache.
      // Otherwise, fall back to network.
      return net::LOAD_SKIP_CACHE_VALIDATION;
    case AwContentsIoThreadClient::LOAD_NO_CACHE:
      // Always load from the network, don't use the cache.
      return net::LOAD_BYPASS_CACHE;
    case AwContentsIoThreadClient::LOAD_CACHE_ONLY:
      // If the resource is in the cache (even if expired), load from cache. Do
      // not fall back to the network.
      return net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
    default:
      // If the resource is in the cache (and is valid), load from cache.
      // Otherwise, fall back to network. This is the usual (default) case.
      return 0;
  }
}

}  // namespace android_webview
