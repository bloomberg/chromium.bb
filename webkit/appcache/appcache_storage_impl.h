// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_

#include "base/file_path.h"
#include "webkit/appcache/mock_appcache_storage.h"

namespace appcache {

// TODO(michaeln): write me, for now we derive from 'mock' storage.
class AppCacheStorageImpl : public MockAppCacheStorage {
 public:
  explicit AppCacheStorageImpl(AppCacheService* service)
      : MockAppCacheStorage(service), is_incognito_(false) {}

  void Initialize(const FilePath& cache_directory);

 private:
  bool is_incognito_;
  FilePath cache_directory_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_
