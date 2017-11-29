// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_QUOTA_MESSAGES_STRUCT_TRAITS_H_
#define CONTENT_COMMON_QUOTA_MESSAGES_STRUCT_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "storage/common/quota/quota_status_code.h"
#include "storage/common/quota/quota_types.h"
#include "storage/common/quota/quota_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<storage::mojom::StorageType, storage::StorageType> {
  static storage::mojom::StorageType ToMojom(
      storage::StorageType storage_type) {
    switch (storage_type) {
      case storage::kStorageTypeTemporary:
        return storage::mojom::StorageType::kTemporary;
      case storage::kStorageTypePersistent:
        return storage::mojom::StorageType::kPersistent;
      case storage::kStorageTypeSyncable:
        return storage::mojom::StorageType::kSyncable;
      case storage::kStorageTypeQuotaNotManaged:
        return storage::mojom::StorageType::kQuotaNotManaged;
      case storage::kStorageTypeUnknown:
        return storage::mojom::StorageType::kUnknown;
    }
    NOTREACHED();
    return storage::mojom::StorageType::kUnknown;
  }

  static bool FromMojom(storage::mojom::StorageType storage_type,
                        storage::StorageType* out) {
    switch (storage_type) {
      case storage::mojom::StorageType::kTemporary:
        *out = storage::kStorageTypeTemporary;
        return true;
      case storage::mojom::StorageType::kPersistent:
        *out = storage::kStorageTypePersistent;
        return true;
      case storage::mojom::StorageType::kSyncable:
        *out = storage::kStorageTypeSyncable;
        return true;
      case storage::mojom::StorageType::kQuotaNotManaged:
        *out = storage::kStorageTypeQuotaNotManaged;
        return true;
      case storage::mojom::StorageType::kUnknown:
        *out = storage::kStorageTypeUnknown;
        return true;
    }
    NOTREACHED();
    *out = storage::kStorageTypeUnknown;
    return false;
  }
};

template <>
struct EnumTraits<storage::mojom::QuotaStatusCode, storage::QuotaStatusCode> {
  static storage::mojom::QuotaStatusCode ToMojom(
      storage::QuotaStatusCode status_code) {
    switch (status_code) {
      case storage::kQuotaStatusOk:
        return storage::mojom::QuotaStatusCode::kOk;
      case storage::kQuotaErrorNotSupported:
        return storage::mojom::QuotaStatusCode::kErrorNotSupported;
      case storage::kQuotaErrorInvalidModification:
        return storage::mojom::QuotaStatusCode::kErrorInvalidModification;
      case storage::kQuotaErrorInvalidAccess:
        return storage::mojom::QuotaStatusCode::kErrorInvalidAccess;
      case storage::kQuotaErrorAbort:
        return storage::mojom::QuotaStatusCode::kErrorAbort;
      case storage::kQuotaStatusUnknown:
        return storage::mojom::QuotaStatusCode::kUnknown;
    }
    NOTREACHED();
    return storage::mojom::QuotaStatusCode::kUnknown;
  }

  static bool FromMojom(storage::mojom::QuotaStatusCode status_code,
                        storage::QuotaStatusCode* out) {
    switch (status_code) {
      case storage::mojom::QuotaStatusCode::kOk:
        *out = storage::kQuotaStatusOk;
        return true;
      case storage::mojom::QuotaStatusCode::kErrorNotSupported:
        *out = storage::kQuotaErrorNotSupported;
        return true;
      case storage::mojom::QuotaStatusCode::kErrorInvalidModification:
        *out = storage::kQuotaErrorInvalidModification;
        return true;
      case storage::mojom::QuotaStatusCode::kErrorInvalidAccess:
        *out = storage::kQuotaErrorInvalidAccess;
        return true;
      case storage::mojom::QuotaStatusCode::kErrorAbort:
        *out = storage::kQuotaErrorAbort;
        return true;
      case storage::mojom::QuotaStatusCode::kUnknown:
        *out = storage::kQuotaStatusUnknown;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_QUOTA_MESSAGES_STRUCT_TRAITS_H_
