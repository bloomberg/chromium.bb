// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_QUOTA_QUOTA_TYPES_STRUCT_TRAITS_H_
#define THIRD_PARTY_WEBKIT_COMMON_QUOTA_QUOTA_TYPES_STRUCT_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/WebKit/common/quota/quota_status_code.h"
#include "third_party/WebKit/common/quota/quota_types.mojom.h"
#include "third_party/WebKit/common/quota/storage_type.h"

namespace mojo {

template <>
struct EnumTraits<blink::mojom::StorageType, blink::StorageType> {
  static blink::mojom::StorageType ToMojom(blink::StorageType storage_type) {
    switch (storage_type) {
      case blink::StorageType::kTemporary:
        return blink::mojom::StorageType::kTemporary;
      case blink::StorageType::kPersistent:
        return blink::mojom::StorageType::kPersistent;
      case blink::StorageType::kSyncable:
        return blink::mojom::StorageType::kSyncable;
      case blink::StorageType::kQuotaNotManaged:
        return blink::mojom::StorageType::kQuotaNotManaged;
      case blink::StorageType::kUnknown:
        return blink::mojom::StorageType::kUnknown;
    }
    NOTREACHED();
    return blink::mojom::StorageType::kUnknown;
  }

  static bool FromMojom(blink::mojom::StorageType storage_type,
                        blink::StorageType* out) {
    switch (storage_type) {
      case blink::mojom::StorageType::kTemporary:
        *out = blink::StorageType::kTemporary;
        return true;
      case blink::mojom::StorageType::kPersistent:
        *out = blink::StorageType::kPersistent;
        return true;
      case blink::mojom::StorageType::kSyncable:
        *out = blink::StorageType::kSyncable;
        return true;
      case blink::mojom::StorageType::kQuotaNotManaged:
        *out = blink::StorageType::kQuotaNotManaged;
        return true;
      case blink::mojom::StorageType::kUnknown:
        *out = blink::StorageType::kUnknown;
        return true;
    }
    NOTREACHED();
    *out = blink::StorageType::kUnknown;
    return false;
  }
};

template <>
struct EnumTraits<blink::mojom::QuotaStatusCode, blink::QuotaStatusCode> {
  static blink::mojom::QuotaStatusCode ToMojom(
      blink::QuotaStatusCode status_code) {
    switch (status_code) {
      case blink::QuotaStatusCode::kOk:
        return blink::mojom::QuotaStatusCode::kOk;
      case blink::QuotaStatusCode::kErrorNotSupported:
        return blink::mojom::QuotaStatusCode::kErrorNotSupported;
      case blink::QuotaStatusCode::kErrorInvalidModification:
        return blink::mojom::QuotaStatusCode::kErrorInvalidModification;
      case blink::QuotaStatusCode::kErrorInvalidAccess:
        return blink::mojom::QuotaStatusCode::kErrorInvalidAccess;
      case blink::QuotaStatusCode::kErrorAbort:
        return blink::mojom::QuotaStatusCode::kErrorAbort;
      case blink::QuotaStatusCode::kUnknown:
        return blink::mojom::QuotaStatusCode::kUnknown;
    }
    NOTREACHED();
    return blink::mojom::QuotaStatusCode::kUnknown;
  }

  static bool FromMojom(blink::mojom::QuotaStatusCode status_code,
                        blink::QuotaStatusCode* out) {
    switch (status_code) {
      case blink::mojom::QuotaStatusCode::kOk:
        *out = blink::QuotaStatusCode::kOk;
        return true;
      case blink::mojom::QuotaStatusCode::kErrorNotSupported:
        *out = blink::QuotaStatusCode::kErrorNotSupported;
        return true;
      case blink::mojom::QuotaStatusCode::kErrorInvalidModification:
        *out = blink::QuotaStatusCode::kErrorInvalidModification;
        return true;
      case blink::mojom::QuotaStatusCode::kErrorInvalidAccess:
        *out = blink::QuotaStatusCode::kErrorInvalidAccess;
        return true;
      case blink::mojom::QuotaStatusCode::kErrorAbort:
        *out = blink::QuotaStatusCode::kErrorAbort;
        return true;
      case blink::mojom::QuotaStatusCode::kUnknown:
        *out = blink::QuotaStatusCode::kUnknown;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_WEBKIT_COMMON_QUOTA_QUOTA_TYPES_STRUCT_TRAITS_H_
