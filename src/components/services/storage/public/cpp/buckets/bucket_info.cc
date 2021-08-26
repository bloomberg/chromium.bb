// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/buckets/bucket_info.h"

namespace storage {

BucketInfo::BucketInfo(BucketId bucket_id,
                       blink::StorageKey storage_key,
                       blink::mojom::StorageType type,
                       std::string name,
                       base::Time expiration,
                       int64_t quota)
    : id(std::move(bucket_id)),
      storage_key(std::move(storage_key)),
      type(type),
      name(std::move(name)),
      expiration(std::move(expiration)),
      quota(quota) {}

BucketInfo::BucketInfo() = default;
BucketInfo::~BucketInfo() = default;

BucketInfo::BucketInfo(const BucketInfo&) = default;
BucketInfo::BucketInfo(BucketInfo&&) noexcept = default;
BucketInfo& BucketInfo::operator=(const BucketInfo&) = default;
BucketInfo& BucketInfo::operator=(BucketInfo&&) noexcept = default;

bool operator==(const BucketInfo& lhs, const BucketInfo& rhs) {
  return lhs.id == rhs.id;
}

bool operator!=(const BucketInfo& lhs, const BucketInfo& rhs) {
  return !(lhs == rhs);
}

bool operator<(const BucketInfo& lhs, const BucketInfo& rhs) {
  return lhs.id < rhs.id;
}

}  // namespace storage
