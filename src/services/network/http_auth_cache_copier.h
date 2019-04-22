// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_HTTP_AUTH_CACHE_COPIER_H_
#define SERVICES_NETWORK_HTTP_AUTH_CACHE_COPIER_H_

#include <map>

#include "base/macros.h"
#include "base/unguessable_token.h"

namespace net {
class HttpAuthCache;
}

namespace network {

// Facilitates copying the contents of one HttpAuthCache into another via an
// intermediate cache. This allows copying between two HttpAuthCache instances
// that cannot both be accessed at the same time, such as the HttpAuthCaches
// in two NetworkContexts.
class HttpAuthCacheCopier {
 public:
  HttpAuthCacheCopier();
  ~HttpAuthCacheCopier();

  // Saves the contents of the given HttpAuthCache, and returns a key that can
  // be used to load the saved contents into another HttpAuthCache via the
  // LoadHttpAuthCache method.
  base::UnguessableToken SaveHttpAuthCache(const net::HttpAuthCache& cache);

  // Loads the HttpAuthCache previously saved via a call to SaveHttpAuthCache
  // into the given HttpAuthCache instance.
  void LoadHttpAuthCache(const base::UnguessableToken& key,
                         net::HttpAuthCache* cache);

 private:
  std::map<base::UnguessableToken, net::HttpAuthCache> caches_;

  DISALLOW_COPY_AND_ASSIGN(HttpAuthCacheCopier);
};

}  // namespace network

#endif  // SERVICES_NETWORK_HTTP_AUTH_CACHE_COPIER_H_
