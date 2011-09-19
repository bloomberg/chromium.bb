// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_disk_cache.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "net/base/net_errors.h"

namespace appcache {

// An implementation of AppCacheDiskCacheInterface::Entry that's a thin
// wrapper around disk_cache::Entry.
class AppCacheDiskCache::EntryImpl : public Entry {
 public:
  explicit EntryImpl(disk_cache::Entry* disk_cache_entry)
      : disk_cache_entry_(disk_cache_entry) {
    DCHECK(disk_cache_entry);
  }
  virtual int Read(int index, int64 offset, net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* completion_callback) {
    if (offset < 0 || offset > kint32max)
      return net::ERR_INVALID_ARGUMENT;
    return disk_cache_entry_->ReadData(
        index, static_cast<int>(offset), buf, buf_len, completion_callback);
  }
  virtual int Write(int index, int64 offset, net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* completion_callback) {
    if (offset < 0 || offset > kint32max)
      return net::ERR_INVALID_ARGUMENT;
    const bool kTruncate = true;
    return disk_cache_entry_->WriteData(
        index, static_cast<int>(offset), buf, buf_len,
        completion_callback, kTruncate);
  }
  virtual int64 GetSize(int index) {
    return disk_cache_entry_->GetDataSize(index);
  }
  virtual void Close() {
    disk_cache_entry_->Close();
    disk_cache_entry_ = NULL;
  }

 private:
  disk_cache::Entry* disk_cache_entry_;
};

// Separate object to hold state for each Create, Delete, or Doom call
// while the call is inflight and to produce an EntryImpl upon completion.
class AppCacheDiskCache::ActiveCall {
 public:
  explicit ActiveCall(AppCacheDiskCache* owner)
      : entry_(NULL), callback_(NULL), owner_(owner), entry_ptr_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            async_completion_(this, &ActiveCall::OnAsyncCompletion)) {
  }

  int CreateEntry(int64 key, Entry** entry, net::CompletionCallback* callback) {
    int rv = owner_->disk_cache()->CreateEntry(
        base::Int64ToString(key), &entry_ptr_, &async_completion_);
    return HandleImmediateReturnValue(rv, entry, callback);
  }

  int OpenEntry(int64 key, Entry** entry, net::CompletionCallback* callback) {
    int rv = owner_->disk_cache()->OpenEntry(
        base::Int64ToString(key), &entry_ptr_, &async_completion_);
    return HandleImmediateReturnValue(rv, entry, callback);
  }

  int DoomEntry(int64 key, net::CompletionCallback* callback) {
    int rv = owner_->disk_cache()->DoomEntry(
        base::Int64ToString(key), &async_completion_);
    return HandleImmediateReturnValue(rv, NULL, callback);
  }

 private:
  int HandleImmediateReturnValue(int rv, Entry** entry,
                                 net::CompletionCallback* callback) {
    if (rv == net::ERR_IO_PENDING) {
      // OnAsyncCompletion will be called later.
      callback_ = callback;
      entry_ = entry;
      owner_->AddActiveCall(this);
      return net::ERR_IO_PENDING;
    }
    if (rv == net::OK && entry)
      *entry = new EntryImpl(entry_ptr_);
    delete this;
    return rv;
  }

  void OnAsyncCompletion(int rv) {
    owner_->RemoveActiveCall(this);
    if (rv == net::OK && entry_)
      *entry_ = new EntryImpl(entry_ptr_);
    callback_->Run(rv);
    callback_ = NULL;
    delete this;
  }

  Entry** entry_;
  net::CompletionCallback* callback_;
  AppCacheDiskCache* owner_;
  disk_cache::Entry* entry_ptr_;
  net::CompletionCallbackImpl<ActiveCall> async_completion_;
};

AppCacheDiskCache::AppCacheDiskCache()
    : is_disabled_(false), init_callback_(NULL) {
}

AppCacheDiskCache::~AppCacheDiskCache() {
  if (create_backend_callback_) {
    create_backend_callback_->Cancel();
    create_backend_callback_.release();
    OnCreateBackendComplete(net::ERR_ABORTED);
  }
  disk_cache_.reset();
  STLDeleteElements(&active_calls_);
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

int AppCacheDiskCache::CreateEntry(int64 key, Entry** entry,
                                   net::CompletionCallback* callback) {
  DCHECK(entry && callback);
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(CREATE, key, entry, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_.get())
    return net::ERR_FAILED;

  return (new ActiveCall(this))->CreateEntry(key, entry, callback);
}

int AppCacheDiskCache::OpenEntry(int64 key, Entry** entry,
                                 net::CompletionCallback* callback) {
  DCHECK(entry && callback);
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(OPEN, key, entry, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_.get())
    return net::ERR_FAILED;

  return (new ActiveCall(this))->OpenEntry(key, entry, callback);
}

int AppCacheDiskCache::DoomEntry(int64 key,
                                 net::CompletionCallback* callback) {
  DCHECK(callback);
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(DOOM, key, NULL, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_.get())
    return net::ERR_FAILED;

  return (new ActiveCall(this))->DoomEntry(key, callback);
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
      cache_type, cache_directory, cache_size, force, cache_thread, NULL,
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

