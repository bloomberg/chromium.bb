// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/http_auth_cache_copier.h"

#include "net/http/http_auth_cache.h"

namespace network {

HttpAuthCacheCopier::HttpAuthCacheCopier() = default;
HttpAuthCacheCopier::~HttpAuthCacheCopier() = default;

base::UnguessableToken HttpAuthCacheCopier::SaveHttpAuthCache(
    const net::HttpAuthCache& cache) {
  base::UnguessableToken key = base::UnguessableToken::Create();
  caches_[key].UpdateAllFrom(cache);
  return key;
}

void HttpAuthCacheCopier::LoadHttpAuthCache(const base::UnguessableToken& key,
                                            net::HttpAuthCache* cache) {
  auto it = caches_.find(key);
  if (it == caches_.end()) {
    DLOG(ERROR) << "Unknown HttpAuthCache key: " << key;
    return;
  }
  cache->UpdateAllFrom(it->second);
  caches_.erase(it);
}

}  // namespace network
