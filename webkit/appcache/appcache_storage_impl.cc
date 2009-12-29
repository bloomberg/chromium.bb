// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_storage_impl.h"

namespace appcache {

void AppCacheStorageImpl::Initialize(const FilePath& cache_directory) {
  is_incognito_ = cache_directory.empty();
  cache_directory_ = cache_directory;
  // TODO(michaeln): retrieve last_ids from storage
}

}  // namespace appcache
