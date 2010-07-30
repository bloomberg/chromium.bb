// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_disk_cache.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "net/base/net_errors.h"

namespace appcache {

AppCacheDiskCache::AppCacheDiskCache()
    : is_disabled_(false), init_callback_(NULL) {
}

AppCacheDiskCache::~AppCacheDiskCache() {
  if (create_backend_callback_) {
    create_backend_callback_->Cancel();
    create_backend_callback_.release();
    OnCreateBackendComplete(net::ERR_ABORTED);
  }
}

int AppCacheDiskCache::InitWithDiskBackend(
    const FilePath& disk_cache_directory, int disk_cache_size, bool force,
    base::MessageLoopProxy* cache_thread, net::CompletionCallback* callback) {
  return Init(net::APP_CACHE, disk_cache_directory,
              disk_cache_size, force, cache_thread, callback);
}

int AppCacheDiskCache::InitWithMemBackend(
    int mem_cache_size, net::CompletionCallback* callback) {
  return Init(net::MEMORY_CACHE, FilePath(), mem_cache_size, false, NULL,
              callback);
}

void AppCacheDiskCache::Disable() {
  if (is_disabled_)
    return;

  is_disabled_ = true;

  if (create_backend_callback_) {
    create_backend_callback_->Cancel();
    create_backend_callback_.release();
    OnCreateBackendComplete(net::ERR_ABORTED);
  }
}

int AppCacheDiskCache::CreateEntry(int64 key, disk_cache::Entry** entry,
                                   net::CompletionCallback* callback) {
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(CREATE, key, entry, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_.get())
    return net::ERR_FAILED;

  return disk_cache_->CreateEntry(base::Int64ToString(key), entry, callback);
}

int AppCacheDiskCache::OpenEntry(int64 key, disk_cache::Entry** entry,
                                 net::CompletionCallback* callback) {
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(OPEN, key, entry, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_.get())
    return net::ERR_FAILED;

  return disk_cache_->OpenEntry(base::Int64ToString(key), entry, callback);
}

int AppCacheDiskCache::DoomEntry(int64 key,
                                 net::CompletionCallback* callback) {
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(DOOM, key, NULL, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_.get())
    return net::ERR_FAILED;

  return disk_cache_->DoomEntry(base::Int64ToString(key), callback);
}

int AppCacheDiskCache::Init(net::CacheType cache_type,
                            const FilePath& cache_directory,
                            int cache_size, bool force,
                            base::MessageLoopProxy* cache_thread,
                            net::CompletionCallback* callback) {
  DCHECK(!is_initializing() && !disk_cache_.get());
  is_disabled_ = false;
  create_backend_callback_ = new CreateBackendCallback(
      this, &AppCacheDiskCache::OnCreateBackendComplete);

  int rv = disk_cache::CreateCacheBackend(
      cache_type, cache_directory, cache_size, force, cache_thread,
      &(create_backend_callback_->backend_ptr_), create_backend_callback_);
  if (rv == net::ERR_IO_PENDING)
    init_callback_ = callback;
  else
    OnCreateBackendComplete(rv);
  return rv;
}

void AppCacheDiskCache::OnCreateBackendComplete(int rv) {
  if (rv == net::OK) {
    disk_cache_.reset(create_backend_callback_->backend_ptr_);
    create_backend_callback_->backend_ptr_ = NULL;
  }
  create_backend_callback_ = NULL;

  // Invoke our clients callback function.
  if (init_callback_) {
    init_callback_->Run(rv);
    init_callback_ = NULL;
  }

  // Service pending calls that were queued up while we were initailizating.
  for (PendingCalls::const_iterator iter = pending_calls_.begin();
       iter < pending_calls_.end(); ++iter) {
    int rv = net::ERR_FAILED;
    switch (iter->call_type) {
      case CREATE:
        rv = CreateEntry(iter->key, iter->entry, iter->callback);
        break;
      case OPEN:
        rv = OpenEntry(iter->key, iter->entry, iter->callback);
        break;
      case DOOM:
        rv = DoomEntry(iter->key, iter->callback);
        break;
      default:
        NOTREACHED();
        break;
    }
    if (rv != net::ERR_IO_PENDING)
      iter->callback->Run(rv);
  }
  pending_calls_.clear();
}

}  // namespace appcache

