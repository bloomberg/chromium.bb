// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/numerics/safe_math.h"
#include "storage/browser/blob/blob_async_transport_strategy.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

namespace storage {
namespace {
bool IsBytes(DataElement::Type type) {
  return type == DataElement::TYPE_BYTES ||
         type == DataElement::TYPE_BYTES_DESCRIPTION;
}

// This is the general template that each strategy below implements. See the
// ForEachWithSegment method for a description of how these are called.
// class BlobSegmentVisitor {
//  public:
//   typedef ___ SizeType;
//   void VisitBytesSegment(size_t element_index, uint64_t element_offset,
//                          size_t segment_index, uint64_t segment_offset,
//                          uint64_t size);
//   void VisitNonBytesSegment(const DataElement& element, size_t element_idx);
//   void Done();
// };

// This class handles the logic of how transported memory is going to be
// represented as storage in the browser.  The main idea is that all the memory
// is now packed into file chunks, and the browser items will just reference
// the file with offsets and sizes.
class FileStorageStrategy {
 public:
  FileStorageStrategy(
      std::vector<BlobAsyncTransportStrategy::RendererMemoryItemRequest>*
          requests,
      BlobDataBuilder* builder)
      : requests(requests), builder(builder), current_item_index(0) {}

  ~FileStorageStrategy() {}

  void VisitBytesSegment(size_t element_index,
                         uint64_t element_offset,
                         size_t segment_index,
                         uint64_t segment_offset,
                         uint64_t size) {
    BlobAsyncTransportStrategy::RendererMemoryItemRequest request;
    request.browser_item_index = current_item_index;
    request.browser_item_offset = 0;
    request.message.request_number = requests->size();
    request.message.transport_strategy = IPCBlobItemRequestStrategy::FILE;
    request.message.renderer_item_index = element_index;
    request.message.renderer_item_offset = element_offset;
    request.message.size = size;
    request.message.handle_index = segment_index;
    request.message.handle_offset = segment_offset;

    requests->push_back(request);
    builder->AppendFutureFile(segment_offset, size);
    current_item_index++;
  }

  void VisitNonBytesSegment(const DataElement& element, size_t element_index) {
    builder->AppendIPCDataElement(element);
    current_item_index++;
  }

  void Done() {}

  std::vector<BlobAsyncTransportStrategy::RendererMemoryItemRequest>* requests;
  BlobDataBuilder* builder;

  size_t current_item_index;
};

// This class handles the logic of storing memory that is transported as
// consolidated shared memory.
class SharedMemoryStorageStrategy {
 public:
  SharedMemoryStorageStrategy(
      size_t max_segment_size,
      std::vector<BlobAsyncTransportStrategy::RendererMemoryItemRequest>*
          requests,
      BlobDataBuilder* builder)
      : requests(requests),
        max_segment_size(max_segment_size),
        builder(builder),
        current_item_size(0),
        current_item_index(0) {}
  ~SharedMemoryStorageStrategy() {}

  void VisitBytesSegment(size_t element_index,
                         uint64_t element_offset,
                         size_t segment_index,
                         uint64_t segment_offset,
                         uint64_t size) {
    if (current_item_size + size > max_segment_size) {
      builder->AppendFutureData(current_item_size);
      current_item_index++;
      current_item_size = 0;
    }
    BlobAsyncTransportStrategy::RendererMemoryItemRequest request;
    request.browser_item_index = current_item_index;
    request.browser_item_offset = current_item_size;
    request.message.request_number = requests->size();
    request.message.transport_strategy =
        IPCBlobItemRequestStrategy::SHARED_MEMORY;
    request.message.renderer_item_index = element_index;
    request.message.renderer_item_offset = element_offset;
    request.message.size = size;
    request.message.handle_index = segment_index;
    request.message.handle_offset = segment_offset;

    requests->push_back(request);
    current_item_size += size;
  }

