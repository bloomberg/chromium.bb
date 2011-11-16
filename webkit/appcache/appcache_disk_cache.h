// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_DISK_CACHE_H_
#define WEBKIT_APPCACHE_APPCACHE_DISK_CACHE_H_

#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/disk_cache/disk_cache.h"
#include "webkit/appcache/appcache_export.h"
#include "webkit/appcache/appcache_response.h"

namespace appcache {

// An implementation of AppCacheDiskCacheInterface that
// uses net::DiskCache as the backing store.
class APPCACHE_EXPORT AppCacheDiskCache
    : public AppCacheDiskCacheInterface {
 public:
  AppCacheDiskCache();
  virtual ~AppCacheDiskCache();

  // Initializes the object to use disk backed storage.
  int InitWithDiskBackend(const FilePath& disk_cache_directory,
                          int disk_cache_size, bool force,
                          base::MessageLoopProxy* cache_thread,
                          net::OldCompletionCallback* callback);

  // Initializes the object to use memory only storage.
  // This is used for Chrome's incognito browsing.
  int InitWithMemBackend(int disk_cache_size,
                         net::OldCompletionCallback* callback);

  void Disable();
  bool is_disabled() const { return is_disabled_; }

  virtual int CreateEntry(int64 key, Entry** entry,
                          net::OldCompletionCallback* callback) OVERRIDE;
  virtual int OpenEntry(int64 key, Entry** entry,
                        net::OldCompletionCallback* callback) OVERRIDE;
  virtual int DoomEntry(int64 key,
                        net::OldCompletionCallback* callback) OVERRIDE;

 private:
  class EntryImpl;

  class CreateBackendCallback
      : public net::CancelableOldCompletionCallback<AppCacheDiskCache> {
   public:
    typedef net::CancelableOldCompletionCallback<AppCacheDiskCache> BaseClass;
    CreateBackendCallback(AppCacheDiskCache* object,
                          void (AppCacheDiskCache::* method)(int))
        : BaseClass(object, method), backend_ptr_(NULL) {}

    disk_cache::Backend* backend_ptr_;  // Accessed directly.
   private:
    virtual ~CreateBackendCallback() {
      delete backend_ptr_;
    }
  };

  // PendingCalls allow CreateEntry, OpenEntry, and DoomEntry to be called
  // immediately after construction, without waiting for the
  // underlying disk_cache::Backend to be fully constructed. Early
  // calls are queued up and serviced once the disk_cache::Backend is
  // really ready to go.
  enum PendingCallType {
    CREATE,
    OPEN,
    DOOM
  };
  struct PendingCall {
    PendingCallType call_type;
    int64 key;
    Entry** entry;
    net::OldCompletionCallback* callback;

    PendingCall()
        : call_type(CREATE), key(0), entry(NULL), callback(NULL) {}
    PendingCall(PendingCallType call_type, int64 key,
                Entry** entry, net::OldCompletionCallback* callback)
        : call_type(call_type), key(key), entry(entry), callback(callback) {}
  };
  typedef std::vector<PendingCall> PendingCalls;

  class ActiveCall;
  typedef std::set<ActiveCall*> ActiveCalls;

  bool is_initializing() const {
    return create_backend_callback_.get() != NULL;
  }
  disk_cache::Backend* disk_cache() { return disk_cache_.get(); }
  int Init(net::CacheType cache_type, const FilePath& directory,
           int cache_size, bool force, base::MessageLoopProxy* cache_thread,
           net::OldCompletionCallback* callback);
  void OnCreateBackendComplete(int rv);
  void AddActiveCall(ActiveCall* call) { active_calls_.insert(call); }
  void RemoveActiveCall(ActiveCall* call) { active_calls_.erase(call); }

  bool is_disabled_;
  net::OldCompletionCallback* init_callback_;
  scoped_refptr<CreateBackendCallback> create_backend_callback_;
  PendingCalls pending_calls_;
  ActiveCalls active_calls_;
  scoped_ptr<disk_cache::Backend> disk_cache_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_DISK_CACHE_H_

