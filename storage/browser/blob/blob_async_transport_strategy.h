// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_ASYNC_TRANSPORT_STRATEGY_H_
#define STORAGE_BROWSER_BLOB_BLOB_ASYNC_TRANSPORT_STRATEGY_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/data_element.h"

namespace storage {

// This class computes and stores the strategy for asynchronously transporting
// memory from the renderer to the browser. We take memory constraints of our
// system and the description of a blob, and figure out:
// 1) How to store the blob data in the browser process: in memory or on disk.
// 2) How to transport the data from the renderer: ipc payload, shared memory,
//    or file handles.
// We then generate data requests for that blob's memory and seed a
// BlobDataBuilder for storing that data.
//
// Note: This class does not compute requests by using the 'shortcut' method,
//       where the data is already present in the blob description, and will
//       always give the caller the full strategy for requesting all data from
//       the renderer.
class STORAGE_EXPORT BlobAsyncTransportStrategy {
 public:
  enum Error {
    ERROR_NONE = 0,
    ERROR_TOO_LARGE,  // This item can't fit in disk or memory
    ERROR_INVALID_PARAMS
  };

  struct RendererMemoryItemRequest {
    RendererMemoryItemRequest();
    // This is the index of the item in the builder on the browser side.
    size_t browser_item_index;
    // Note: For files this offset should always be 0, as the file offset in
    //       segmentation is handled by the handle_offset in the message.  This
    //       offset is used for populating a chunk when the data comes back to
    //       the browser.
    size_t browser_item_offset;
    BlobItemBytesRequest message;
    bool received;
  };

  BlobAsyncTransportStrategy();
  virtual ~BlobAsyncTransportStrategy();

  // This call does the computation to create the requests and builder for the
  // blob given the memory constraints and blob description. |memory_available|
  // is the total amount of memory we can offer for storing blobs.
  // This method can only be called once.
  void Initialize(size_t max_ipc_memory_size,
                  size_t max_shared_memory_size,
                  size_t max_file_size,
                  uint64_t disk_space_left,
                  size_t memory_available,
                  const std::string& uuid,
                  const std::vector<DataElement>& blob_item_infos);

  // The sizes of the handles being used (by handle index) in the async
  // operation. This is used for both files or shared memory, as their use is
  // mutually exclusive.
  const std::vector<size_t>& handle_sizes() const { return handle_sizes_; }

  // The requests for memory, segmented as described above, along with their
  // destination browser indexes and offsets.
  const std::vector<RendererMemoryItemRequest>& requests() const {
    return requests_;
  }

  // Marks the request at the given request number as recieved.
  void MarkRequestAsReceived(size_t request_num) {
    DCHECK_LT(request_num, requests_.size());
    requests_[request_num].received = true;
  }

  // A BlobDataBuilder which can be used to construct the Blob in the
  // BlobStorageContext object after:
  // * The bytes items from AppendFutureData are populated by
  //   PopulateFutureData.
  // * The temporary files from AppendFutureFile are populated by
  //   PopulateFutureFile.
  BlobDataBuilder* blob_builder() { return builder_.get(); }

  // The total bytes size of memory items in the blob.
  uint64_t total_bytes_size() const { return total_bytes_size_; }

  Error error() const { return error_; }

  static bool ShouldBeShortcut(const std::vector<DataElement>& items,
                               size_t memory_available);

 private:
  static void ComputeHandleSizes(uint64_t total_memory_size,
                                 size_t max_segment_size,
                                 std::vector<size_t>* segment_sizes);

  Error error_;

  // We use the same vector for shared memory handle sizes and file handle sizes
  // because we only use one for any strategy. The size of the handles is capped
  // by the |max_file_size| argument in Initialize, so we can just use size_t.
  std::vector<size_t> handle_sizes_;

  uint64_t total_bytes_size_;
  std::vector<RendererMemoryItemRequest> requests_;
  scoped_ptr<BlobDataBuilder> builder_;

  DISALLOW_COPY_AND_ASSIGN(BlobAsyncTransportStrategy);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_ASYNC_TRANSPORT_STRATEGY_H_
