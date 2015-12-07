// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_UTIL_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_UTIL_H_

#include "base/memory/ref_counted.h"
#include "sync/base/sync_export.h"

namespace base {
class RefCountedMemory;
}

namespace syncer {

// Return the crc32c of the memory described by |data|.
//
// Potentially expensive.
// Ideally this function should be static function in Attachment class, but
// include_rules from sync/api/DEPS don't allow direct dependency on
// third_party.
SYNC_EXPORT_PRIVATE uint32_t
ComputeCrc32c(const scoped_refptr<base::RefCountedMemory>& data);

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_UTIL_H_
