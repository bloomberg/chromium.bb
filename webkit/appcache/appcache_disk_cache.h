// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_DISK_CACHE_H_
#define WEBKIT_APPCACHE_APPCACHE_DISK_CACHE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/disk_cache/disk_cache.h"

namespace appcache {

// A thin wrapper around net::DiskCache that does a couple of things for us.
//
// 1. Translates int64 keys used by the appcache into std::string
//    keys used by net::DiskCache.
// 2. Allows CreateEntry, OpenEntry, and DoomEntry to be called
//    immediately after construction, without waiting for the
//    underlying disk_cache::Backend to be fully constructed. Early
//    calls are queued up and serviced once the disk_cache::Backend is
//    really ready to go.
class AppCacheDiskCache {
 public:
  AppCacheDiskCache();
  ~AppCacheDiskCache();

  // Initializes the object to use disk backed storage.
  int InitWithDiskBackend(const FilePath& disk_cache_directory,
                          int disk_cache_size, bool force,
                          base::MessageLoopProxy* cache_thread,
                          net::CompletionCallback* callback);

  // Initializes the object to use memory only storage.
  // This is used for Chrome's incognito browsing.
  int InitWithMemBackend(int disk_cache_size,
                         net::CompletionCallback* callback);

  void Disable();
  bool is_disabled() const { return is_disabled_; }

  int CreateEntry(int64 key, disk_cache::Entry** entry,
                  net::CompletionCallback* callback);
  int OpenEntry(int64 key, disk_cache::Entry** entry,
                net::CompletionCallback* callback);
  int DoomEntry(int64 key, net::CompletionCallback* callback);

 private:
  class CreateBackendCallback
      : public net::CancelableCompletionCallback<AppCacheDiskCache> {
   public:
    typedef net::CancelableCompletionCallback<AppCacheDiskCache> BaseClass;
    CreateBackendCallback(AppCacheDiskCache* object,
                          void (AppCacheDiskCache::* method)(int))
        : BaseClass(object, method), backend_ptr_(NULL) {}

    disk_cache::Backend* backend_ptr_;  // Accessed directly.
   private:
    virtual ~CreateBackendCallback() {
      delete backend_ptr_;
    }
  };

  enum PendingCallType {
    CREATE,
    OPEN,
    DOOM
  };
  struct PendingCall {
    PendingCallType call_type;
    int64 key;
    disk_cache::Entry** entry;
    net::CompletionCallback* callback;

    PendingCall()
        : call_type(CREATE), key(0), entry(NULL), callback(NULL) {}

    PendingCall(PendingCallType call_type, int64 key,
                disk_cache::Entry** entry, net::CompletionCallback* callback)
        : call_type(call_type), key(key), entry(entry), callback(callback) {}
  };
  typedef std::vector<PendingCall> PendingCalls;

  bool is_initializing() const {
    return create_backend_callback_.get() != NULL;
  }
  int Init(net::CacheType cache_type, const FilePath& directory,
           int cache_size, bool force, base::MessageLoopProxy* cache_thread,
           net::CompletionCallback* callback);
  void OnCreateBackendComplete(int rv);

  bool is_disabled_;
  net::CompletionCallback* init_callback_;
  scoped_refptr<CreateBackendCallback> create_backend_callback_;
  PendingCalls pending_calls_;
  scoped_ptr<disk_cache::Backend> disk_cache_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_DISK_CACHE_H_

