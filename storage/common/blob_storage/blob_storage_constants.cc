// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/blob_storage/blob_storage_constants.h"

#include "base/logging.h"

namespace storage {

static_assert(kDefaultIPCMemorySize < kDefaultSharedMemorySize,
              "IPC transport size must be smaller than shared memory size.");
static_assert(kDefaultMinPageFileSize < kDefaultMaxPageFileSize,
              "Min page file size must be less than max.");
static_assert(kDefaultMinPageFileSize < kDefaultMaxBlobInMemorySpace,
              "Page file size must be less than in-memory space.");

bool BlobStorageLimits::IsValid() const {
  return max_ipc_memory_size < max_shared_memory_size &&
         min_page_file_size < max_file_size &&
         min_page_file_size < max_blob_in_memory_space &&
         effective_max_disk_space <= desired_max_disk_space;
}

bool BlobStatusIsError(BlobStatus status) {
  return static_cast<int>(status) <= static_cast<int>(BlobStatus::LAST_ERROR);
}

bool BlobStatusIsPending(BlobStatus status) {
  int status_int = static_cast<int>(status);
  return status_int >= static_cast<int>(BlobStatus::PENDING_QUOTA) &&
         status_int <= static_cast<int>(BlobStatus::PENDING_INTERNALS);
}

bool BlobStatusIsBadIPC(BlobStatus status) {
  return status == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
}

}  // namespace storage
