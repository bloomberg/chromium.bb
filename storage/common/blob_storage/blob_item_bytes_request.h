// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_BLOB_STORAGE_BLOB_ITEM_BYTES_REQUEST_H_
#define STORAGE_COMMON_BLOB_STORAGE_BLOB_ITEM_BYTES_REQUEST_H_

#include <stdint.h>
#include <ostream>

#include "base/basictypes.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "storage/common/storage_common_export.h"

namespace storage {

// This class is serialized over IPC to request bytes from a blob item.
struct STORAGE_COMMON_EXPORT BlobItemBytesRequest {
  // Not using std::numeric_limits<T>::max() because of non-C++11 builds.
  static const size_t kInvalidIndex = SIZE_MAX;
  static const uint64_t kInvalidSize = kuint64max;

  static BlobItemBytesRequest CreateIPCRequest(size_t request_number,
                                               size_t renderer_item_index,
                                               size_t renderer_item_offset,
                                               size_t size);

  static BlobItemBytesRequest CreateSharedMemoryRequest(
      size_t request_number,
      size_t renderer_item_index,
      size_t renderer_item_offset,
      size_t size,
      size_t handle_index,
      uint64_t handle_offset);

  static BlobItemBytesRequest CreateFileRequest(size_t request_number,
                                                size_t renderer_item_index,
                                                uint64_t renderer_item_offset,
                                                uint64_t size,
                                                size_t handle_index,
                                                uint64_t handle_offset);

  BlobItemBytesRequest();
  BlobItemBytesRequest(size_t request_number,
                       IPCBlobItemRequestStrategy transport_strategy,
                       size_t renderer_item_index,
                       uint64_t renderer_item_offset,
                       uint64_t size,
                       size_t handle_index,
                       uint64_t handle_offset);
  ~BlobItemBytesRequest();

  // The request number uniquely identifies the memory request. We can't use
  // the renderer item index or browser item index as there can be multiple
  // requests for both (as segmentation boundaries can exist in both).
  size_t request_number;
  IPCBlobItemRequestStrategy transport_strategy;
  size_t renderer_item_index;
  uint64_t renderer_item_offset;
  uint64_t size;
  size_t handle_index;
  uint64_t handle_offset;
};

STORAGE_COMMON_EXPORT void PrintTo(const BlobItemBytesRequest& request,
                                   std::ostream* os);

#if defined(UNIT_TEST)
STORAGE_COMMON_EXPORT inline bool operator==(const BlobItemBytesRequest& a,
                                             const BlobItemBytesRequest& b) {
  return a.request_number == b.request_number &&
         a.transport_strategy == b.transport_strategy &&
         a.renderer_item_index == b.renderer_item_index &&
         a.renderer_item_offset == b.renderer_item_offset && a.size == b.size &&
         a.handle_index == b.handle_index && a.handle_offset == b.handle_offset;
}

STORAGE_COMMON_EXPORT inline bool operator!=(const BlobItemBytesRequest& a,
                                             const BlobItemBytesRequest& b) {
  return !(a == b);
}
#endif  // defined(UNIT_TEST)

}  // namespace storage

#endif  // STORAGE_COMMON_BLOB_STORAGE_BLOB_ITEM_BYTES_REQUEST_H_
