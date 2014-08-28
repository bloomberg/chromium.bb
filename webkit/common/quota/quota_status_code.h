// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_QUOTA_QUOTA_STATUS_CODE_H_
#define WEBKIT_COMMON_QUOTA_QUOTA_STATUS_CODE_H_

#include "third_party/WebKit/public/platform/WebStorageQuotaError.h"
#include "webkit/common/storage_export.h"

namespace storage {

enum QuotaStatusCode {
  kQuotaStatusOk = 0,
  kQuotaErrorNotSupported = blink::WebStorageQuotaErrorNotSupported,
  kQuotaErrorInvalidModification =
      blink::WebStorageQuotaErrorInvalidModification,
  kQuotaErrorInvalidAccess = blink::WebStorageQuotaErrorInvalidAccess,
  kQuotaErrorAbort = blink::WebStorageQuotaErrorAbort,
  kQuotaStatusUnknown = -1,
};

STORAGE_EXPORT const char* QuotaStatusCodeToString(
    QuotaStatusCode status);

}  // namespace storage

#endif  // WEBKIT_COMMON_QUOTA_QUOTA_STATUS_CODE_H_
