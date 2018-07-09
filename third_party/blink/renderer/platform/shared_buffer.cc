/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/shared_buffer.h"

#include <memory>

#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/skia/include/core/SkData.h"

namespace blink {

constexpr unsigned SharedBuffer::kSegmentSize;

static inline size_t SegmentIndex(size_t position) {
  return position / SharedBuffer::kSegmentSize;
}

static inline size_t OffsetInSegment(size_t position) {
  return position % SharedBuffer::kSegmentSize;
}

class SharedBuffer::Segment final {
 public:
  Segment() {
    ptr_.reset(static_cast<char*>(WTF::Partitions::FastMalloc(
        SharedBuffer::kSegmentSize, "blink::SharedBuffer")));
  }
  ~Segment() = default;
  Segment(Segment&&) = default;

  char* get() { return ptr_.get(); }
  const char* get() const { return ptr_.get(); }

 private:
  struct Deleter {
    void operator()(char* p) const { WTF::Partitions::FastFree(p); }
  };
  std::unique_ptr<char[], Deleter> ptr_;
};

SharedBuffer::SharedBuffer() : size_(0) {}

SharedBuffer::SharedBuffer(size_t size) : size_(size), buffer_(size) {}

SharedBuffer::SharedBuffer(const char* data, size_t size) : size_(size) {
  buffer_.Append(data, size);
}

SharedBuffer::SharedBuffer(const unsigned char* data, size_t size)
    : SharedBuffer(reinterpret_cast<const char*>(data), size) {}

SharedBuffer::~SharedBuffer() = default;

scoped_refptr<SharedBuffer> SharedBuffer::AdoptVector(Vector<char>& vector) {
  scoped_refptr<SharedBuffer> buffer = Create();
  buffer->buffer_.swap(vector);
  buffer->size_ = buffer->buffer_.size();
  return buffer;
}

const char* SharedBuffer::Data() {
  MergeSegmentsIntoBuffer();
  return buffer_.data();
}

void SharedBuffer::Append(const SharedBuffer& data) {
  const char* segment;
  size_t position = 0;
  while (size_t length = data.GetSomeDataInternal(segment, position)) {
    Append(segment, length);
    position += length;
  }
}

void SharedBuffer::AppendInternal(const char* data, size_t length) {
  if (!length)
    return;

  DCHECK_GE(size_, buffer_.size());
  size_t position_in_segment = OffsetInSegment(size_ - buffer_.size());
  size_ += length;

  if (size_ <= kSegmentSize) {
    // No need to use segments for small resource data.
    buffer_.Append(data, length);
    return;
  }

  while (length > 0) {
    if (!position_in_segment)
      segments_.push_back(Segment());

    size_t bytes_to_copy = std::min(length, kSegmentSize - position_in_segment);
    memcpy(segments_.back().get() + position_in_segment, data, bytes_to_copy);

    data += bytes_to_copy;
    length -= bytes_to_copy;
    position_in_segment = 0;
  }
}

void SharedBuffer::Clear() {
  segments_.clear();
  size_ = 0;
  buffer_.clear();
}

void SharedBuffer::MergeSegmentsIntoBuffer() {
  size_t bytes_left = size_ - buffer_.size();
  for (const auto& segment : segments_) {
    size_t bytes_to_copy = std::min<size_t>(bytes_left, kSegmentSize);
    buffer_.Append(segment.get(), bytes_to_copy);
    bytes_left -= bytes_to_copy;
  }
  segments_.clear();
}

size_t SharedBuffer::GetSomeDataInternal(const char*& some_data,
                                         size_t position) const {
  if (position >= size_) {
    some_data = nullptr;
    return 0;
  }

  DCHECK(position < size_);
  size_t consecutive_size = buffer_.size();
  if (position < consecutive_size) {
    some_data = buffer_.data() + position;
    return consecutive_size - position;
  }

  position -= consecutive_size;
  size_t index = SegmentIndex(position);
  size_t bytes_left = size_ - consecutive_size;
  size_t position_in_segment = OffsetInSegment(position);

  DCHECK_LT(index, segments_.size());
  some_data = segments_[index].get() + position_in_segment;
  return index == segments_.size() - 1 ? bytes_left - position
                                       : kSegmentSize - position_in_segment;
}

bool SharedBuffer::GetBytesInternal(void* dest, size_t byte_length) const {
  if (!dest)
    return false;

  const char* segment = nullptr;
  size_t load_position = 0;
  size_t write_position = 0;
  while (byte_length > 0) {
    size_t load_size = GetSomeDataInternal(segment, load_position);
    if (load_size == 0)
      break;

    if (byte_length < load_size)
      load_size = byte_length;
    memcpy(static_cast<char*>(dest) + write_position, segment, load_size);
    load_position += load_size;
    write_position += load_size;
    byte_length -= load_size;
  }

  return byte_length == 0;
}

sk_sp<SkData> SharedBuffer::GetAsSkData() const {
  sk_sp<SkData> data = SkData::MakeUninitialized(size());
  char* buffer = static_cast<char*>(data->writable_data());
  const char* segment = nullptr;
  size_t position = 0;
  while (size_t segment_size = GetSomeDataInternal(segment, position)) {
    memcpy(buffer + position, segment, segment_size);
    position += segment_size;
  }

  DCHECK_EQ(position, size());
  return data;
}

void SharedBuffer::OnMemoryDump(const String& dump_prefix,
                                WebProcessMemoryDump* memory_dump) const {
  if (buffer_.size()) {
    WebMemoryAllocatorDump* dump =
        memory_dump->CreateMemoryAllocatorDump(dump_prefix + "/shared_buffer");
    dump->AddScalar("size", "bytes", buffer_.size());
    memory_dump->AddSuballocation(
        dump->Guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
  } else {
    // If there is data in the segments, then it should have been allocated
    // using fastMalloc.
    const String data_dump_name = dump_prefix + "/segments";
    auto* dump = memory_dump->CreateMemoryAllocatorDump(data_dump_name);
    dump->AddScalar("size", "bytes", size_);
    memory_dump->AddSuballocation(
        dump->Guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
  }
}

SharedBuffer::DeprecatedFlatData::DeprecatedFlatData(
    scoped_refptr<const SharedBuffer> buffer)
    : buffer_(std::move(buffer)) {
  DCHECK(buffer_);

  if (buffer_->size() <= buffer_->buffer_.size()) {
    // The SharedBuffer is not segmented - just point to its data.
    data_ = buffer_->buffer_.data();
    return;
  }

  // Merge all segments.
  flat_buffer_.ReserveInitialCapacity(buffer_->size());
  buffer_->ForEachSegment([this](const char* segment, size_t segment_size,
                                 size_t segment_offset) -> bool {
    flat_buffer_.Append(segment, segment_size);
    return true;
  });

  data_ = flat_buffer_.data();
}

}  // namespace blink
