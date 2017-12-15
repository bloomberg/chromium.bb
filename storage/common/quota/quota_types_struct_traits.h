// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_QUOTA_MESSAGES_STRUCT_TRAITS_H_
#define CONTENT_COMMON_QUOTA_MESSAGES_STRUCT_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "storage/common/quota/quota_types.h"
#include "storage/common/quota/quota_types.mojom.h"
#include "third_party/WebKit/common/quota/quota_status_code.h"

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
struct EnumTraits<storage::mojom::QuotaStatusCode, blink::QuotaStatusCode> {
  static storage::mojom::QuotaStatusCode ToMojom(
      blink::QuotaStatusCode status_code) {
    switch (status_code) {
      case blink::QuotaStatusCode::kOk:
        return storage::mojom::QuotaStatusCode::kOk;
      case blink::QuotaStatusCode::kErrorNotSupported:
        return storage::mojom::QuotaStatusCode::kErrorNotSupported;
      case blink::QuotaStatusCode::kErrorInvalidModification:
        return storage::mojom::QuotaStatusCode::kErrorInvalidModification;
      case blink::QuotaStatusCode::kErrorInvalidAccess:
        return storage::mojom::QuotaStatusCode::kErrorInvalidAccess;
      case blink::QuotaStatusCode::kErrorAbort:
        return storage::mojom::QuotaStatusCode::kErrorAbort;
      case blink::QuotaStatusCode::kUnknown:
        return storage::mojom::QuotaStatusCode::kUnknown;
    }
    NOTREACHED();
    return storage::mojom::QuotaStatusCode::kUnknown;
  }

  static bool FromMojom(storage::mojom::QuotaStatusCode status_code,
                        blink::QuotaStatusCode* out) {
    switch (status_code) {
      case storage::mojom::QuotaStatusCode::kOk:
        *out = blink::QuotaStatusCode::kOk;
        return true;
      case storage::mojom::QuotaStatusCode::kErrorNotSupported:
        *out = blink::QuotaStatusCode::kErrorNotSupported;
        return true;
      case storage::mojom::QuotaStatusCode::kErrorInvalidModification:
        *out = blink::QuotaStatusCode::kErrorInvalidModification;
        return true;
      case storage::mojom::QuotaStatusCode::kErrorInvalidAccess:
        *out = blink::QuotaStatusCode::kErrorInvalidAccess;
        return true;
      case storage::mojom::QuotaStatusCode::kErrorAbort:
        *out = blink::QuotaStatusCode::kErrorAbort;
        return true;
      case storage::mojom::QuotaStatusCode::kUnknown:
        *out = blink::QuotaStatusCode::kUnknown;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_QUOTA_MESSAGES_STRUCT_TRAITS_H_
