// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "content/browser/appcache/appcache_disk_cache.h"
#include "content/browser/appcache/appcache_disk_cache_ops.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

// Wholesale reusage of the appcache code for response reading,
// writing, and storage. See the corresponding class in that
// library for doc comments and other details.
// TODO(michaeln): If this reuse sticks, refactor/move the
// resused classes to a more common location.

class CONTENT_EXPORT ServiceWorkerDiskCache : public AppCacheDiskCache {
 public:
  ServiceWorkerDiskCache();
};

// TODO(crbug.com/1060076): Migrate to
// storage::mojom::ServiceWorkerResourceReader.
class CONTENT_EXPORT ServiceWorkerResponseReader
    : public AppCacheResponseReader {
 public:
  // Reads response headers and metadata associated with this reader from
  // storage. This is an adaptor method of ReadInfo().
  using ReadResponseHeadCallback =
      base::OnceCallback<void(int result,
                              network::mojom::URLResponseHeadPtr response_head,
                              scoped_refptr<net::IOBufferWithSize> metadata)>;
  void ReadResponseHead(ReadResponseHeadCallback callback);

 protected:
  // Should only be constructed by the storage class.
  friend class ServiceWorkerStorage;

  ServiceWorkerResponseReader(int64_t resource_id,
                              base::WeakPtr<AppCacheDiskCache> disk_cache);
};

// TODO(crbug.com/1060076): Migrate to
// storage::mojom::ServiceWorkerResourceWriter.
class CONTENT_EXPORT ServiceWorkerResponseWriter
    : public AppCacheResponseWriter {
 public:
  // Writes response headers for a service worker script to storage. Currently
  // this just converts |response_head| to HttpResponseInfo and calls
  // WriteInfo(). |response_head| must be examined by
  // service_worker_loader_helpers::CheckResponseHead() before calling this
  // method.
  void WriteResponseHead(const network::mojom::URLResponseHead& response_head,
                         int response_data_size,
                         net::CompletionOnceCallback callback);

 protected:
  // Should only be constructed by the storage class.
  friend class ServiceWorkerStorage;

  ServiceWorkerResponseWriter(int64_t resource_id,
                              base::WeakPtr<AppCacheDiskCache> disk_cache);
};

class CONTENT_EXPORT ServiceWorkerResponseMetadataWriter
    : public AppCacheResponseMetadataWriter {
 protected:
  // Should only be constructed by the storage class.
  friend class ServiceWorkerStorage;

  ServiceWorkerResponseMetadataWriter(
      int64_t resource_id,
      base::WeakPtr<AppCacheDiskCache> disk_cache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_
