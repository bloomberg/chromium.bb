// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_QUOTA_QUOTA_TYPES_H_
#define STORAGE_COMMON_QUOTA_QUOTA_TYPES_H_

namespace storage {

enum StorageType {
  kStorageTypeTemporary,
  kStorageTypePersistent,
  kStorageTypeSyncable,
  kStorageTypeQuotaNotManaged,
  kStorageTypeUnknown,
  kStorageTypeLast = kStorageTypeUnknown
};

}  // namespace storage

#endif  // STORAGE_COMMON_QUOTA_QUOTA_TYPES_H_
