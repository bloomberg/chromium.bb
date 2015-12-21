// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_ASYNC_BUILDER_HOST_H_
#define STORAGE_BROWSER_BLOB_BLOB_ASYNC_BUILDER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory_handle.h"
#include "storage/browser/blob/blob_async_transport_strategy.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "storage/common/data_element.h"

namespace base {
class SharedMemory;
}

namespace storage {

// This class holds all blobs that are currently being built asynchronously for
// a child process. It sends memory request, cancel, and done messages through
// the given callbacks.
// This also includes handling 'shortcut' logic, where the host will use the
// initial data in the description instead of requesting for data if we have
// enough immediate space.
class STORAGE_EXPORT BlobAsyncBuilderHost {
 public:
  BlobAsyncBuilderHost();
  virtual ~BlobAsyncBuilderHost();

  // This method begins the construction of the blob given the descriptions.
  // After this method is called, either of the following can happen.
  // * The |done| callback is triggered immediately because we can shortcut the
  //   construction.
  // * The |request_memory| callback is called to request memory from the
  //   renderer. This class waits for calls to OnMemoryResponses to continue.
  //   The last argument in the callback corresponds to file handle sizes.
  //   When all memory is recieved, the |done| callback is triggered.
  // * The |cancel| callback is triggered if there is an error at any point. All
  //   state for the cancelled blob is cleared before |cancel| is called.
  // Returns if the arguments are valid and we have a good IPC message.
  bool StartBuildingBlob(
      const std::string& uuid,
      const std::string& type,
      const std::vector<DataElement>& descriptions,
      size_t memory_available,
      const base::Callback<
          void(const std::vector<storage::BlobItemBytesRequest>&,
               const std::vector<base::SharedMemoryHandle>&,
               const std::vector<uint64_t>&)>& request_memory,
      const base::Callback<void(const BlobDataBuilder&)>& done,
      const base::Callback<void(IPCBlobCreationCancelCode)>& cancel);

  // This is called when we have responses from the Renderer to our calls to
  // the request_memory callback above.
  // Returns if the arguments are valid and we have a good IPC message.
  bool OnMemoryResponses(const std::string& uuid,
                         const std::vector<BlobItemBytesResponse>& responses);

  // This erases the blob building state.
  void StopBuildingBlob(const std::string& uuid);

  size_t blob_building_count() const { return async_blob_map_.size(); }

  // For testing use only.  Must be called before StartBuildingBlob.
  void SetMemoryConstantsForTesting(size_t max_ipc_memory_size,
                                    size_t max_shared_memory_size,
                                    uint64_t max_file_size) {
    max_ipc_memory_size_ = max_ipc_memory_size;
    max_shared_memory_size_ = max_shared_memory_size;
    max_file_size_ = max_file_size;
  }

 private:
  struct BlobBuildingState {
    BlobBuildingState();
    ~BlobBuildingState();

    std::string type;
    BlobAsyncTransportStrategy transport_strategy;
    size_t next_request;
    size_t num_fulfilled_requests;
    scoped_ptr<base::SharedMemory> shared_memory_block;
    // This is the number of requests that have been sent to populate the above
    // shared data. We won't ask for more data in shared memory until all
    // requests have been responded to.
    size_t num_shared_memory_requests;
    // Only relevant if num_shared_memory_requests is > 0
    size_t current_shared_memory_handle_index;

    base::Callback<void(const std::vector<storage::BlobItemBytesRequest>&,
                        const std::vector<base::SharedMemoryHandle>&,
                        const std::vector<uint64_t>&)> request_memory_callback;
    base::Callback<void(const BlobDataBuilder&)> done_callback;
    base::Callback<void(IPCBlobCreationCancelCode)> cancel_callback;
  };

  typedef std::map<std::string, scoped_ptr<BlobBuildingState>> AsyncBlobMap;

  // This is the 'main loop' of our memory requests to the renderer.
  void ContinueBlobMemoryRequests(const std::string& uuid);

  void CancelAndCleanup(const std::string& uuid,
                        IPCBlobCreationCancelCode code);
  void DoneAndCleanup(const std::string& uuid);

  AsyncBlobMap async_blob_map_;

  // Here for testing.
  size_t max_ipc_memory_size_ = kBlobStorageIPCThresholdBytes;
  size_t max_shared_memory_size_ = kBlobStorageMaxSharedMemoryBytes;
  uint64_t max_file_size_ = kBlobStorageMaxFileSizeBytes;

  DISALLOW_COPY_AND_ASSIGN(BlobAsyncBuilderHost);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_ASYNC_BUILDER_HOST_H_
