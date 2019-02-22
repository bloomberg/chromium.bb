// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CODE_CACHE_GENERATED_CODE_CACHE_H_
#define CONTENT_BROWSER_CODE_CACHE_GENERATED_CODE_CACHE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "url/origin.h"

namespace content {

// Cache for storing generated code from the renderer on the disk.
// This cache is keyed on two keys: |resource_url| and |requesting_origin|.
// The |resource_url| is the url of the resource that was requested and the
// |requesting_origin| is the origin that requested this resource. This origin
// is used to enforce site isolation policy on stored code. We don't cache the
// code corresponding to unique origins or invalid URLs.
//
// This uses a simple disk_cache backend. It just stores one data stream and
// stores response_time + generated code as one data blob.
// TODO(mythria): Update this comment if the design changes.
//
// There exists one cache per storage partition and is owned by the storage
// partition. This cache is created, accessed and destroyed on the I/O thread.
class CONTENT_EXPORT GeneratedCodeCache {
 public:
  using ReadDataCallback =
      base::RepeatingCallback<void(const base::Time&,
                                   const std::vector<uint8_t>&)>;
  static const int kResponseTimeSizeInBytes = sizeof(int64_t);

  // Cache type. Used for collecting statistics for JS and Wasm in separate
  // buckets.
  enum CodeCacheType { kJavaScript, kWebAssembly };

  // Used for collecting statistics about cache behaviour.
  enum CacheEntryStatus {
    kHit,
    kMiss,
    kClear,
    kUpdate,
    kCreate,
    kError,
    kIncompleteEntry,
    kMaxValue = kIncompleteEntry
  };

  // Creates a GeneratedCodeCache with the specified path and the maximum size.
  // If |max_size_bytes| is 0, then disk_cache picks a default size based on
  // some heuristics.
  GeneratedCodeCache(const base::FilePath& path,
                     int max_size_bytes,
                     CodeCacheType cache_type);

  ~GeneratedCodeCache();

  // Writes data to the cache. If there is an entry corresponding to
  // <|url|, |origin|> this overwrites the existing data. If there is no entry
  // it creates a new one.
  void WriteData(const GURL& url,
                 const url::Origin& origin,
                 const base::Time& response_time,
                 const std::vector<uint8_t>& data);

  // Fetch entry corresponding to <url, origin> from the cache and pass
  // it using the ReadDataCallback.
  void FetchEntry(const GURL& url, const url::Origin& origin, ReadDataCallback);

  // Delete the entry corresponding to <url, origin>
  void DeleteEntry(const GURL& url, const url::Origin& origin);

  // Clear code cache.
  // TODO(mythria): Add support to conditional clearing based on URL
  // and time range.
  // TODO(mythria): Also check if we can avoid retruning an error code and
  // always call the callback to be consistent with other methods.
  int ClearCache(net::CompletionCallback callback);

  const base::FilePath& path() const { return path_; }

 private:
  class PendingOperation;
  using ScopedBackendPtr = std::unique_ptr<disk_cache::Backend>;

  // State of the backend.
  enum BackendState { kUnInitialized, kInitializing, kInitialized, kFailed };

  // The operation requested.
  enum Operation { kFetch, kWrite, kDelete, kClearCache };

  // Data streams corresponding to each entry.
  enum { kDataIndex = 1 };

  // Creates a simple_disk_cache backend.
  void CreateBackend();
  void DidCreateBackend(
      scoped_refptr<base::RefCountedData<ScopedBackendPtr>> backend_ptr,
      int rv);

  // The requests that are received while tha backend is being initialized
  // are recorded in pending operations list. This function issues all pending
  // operations.
  void IssuePendingOperations();

  // Write entry to cache
  void WriteDataImpl(const std::string& key,
                     scoped_refptr<net::IOBufferWithSize> buffer);
  void OpenCompleteForWriteData(
      scoped_refptr<net::IOBufferWithSize> buffer,
      const std::string& key,
      scoped_refptr<base::RefCountedData<disk_cache::Entry*>> entry,
      int rv);
  void CreateCompleteForWriteData(
      scoped_refptr<net::IOBufferWithSize> buffer,
      scoped_refptr<base::RefCountedData<disk_cache::Entry*>> entry,
      int rv);

  // Fetch entry from cache
  void FetchEntryImpl(const std::string& key, ReadDataCallback);
  void OpenCompleteForReadData(
      ReadDataCallback callback,
      scoped_refptr<base::RefCountedData<disk_cache::Entry*>> entry,
      int rv);
  void ReadDataComplete(ReadDataCallback callback,
                        scoped_refptr<net::IOBufferWithSize> buffer,
                        int rv);

  // Delete entry from cache
  void DeleteEntryImpl(const std::string& key);

  void DoPendingClearCache(net::CompletionCallback callback);
  void PendingClearComplete(net::CompletionCallback callback, int rv);

  void CollectStatistics(GeneratedCodeCache::CacheEntryStatus status);

  std::unique_ptr<disk_cache::Backend> backend_;
  BackendState backend_state_;

  std::vector<std::unique_ptr<PendingOperation>> pending_ops_;

  base::FilePath path_;
  int max_size_bytes_;
  CodeCacheType cache_type_;

  base::WeakPtrFactory<GeneratedCodeCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedCodeCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CODE_CACHE_GENERATED_CODE_CACHE_H_
