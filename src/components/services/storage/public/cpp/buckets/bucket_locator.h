// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_LOCATOR_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_LOCATOR_H_

#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace storage {

// Information used to locate a bucket's data on disk or in databases.
//
// The information in a BucketLocator does not change throughout the bucket's
// lifetime.
struct COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT) BucketLocator {
  // Creates null locator.
  BucketLocator();
  BucketLocator(BucketId bucket_id,
                blink::StorageKey storage_key,
                blink::mojom::StorageType type,
                bool is_default);

  ~BucketLocator();

  BucketLocator(const BucketLocator&);
  BucketLocator(BucketLocator&&) noexcept;
  BucketLocator& operator=(const BucketLocator&);
  BucketLocator& operator=(BucketLocator&&) noexcept;

  COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT)
  friend bool operator==(const BucketLocator& lhs, const BucketLocator& rhs);

  COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT)
  friend bool operator!=(const BucketLocator& lhs, const BucketLocator& rhs);

  COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT)
  friend bool operator<(const BucketLocator& lhs, const BucketLocator& rhs);

  BucketId id;
  blink::StorageKey storage_key;
  blink::mojom::StorageType type = blink::mojom::StorageType::kUnknown;
  bool is_default = false;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_LOCATOR_H_
