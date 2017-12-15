// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_QUOTA_QUOTA_STATUS_CODE_H_
#define THIRD_PARTY_WEBKIT_COMMON_QUOTA_QUOTA_STATUS_CODE_H_

namespace blink {

// These values are used by WebStorageQuotaError and need to match
// dom/ExceptionState.h.
// TODO(sashab): Remove this and use mojom::storage::QuotaStatusCode instead.
enum class QuotaStatusCode {
  kOk = 0,
  kErrorNotSupported = 7,
  kErrorInvalidModification = 11,
  kErrorInvalidAccess = 13,
  kErrorAbort = 17,
  kUnknown = -1,
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_COMMON_QUOTA_QUOTA_STATUS_CODE_H_
