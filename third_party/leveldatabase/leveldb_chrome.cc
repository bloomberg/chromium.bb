// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "third_party/leveldatabase/leveldb_chrome.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sys_info.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "util/mutexlock.h"

using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;
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

    memory_pressure_listener_.reset(new base::MemoryPressureListener(
        base::Bind(&Globals::OnMemoryPressure, base::Unretained(this))));
  }

  Cache* web_block_cache() const {
    if (web_block_cache_)
      return web_block_cache_.get();
    return browser_block_cache();
  }

  Cache* browser_block_cache() const { return browser_block_cache_.get(); }

  // Called when the system is under memory pressure.
  void OnMemoryPressure(MemoryPressureLevel memory_pressure_level) {
    if (memory_pressure_level ==
        MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE)
      return;
    browser_block_cache()->Prune();
    if (browser_block_cache() == web_block_cache())
      return;
    web_block_cache()->Prune();
  }

  void DidCreateChromeMemEnv(leveldb::Env* env) {
    leveldb::MutexLock l(&env_mutex_);
    DCHECK(in_memory_envs_.find(env) == in_memory_envs_.end());
    in_memory_envs_.insert(env);
  }

  void WillDestroyChromeMemEnv(leveldb::Env* env) {
    leveldb::MutexLock l(&env_mutex_);
    DCHECK(in_memory_envs_.find(env) != in_memory_envs_.end());
    in_memory_envs_.erase(env);
  }

  bool IsInMemoryEnv(const leveldb::Env* env) const {
    leveldb::MutexLock l(&env_mutex_);
    return in_memory_envs_.find(env) != in_memory_envs_.end();
  }

 private:
  ~Globals() {}

  std::unique_ptr<Cache> web_block_cache_;      // null on low end devices.
  std::unique_ptr<Cache> browser_block_cache_;  // Never null.
  // Listens for the system being under memory pressure.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  mutable leveldb::port::Mutex env_mutex_;
  base::flat_set<leveldb::Env*> in_memory_envs_;

  DISALLOW_COPY_AND_ASSIGN(Globals);
};

class ChromeMemEnv : public leveldb::EnvWrapper {
 public:
  ChromeMemEnv(leveldb::Env* base_env)
      : EnvWrapper(leveldb::NewMemEnv(base_env)), base_env_(target()) {
    Globals::GetInstance()->DidCreateChromeMemEnv(this);
  }

  ~ChromeMemEnv() override {
    Globals::GetInstance()->WillDestroyChromeMemEnv(this);
  }

 private:
  std::unique_ptr<leveldb::Env> base_env_;
  DISALLOW_COPY_AND_ASSIGN(ChromeMemEnv);
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

bool IsMemEnv(const leveldb::Env* env) {
  DCHECK(env);
  return Globals::GetInstance()->IsInMemoryEnv(env);
}

leveldb::Env* NewMemEnv(leveldb::Env* base_env) {
  return new ChromeMemEnv(base_env);
}

}  // namespace leveldb_chrome
