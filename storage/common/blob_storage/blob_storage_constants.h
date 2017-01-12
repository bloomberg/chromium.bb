// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_
#define STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback_forward.h"
#include "storage/common/storage_common_export.h"

namespace storage {

constexpr size_t kDefaultIPCMemorySize = 250u * 1024;
constexpr size_t kDefaultSharedMemorySize = 10u * 1024 * 1024;
constexpr size_t kDefaultMaxBlobInMemorySpace = 500u * 1024 * 1024;
constexpr uint64_t kDefaultMaxBlobDiskSpace = 0ull;
constexpr uint64_t kDefaultMinPageFileSize = 5ull * 1024 * 1024;
constexpr uint64_t kDefaultMaxPageFileSize = 100ull * 1024 * 1024;

// All sizes are in bytes.
struct STORAGE_COMMON_EXPORT BlobStorageLimits {
  // Returns if the current configuration is valid.
  bool IsValid() const;

  size_t memory_limit_before_paging() const {
    return max_blob_in_memory_space - min_page_file_size;
  }

  // If disk space goes less than this we stop allocating more disk quota.
  uint64_t min_available_external_disk_space() const {
    return 2ull * memory_limit_before_paging();
  }

  bool IsDiskSpaceConstrained() const {
    return desired_max_disk_space != effective_max_disk_space;
  }

  // This is the maximum amount of memory we can send in an IPC.
  size_t max_ipc_memory_size = kDefaultIPCMemorySize;
  // This is the maximum size of a shared memory handle.
  size_t max_shared_memory_size = kDefaultSharedMemorySize;

  // This is the maximum amount of memory we can use to store blobs.
  size_t max_blob_in_memory_space = kDefaultMaxBlobInMemorySpace;

  // This is the maximum amount of disk space we can use.
  uint64_t desired_max_disk_space = kDefaultMaxBlobDiskSpace;
  // This value will change based on the amount of free space on the device.
  uint64_t effective_max_disk_space = kDefaultMaxBlobDiskSpace;

  // This is the minimum file size we can use when paging blob items to disk.
  // We combine items until we reach at least this size.
  uint64_t min_page_file_size = kDefaultMinPageFileSize;
  // This is the maximum file size we can create.
  uint64_t max_file_size = kDefaultMaxPageFileSize;
};

enum class IPCBlobItemRequestStrategy {
  UNKNOWN = 0,
  IPC,
  SHARED_MEMORY,
  FILE,
  LAST = FILE
};

// This is the enum to rule them all in the blob system.
// These values are used in UMA metrics, so they should not be changed. Please
// update LAST_ERROR if you add an error condition and LAST if you add new
// state.
enum class BlobStatus {
  // Error case section:
  // The construction arguments are invalid. This is considered a bad ipc.
  ERR_INVALID_CONSTRUCTION_ARGUMENTS = 0,
  // We don't have enough memory for the blob.
  ERR_OUT_OF_MEMORY = 1,
  // We couldn't create or write to a file. File system error, like a full disk.
  ERR_FILE_WRITE_FAILED = 2,
  // The renderer was destroyed while data was in transit.
  ERR_SOURCE_DIED_IN_TRANSIT = 3,
  // The renderer destructed the blob before it was done transferring, and there
  // were no outstanding references (no one is waiting to read) to keep the
  // blob alive.
  ERR_BLOB_DEREFERENCED_WHILE_BUILDING = 4,
  // A blob that we referenced during construction is broken, or a browser-side
  // builder tries to build a blob with a blob reference that isn't finished
  // constructing.
  ERR_REFERENCED_BLOB_BROKEN = 5,
  LAST_ERROR = ERR_REFERENCED_BLOB_BROKEN,

  // Blob state section:
  // The blob has finished.
  DONE = 200,
  // Waiting for memory or file quota for the to-be transported data.
  PENDING_QUOTA = 201,
  // Waiting for data to be transported (quota has been granted).
  PENDING_TRANSPORT = 202,
  // Waiting for any operations involving dependent blobs after transport data
  // has been populated. See BlobEntry::BuildingState for more info.
  // TODO(dmurph): Change to PENDING_REFERENCED_BLOBS (crbug.com/670398).
  PENDING_INTERNALS = 203,
  LAST = PENDING_INTERNALS
};

using BlobStatusCallback = base::Callback<void(BlobStatus)>;

// Returns if the status is an error code.
STORAGE_COMMON_EXPORT bool BlobStatusIsError(BlobStatus status);

STORAGE_COMMON_EXPORT bool BlobStatusIsPending(BlobStatus status);

// Returns if the status is a bad enough error to flag the IPC as bad. This is
// only INVALID_CONSTRUCTION_ARGUMENTS.
STORAGE_COMMON_EXPORT bool BlobStatusIsBadIPC(BlobStatus status);

}  // namespace storage

#endif  // STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_
