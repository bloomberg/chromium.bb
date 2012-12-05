// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_STATUS_CODE_H_
#define WEBKIT_QUOTA_QUOTA_STATUS_CODE_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageQuotaError.h"
#include "webkit/storage/webkit_storage_export.h"

namespace quota {

enum QuotaStatusCode {
  kQuotaStatusOk = 0,
  kQuotaErrorNotSupported = WebKit::WebStorageQuotaErrorNotSupported,
  kQuotaErrorInvalidModification =
      WebKit::WebStorageQuotaErrorInvalidModification,
  kQuotaErrorInvalidAccess = WebKit::WebStorageQuotaErrorInvalidAccess,
  kQuotaErrorAbort = WebKit::WebStorageQuotaErrorAbort,
  kQuotaStatusUnknown = -1,
};

WEBKIT_STORAGE_EXPORT const char* QuotaStatusCodeToString(
    QuotaStatusCode status);

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_STATUS_CODE_H_
