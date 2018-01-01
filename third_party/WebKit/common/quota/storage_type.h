// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_QUOTA_STORAGE_TYPE_H_
#define THIRD_PARTY_WEBKIT_COMMON_QUOTA_STORAGE_TYPE_H_

namespace blink {

// TODO(sashab): Remove this and use mojom::storage::StorageType instead.
enum class StorageType {
  kTemporary,
  kPersistent,
  kSyncable,
  kQuotaNotManaged,
  kUnknown,
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_COMMON_QUOTA_STORAGE_TYPE_H_
