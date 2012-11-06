// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_STATUS_CODE_H_
#define WEBKIT_QUOTA_QUOTA_STATUS_CODE_H_

#include "webkit/storage/webkit_storage_export.h"

namespace quota {

// The numbers should match with the error code defined in
// third_party/WebKit/Source/WebCore/dom/ExceptionCode.h.
enum QuotaStatusCode {
  kQuotaStatusOk = 0,
  kQuotaErrorNotSupported = 9,          // NOT_SUPPORTED_ERR
  kQuotaErrorInvalidModification = 13,  // INVALID_MODIFICATION_ERR
  kQuotaErrorInvalidAccess = 15,        // INVALID_ACCESS_ERR
  kQuotaErrorAbort = 20,                // ABORT_ERR
  kQuotaStatusUnknown = -1,
};

WEBKIT_STORAGE_EXPORT const char* QuotaStatusCodeToString(
    QuotaStatusCode status);

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_STATUS_CODE_H_
