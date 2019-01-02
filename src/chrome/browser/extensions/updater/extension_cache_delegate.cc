// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_cache_delegate.h"

namespace extensions {

ExtensionCacheDelegate::~ExtensionCacheDelegate() {
}

size_t ExtensionCacheDelegate::GetMinimumCacheSize() const {
  // Default minimum size of local cache on disk, in bytes.
  static constexpr int kDefaultMinimumCacheSize = 1024 * 1024;
  return kDefaultMinimumCacheSize;
}

size_t ExtensionCacheDelegate::GetMaximumCacheSize() const {
  // Default maximum size of local cache on disk, in bytes.
  static constexpr size_t kDefaultCacheSizeLimit = 256 * 1024 * 1024;
  return kDefaultCacheSizeLimit;
}

base::TimeDelta ExtensionCacheDelegate::GetMaximumCacheAge() const {
  // Maximum age of unused extensions in the cache.
  static constexpr base::TimeDelta kMaxCacheAge = base::TimeDelta::FromDays(30);
  return kMaxCacheAge;
}

}  // namespace extensions
