// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"
#include "url/origin.h"

namespace storage {

// Interface between each storage API and the quota manager.
//
// Each storage API must register an implementation of this interface with
// the quota manager, by calling QuotaManager::RegisterClient().
//
// All the methods will be called on the IO thread in the browser.
//
// When AppCache is deleted, this can become a std::unique_ptr instead
// of refcounted, and owned by the QuotaManager.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaClient
    : public base::RefCountedThreadSafe<QuotaClient> {
 public:
  using GetUsageCallback = base::OnceCallback<void(int64_t usage)>;
  using GetOriginsCallback =
      base::OnceCallback<void(const std::set<url::Origin>& origins)>;
  using DeletionCallback =
      base::OnceCallback<void(blink::mojom::QuotaStatusCode status)>;

  virtual QuotaClientType type() const = 0;

  // Called when the QuotaManager is destroyed.
  virtual void OnQuotaManagerDestroyed() = 0;

  // Called by the QuotaManager.
  // Gets the amount of data stored in the storage specified by
  // |origin| and |type|.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginUsage(const url::Origin& origin,
                              blink::mojom::StorageType type,
                              GetUsageCallback callback) = 0;

  // Called by the QuotaManager.
  // Returns a list of origins that has data in the |type| storage.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginsForType(blink::mojom::StorageType type,
                                 GetOriginsCallback callback) = 0;

  // Called by the QuotaManager.
  // Returns a list of origins that match the |host|.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginsForHost(blink::mojom::StorageType type,
                                 const std::string& host,
                                 GetOriginsCallback callback) = 0;

  // Called by the QuotaManager.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void DeleteOriginData(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                DeletionCallback callback) = 0;

  // Called by the QuotaManager.
  // Gives the QuotaClient an opportunity to perform a cleanup step after major
  // deletions.
  virtual void PerformStorageCleanup(blink::mojom::StorageType type,
                                     base::OnceClosure callback) = 0;

  virtual bool DoesSupport(blink::mojom::StorageType type) const = 0;

 protected:
  friend class RefCountedThreadSafe<QuotaClient>;

  virtual ~QuotaClient() = default;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_
