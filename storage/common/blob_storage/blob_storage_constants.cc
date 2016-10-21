// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/blob_storage/blob_storage_constants.h"

#include "base/logging.h"

namespace storage {

bool BlobStatusIsError(BlobStatus status) {
  return static_cast<int>(status) <= static_cast<int>(BlobStatus::LAST_ERROR);
}

bool BlobStatusIsBadIPC(BlobStatus status) {
  return status == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
}

}  // namespace storage
