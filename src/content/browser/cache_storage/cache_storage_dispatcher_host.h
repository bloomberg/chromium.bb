// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_index.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_associated_binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace url {
class Origin;
}

namespace content {

class CacheStorageContextImpl;

// Handles Cache Storage related messages sent to the browser process from
// child processes. One host instance exists per child process. All
// messages are processed on the IO thread.
class CONTENT_EXPORT CacheStorageDispatcherHost
    : public base::RefCountedThreadSafe<CacheStorageDispatcherHost,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  CacheStorageDispatcherHost();

  // Runs on UI thread.
  void Init(CacheStorageContextImpl* context);

  // Binds Mojo request to this instance, must be called on IO thread.
  // NOTE: The same CacheStorageDispatcherHost instance may be bound to
  // different clients on different origins. Each context is kept on
  // BindingSet's context. This guarantees that the browser process uses the
  // origin of the client known at the binding time, instead of relying on the
  // client to provide its origin at every method call.
  void AddBinding(blink::mojom::CacheStorageRequest request,
                  const url::Origin& origin);

 private:
  // Friends to allow BrowserThread::DeleteOnIOThread delegation.
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<CacheStorageDispatcherHost>;

  class CacheStorageImpl;
  class CacheImpl;
  friend class CacheImpl;

  void AddCacheBinding(
      std::unique_ptr<CacheImpl> cache_impl,
      blink::mojom::CacheStorageCacheAssociatedRequest request);

  CacheStorageHandle OpenCacheStorage(const url::Origin& origin);

  ~CacheStorageDispatcherHost();

  // Called by Init() on IO thread.
  void CreateCacheListener(CacheStorageContextImpl* context);

  scoped_refptr<CacheStorageContextImpl> context_;

  mojo::StrongBindingSet<blink::mojom::CacheStorage> bindings_;
  mojo::StrongAssociatedBindingSet<blink::mojom::CacheStorageCache>
      cache_bindings_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