  void VisitNonBytesSegment(const DataElement& element, size_t element_index) {
    if (current_item_size != 0) {
      builder->AppendFutureData(current_item_size);
      current_item_index++;
    }
    builder->AppendIPCDataElement(element);
    current_item_index++;
    current_item_size = 0;
  }

  void Done() {
    if (current_item_size != 0) {
      builder->AppendFutureData(current_item_size);
    }
  }

  std::vector<BlobAsyncTransportStrategy::RendererMemoryItemRequest>* requests;

  size_t max_segment_size;
  BlobDataBuilder* builder;
  size_t current_item_size;
  uint64_t current_item_index;
};

// This iterates of the data elements and segments the 'bytes' data into
// the smallest number of segments given the max_segment_size.
// The callback describes either:
// * A non-memory item
// * A partition of a bytes element which will be populated into a given
//   segment and segment offset.
// More specifically, we split each |element| into one or more |segments| of a
// max_size, invokes the strategy to determine the request to make for each
// |segment| produced. A |segment| can also span multiple |elements|.
// Assumptions: All memory items are consolidated.  As in, there are no two
//              'bytes' items next to eachother.
template <typename Visitor>
void ForEachWithSegment(const std::vector<DataElement>& elements,
                        uint64_t max_segment_size,
                        Visitor* visitor) {
  DCHECK_GT(max_segment_size, 0ull);
  size_t segment_index = 0;
  uint64_t segment_offset = 0;
  size_t elements_length = elements.size();
  for (size_t element_index = 0; element_index < elements_length;
       ++element_index) {
    const auto& element = elements.at(element_index);
    DataElement::Type type = element.type();
    if (!IsBytes(type)) {
      visitor->VisitNonBytesSegment(element, element_index);
      continue;
    }
    uint64_t element_memory_left = element.length();
    uint64_t element_offset = 0;
    while (element_memory_left > 0) {
      if (segment_offset == max_segment_size) {
        ++segment_index;
        segment_offset = 0;
      }
      uint64_t memory_writing =
          std::min(max_segment_size - segment_offset, element_memory_left);
      visitor->VisitBytesSegment(element_index, element_offset, segment_index,
                                 segment_offset, memory_writing);
      element_memory_left -= memory_writing;
      segment_offset += memory_writing;
      element_offset += memory_writing;
    }
  }
  visitor->Done();
}
}  // namespace

BlobAsyncTransportStrategy::RendererMemoryItemRequest::
    RendererMemoryItemRequest()
    : browser_item_index(0), browser_item_offset(0), received(false) {}

BlobAsyncTransportStrategy::BlobAsyncTransportStrategy()
    : error_(BlobAsyncTransportStrategy::ERROR_NONE), total_bytes_size_(0) {}

BlobAsyncTransportStrategy::~BlobAsyncTransportStrategy() {}

// if total_blob_size > |memory_available| (say 400MB)
//   Request all data in files
//   (Segment all of the existing data into
//   file blocks, of <= |max_file_size|)
// else if total_blob_size > |max_ipc_memory_size| (say 150KB)
//   Request all data in shared memory
//   (Segment all of the existing data into
//   shared memory blocks, of <= |max_shared_memory_size|)
// else
//   Request all data to be sent over IPC
void BlobAsyncTransportStrategy::Initialize(
    size_t max_ipc_memory_size,
    size_t max_shared_memory_size,
    size_t max_file_size,
    uint64_t disk_space_left,
    size_t memory_available,
    const std::string& uuid,
    const std::vector<DataElement>& blob_item_infos) {
  DCHECK(handle_sizes_.empty());
  DCHECK(requests_.empty());
  DCHECK(!builder_.get());
  builder_.reset(new BlobDataBuilder(uuid));
  error_ = BlobAsyncTransportStrategy::ERROR_NONE;

  size_t memory_items = 0;
  base::CheckedNumeric<uint64_t> total_size_checked = 0;
  for (const auto& info : blob_item_infos) {
    if (!IsBytes(info.type())) {
      continue;
    }
    total_size_checked += info.length();
    ++memory_items;
  }

  if (!total_size_checked.IsValid()) {
    DVLOG(1) << "Impossible total size of all memory elements.";
    error_ = BlobAsyncTransportStrategy::ERROR_INVALID_PARAMS;
    return;
  }

  total_bytes_size_ = total_size_checked.ValueOrDie();

  // See if we have enough memory.
  if (total_bytes_size_ >
      disk_space_left + static_cast<uint64_t>(memory_available)) {
    error_ = BlobAsyncTransportStrategy::ERROR_TOO_LARGE;
    return;
  }

  // If we're more than the available memory, then we're going straight to disk.
  if (total_bytes_size_ > memory_available) {
    if (total_bytes_size_ > disk_space_left) {
      error_ = BlobAsyncTransportStrategy::ERROR_TOO_LARGE;
      return;
    }
    ComputeHandleSizes(total_bytes_size_, max_file_size, &handle_sizes_);
    FileStorageStrategy strategy(&requests_, builder_.get());
    ForEachWithSegment(blob_item_infos, static_cast<uint64_t>(max_file_size),
                       &strategy);
    return;
  }

  if (total_bytes_size_ > max_ipc_memory_size) {
    // Note: The size must be <= std::numeric_limits<size_t>::max(). Otherwise
    // we are guarenteed to be caught by the if statement above,
    // |total_bytes_size_ > memory_available|.
    ComputeHandleSizes(total_bytes_size_, max_shared_memory_size,
                       &handle_sizes_);
    SharedMemoryStorageStrategy strategy(max_shared_memory_size, &requests_,
                                         builder_.get());
    ForEachWithSegment(blob_item_infos,
                       static_cast<uint64_t>(max_shared_memory_size),
                       &strategy);
    return;
  }

  // Since they can all fit in IPC memory, we don't need to segment anything,
  // and just request them straight in IPC.
  size_t items_length = blob_item_infos.size();
  for (size_t i = 0; i < items_length; i++) {
    const auto& info = blob_item_infos.at(i);
    if (!IsBytes(info.type())) {
      builder_->AppendIPCDataElement(info);
      continue;
    }
    BlobAsyncTransportStrategy::RendererMemoryItemRequest request;
    request.browser_item_index = i;
    request.browser_item_offset = 0;
    request.message.request_number = requests_.size();
    request.message.transport_strategy = IPCBlobItemRequestStrategy::IPC;
    request.message.renderer_item_index = i;
    request.message.renderer_item_offset = 0;
    request.message.size = info.length();
    requests_.push_back(request);
    builder_->AppendFutureData(info.length());
  }
}

/* static */
bool BlobAsyncTransportStrategy::ShouldBeShortcut(
    const std::vector<DataElement>& elements,
    size_t memory_available) {
  base::CheckedNumeric<size_t> shortcut_bytes = 0;
  for (const auto& element : elements) {
    DataElement::Type type = element.type();
    if (type == DataElement::TYPE_BYTES_DESCRIPTION) {
      return false;
    }
    if (type == DataElement::TYPE_BYTES) {
      shortcut_bytes += element.length();
      if (!shortcut_bytes.IsValid()) {
        return false;
      }
    }
  }
  return shortcut_bytes.ValueOrDie() <= memory_available;
}

/* static */
void BlobAsyncTransportStrategy::ComputeHandleSizes(
    uint64_t total_memory_size,
    size_t max_segment_size,
    std::vector<size_t>* segment_sizes) {
  size_t total_max_segments =
      static_cast<size_t>(total_memory_size / max_segment_size);
  bool has_extra_segment = (total_memory_size % max_segment_size) > 0;
  segment_sizes->reserve(total_max_segments + (has_extra_segment ? 1 : 0));
  segment_sizes->insert(segment_sizes->begin(), total_max_segments,
                        max_segment_size);
  if (has_extra_segment) {
    segment_sizes->push_back(total_memory_size % max_segment_size);
  }
}

}  // namespace storage
