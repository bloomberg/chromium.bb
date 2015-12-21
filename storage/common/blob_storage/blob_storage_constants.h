// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_
#define STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

namespace storage {

// TODO(michaeln): use base::SysInfo::AmountOfPhysicalMemoryMB() in some
// way to come up with a better limit.
const int64_t kBlobStorageMaxMemoryUsage = 500 * 1024 * 1024;  // Half a gig.
const size_t kBlobStorageIPCThresholdBytes = 250 * 1024;
const size_t kBlobStorageMaxSharedMemoryBytes = 10 * 1024 * 1024;
const uint64_t kBlobStorageMaxFileSizeBytes = 100 * 1024 * 1024;
const uint64_t kBlobStorageMinFileSizeBytes = 1 * 1024 * 1024;
const size_t kBlobStorageMaxBlobMemorySize =
    kBlobStorageMaxMemoryUsage - kBlobStorageMinFileSizeBytes;

enum class IPCBlobItemRequestStrategy {
  UNKNOWN = 0,
  IPC,
  SHARED_MEMORY,
  FILE,
  LAST = FILE
};

enum class IPCBlobCreationCancelCode {
  UNKNOWN = 0,
  OUT_OF_MEMORY,
  FILE_WRITE_FAILED,
  LAST = FILE_WRITE_FAILED
};

}  // namespace storage

#endif  // STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_
