// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "third_party/leveldatabase/leveldb_chrome.h"

#include <memory>
#include "base/sys_info.h"

using leveldb::Cache;
using leveldb::NewLRUCache;

namespace leveldb_chrome {

namespace {

size_t DefaultBlockCacheSize() {
  if (base::SysInfo::IsLowEndDevice())
    return 1 << 20;  // 1MB
  else
    return 8 << 20;  // 8MB
}

// Singleton owning resources shared by Chrome's leveldb databases.
class Globals {
 public:
  static Globals* GetInstance() {
    static Globals* globals = new Globals();
    return globals;
  }

  Globals() : browser_block_cache_(NewLRUCache(DefaultBlockCacheSize())) {
    if (!base::SysInfo::IsLowEndDevice())
      web_block_cache_.reset(NewLRUCache(DefaultBlockCacheSize()));
  }

  Cache* web_block_cache() const {
    if (web_block_cache_)
      return web_block_cache_.get();
    return browser_block_cache();
  }

  Cache* browser_block_cache() const { return browser_block_cache_.get(); }

 private:
  ~Globals() {}

  std::unique_ptr<Cache> web_block_cache_;      // null on low end devices.
  std::unique_ptr<Cache> browser_block_cache_;  // Never null.

  DISALLOW_COPY_AND_ASSIGN(Globals);
};

}  // namespace

// Returns a separate (from the default) block cache for use by web APIs.
// This must be used when opening the databases accessible to Web-exposed APIs,
// so rogue pages can't mount a denial of service attack by hammering the block
// cache. Without separate caches, such an attack might slow down Chrome's UI to
// the point where the user can't close the offending page's tabs.
Cache* GetSharedWebBlockCache() {
  return Globals::GetInstance()->web_block_cache();
}

Cache* GetSharedBrowserBlockCache() {
  return Globals::GetInstance()->browser_block_cache();
}

}  // namespace leveldb_chrome
