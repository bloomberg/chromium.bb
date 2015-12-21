// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_async_builder_host.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/memory/shared_memory.h"

namespace storage {

using MemoryItemRequest = BlobAsyncTransportStrategy::RendererMemoryItemRequest;

BlobAsyncBuilderHost::BlobBuildingState::BlobBuildingState()
    : next_request(0),
      num_fulfilled_requests(0),
      num_shared_memory_requests(0),
      current_shared_memory_handle_index(0) {}

BlobAsyncBuilderHost::BlobBuildingState::~BlobBuildingState() {}

BlobAsyncBuilderHost::BlobAsyncBuilderHost() {}

BlobAsyncBuilderHost::~BlobAsyncBuilderHost() {}

bool BlobAsyncBuilderHost::StartBuildingBlob(
    const std::string& uuid,
    const std::string& type,
    const std::vector<DataElement>& descriptions,
    size_t memory_available,
    const base::Callback<void(const std::vector<storage::BlobItemBytesRequest>&,
                              const std::vector<base::SharedMemoryHandle>&,
                              const std::vector<uint64_t>&)>& request_memory,
    const base::Callback<void(const BlobDataBuilder&)>& done,
    const base::Callback<void(IPCBlobCreationCancelCode)>& cancel) {
  if (async_blob_map_.find(uuid) != async_blob_map_.end())
    return false;
  if (BlobAsyncTransportStrategy::ShouldBeShortcut(descriptions,
                                                   memory_available)) {
    // We have enough memory, and all the data is here, so we use the shortcut
    // method and populate the old way.
    BlobDataBuilder builder(uuid);
    builder.set_content_type(type);
    for (const DataElement& element : descriptions) {
      builder.AppendIPCDataElement(element);
    }
    done.Run(builder);
    return true;
  }

  scoped_ptr<BlobBuildingState> state(new BlobBuildingState());
  BlobBuildingState* state_ptr = state.get();
  async_blob_map_[uuid] = std::move(state);
  state_ptr->type = type;
  state_ptr->request_memory_callback = request_memory;
  state_ptr->done_callback = done;
  state_ptr->cancel_callback = cancel;

  // We are currently only operating in 'no disk' mode. This will change in
  // future patches to enable disk storage.
  // Since we don't have a disk yet, we put 0 for disk_space_left.
  state_ptr->transport_strategy.Initialize(
      max_ipc_memory_size_, max_shared_memory_size_, max_file_size_,
      0 /* disk_space_left */, memory_available, uuid, descriptions);

  switch (state_ptr->transport_strategy.error()) {
    case BlobAsyncTransportStrategy::ERROR_TOO_LARGE:
      // Cancel cleanly, we're out of memory.
      CancelAndCleanup(uuid, IPCBlobCreationCancelCode::OUT_OF_MEMORY);
      return true;
    case BlobAsyncTransportStrategy::ERROR_INVALID_PARAMS:
      // Bad IPC, so we ignore and clean up.
      VLOG(1) << "Error initializing transport strategy: "
              << state_ptr->transport_strategy.error();
      async_blob_map_.erase(async_blob_map_.find(uuid));
      return false;
    case BlobAsyncTransportStrategy::ERROR_NONE:
      ContinueBlobMemoryRequests(uuid);
      return true;
  }
  return false;
}

bool BlobAsyncBuilderHost::OnMemoryResponses(
    const std::string& uuid,
    const std::vector<BlobItemBytesResponse>& responses) {
  if (responses.empty()) {
    return false;
  }
  AsyncBlobMap::const_iterator state_it = async_blob_map_.find(uuid);
  if (state_it == async_blob_map_.end()) {
    // There's a possibility that we had a shared memory error, and there were
    // still responses in flight. So we don't fail here, we just ignore.
    DVLOG(1) << "Could not find blob " << uuid;
    return true;
  }
  BlobAsyncBuilderHost::BlobBuildingState* state = state_it->second.get();
  BlobAsyncTransportStrategy& strategy = state->transport_strategy;
  bool invalid_ipc = false;
  bool memory_error = false;
  const auto& requests = strategy.requests();
  for (const BlobItemBytesResponse& response : responses) {
    if (response.request_number >= requests.size()) {
      // Bad IPC, so we delete our record and ignore.
      DVLOG(1) << "Invalid request number " << response.request_number;
      async_blob_map_.erase(state_it);
      return false;
    }
    const MemoryItemRequest& request = requests[response.request_number];
    if (request.received) {
      // Bad IPC, so we delete our record.
      DVLOG(1) << "Already received response for that request.";
      async_blob_map_.erase(state_it);
      return false;
    }
    strategy.MarkRequestAsReceived(response.request_number);
    switch (request.message.transport_strategy) {
      case IPCBlobItemRequestStrategy::IPC:
        if (response.inline_data.size() < request.message.size) {
          DVLOG(1) << "Invalid data size " << response.inline_data.size()
                   << " vs requested size of " << request.message.size;
          invalid_ipc = true;
          break;
        }
        invalid_ipc = !strategy.blob_builder()->PopulateFutureData(
            request.browser_item_index, &response.inline_data[0],
            request.browser_item_offset, request.message.size);
        break;
      case IPCBlobItemRequestStrategy::SHARED_MEMORY:
        if (state->num_shared_memory_requests == 0) {
          DVLOG(1) << "Received too many responses for shared memory.";
          invalid_ipc = true;
          break;
        }
        state->num_shared_memory_requests--;
        if (!state->shared_memory_block->memory()) {
          // We just map the whole block, as we'll probably be accessing the
          // whole thing in this group of responses. Another option is to use
          // MapAt, remove the mapped boolean, and then exclude the
          // handle_offset below.
          size_t handle_size = strategy.handle_sizes().at(
              state->current_shared_memory_handle_index);
          if (!state->shared_memory_block->Map(handle_size)) {
            DVLOG(1) << "Unable to map memory to size " << handle_size;
            memory_error = true;
            break;
          }
        }

        invalid_ipc = !strategy.blob_builder()->PopulateFutureData(
            request.browser_item_index,
            static_cast<const char*>(state->shared_memory_block->memory()) +
                request.message.handle_offset,
            request.browser_item_offset, request.message.size);
        break;
      case IPCBlobItemRequestStrategy::FILE:
      case IPCBlobItemRequestStrategy::UNKNOWN:
        DVLOG(1) << "Not implemented.";
        invalid_ipc = true;
        break;
    }
    if (invalid_ipc) {
      // Bad IPC, so we delete our record and return false.
      async_blob_map_.erase(state_it);
      return false;
    }
    if (memory_error) {
      DVLOG(1) << "Shared memory error.";
      CancelAndCleanup(uuid, IPCBlobCreationCancelCode::OUT_OF_MEMORY);
      return true;
    }
    state->num_fulfilled_requests++;
  }
  ContinueBlobMemoryRequests(uuid);
  return true;
}

void BlobAsyncBuilderHost::StopBuildingBlob(const std::string& uuid) {
  async_blob_map_.erase(async_blob_map_.find(uuid));
}

void BlobAsyncBuilderHost::ContinueBlobMemoryRequests(const std::string& uuid) {
  AsyncBlobMap::const_iterator state_it = async_blob_map_.find(uuid);
  DCHECK(state_it != async_blob_map_.end());
  BlobAsyncBuilderHost::BlobBuildingState* state = state_it->second.get();

  const std::vector<MemoryItemRequest>& requests =
      state->transport_strategy.requests();
  BlobAsyncTransportStrategy& strategy = state->transport_strategy;
  size_t num_requests = requests.size();
  if (state->num_fulfilled_requests == num_requests) {
    DoneAndCleanup(uuid);
    return;
  }
  DCHECK_LT(state->num_fulfilled_requests, num_requests);
  if (state->next_request == num_requests) {
    // We are still waiting on other requests to come back.
    return;
  }

  std::vector<BlobItemBytesRequest> byte_requests;
  std::vector<base::SharedMemoryHandle> shared_memory;
  std::vector<uint64_t> files;

  for (; state->next_request < num_requests; ++state->next_request) {
    const MemoryItemRequest& request = requests[state->next_request];

    bool stop_accumulating = false;
    bool using_shared_memory_handle = state->num_shared_memory_requests > 0;
    switch (request.message.transport_strategy) {
      case IPCBlobItemRequestStrategy::IPC:
        byte_requests.push_back(request.message);
        break;
      case IPCBlobItemRequestStrategy::SHARED_MEMORY:
        if (using_shared_memory_handle &&
            state->current_shared_memory_handle_index !=
                request.message.handle_index) {
          // We only want one shared memory per requesting blob.
          stop_accumulating = true;
          break;
        }
        using_shared_memory_handle = true;
        state->current_shared_memory_handle_index =
            request.message.handle_index;
        state->num_shared_memory_requests++;

        if (!state->shared_memory_block) {
          state->shared_memory_block.reset(new base::SharedMemory());
          size_t size = strategy.handle_sizes()[request.message.handle_index];
          if (!state->shared_memory_block->CreateAnonymous(size)) {
            DVLOG(1) << "Unable to allocate shared memory for blob transfer.";
            CancelAndCleanup(uuid, IPCBlobCreationCancelCode::OUT_OF_MEMORY);
            return;
          }
        }
        shared_memory.push_back(state->shared_memory_block->handle());
        byte_requests.push_back(request.message);
        // Since we are only using one handle at a time, transform our handle
        // index correctly back to 0.
        byte_requests.back().handle_index = 0;
        break;
      case IPCBlobItemRequestStrategy::FILE:
      case IPCBlobItemRequestStrategy::UNKNOWN:
        NOTREACHED() << "Not implemented yet.";
        break;
    }
    if (stop_accumulating) {
      break;
    }
  }

  DCHECK(!requests.empty());

  state->request_memory_callback.Run(byte_requests, shared_memory, files);
}

void BlobAsyncBuilderHost::CancelAndCleanup(const std::string& uuid,
                                            IPCBlobCreationCancelCode code) {
  scoped_ptr<BlobBuildingState> state = std::move(async_blob_map_[uuid]);
  async_blob_map_.erase(uuid);
  state->cancel_callback.Run(code);
}

void BlobAsyncBuilderHost::DoneAndCleanup(const std::string& uuid) {
  scoped_ptr<BlobBuildingState> state = std::move(async_blob_map_[uuid]);
  async_blob_map_.erase(uuid);
  BlobDataBuilder* builder = state->transport_strategy.blob_builder();
  builder->set_content_type(state->type);
  state->done_callback.Run(*builder);
}

}  // namespace storage
