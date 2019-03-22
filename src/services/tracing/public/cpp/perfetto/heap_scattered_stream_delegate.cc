// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/heap_scattered_stream_delegate.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"

namespace tracing {

namespace {

// TODO(oysteine): Tune these values.
static const size_t kDefaultChunkSizeBytes = 128;
static const size_t kMaximumChunkSizeBytes = 128 * 1024;

}  // namespace

HeapScatteredStreamWriterDelegate::Chunk::Chunk(size_t size)
    : buffer_(std::unique_ptr<uint8_t[]>(new uint8_t[size])),
      size_(size),
      unused_bytes_(size) {
  DCHECK(size);
}

HeapScatteredStreamWriterDelegate::Chunk::Chunk(Chunk&& chunk)
    : buffer_(std::move(chunk.buffer_)),
      size_(std::move(chunk.size_)),
      unused_bytes_(std::move(chunk.unused_bytes_)) {}

HeapScatteredStreamWriterDelegate::Chunk::~Chunk() = default;

protozero::ContiguousMemoryRange
HeapScatteredStreamWriterDelegate::Chunk::GetTotalRange() const {
  protozero::ContiguousMemoryRange range = {buffer_.get(),
                                            buffer_.get() + size_};
  return range;
}

protozero::ContiguousMemoryRange
HeapScatteredStreamWriterDelegate::Chunk::GetUsedRange() const {
  protozero::ContiguousMemoryRange range = {
      buffer_.get(), buffer_.get() + size_ - unused_bytes_};
  return range;
}

HeapScatteredStreamWriterDelegate::HeapScatteredStreamWriterDelegate(
    size_t initial_size_hint) {
  next_chunk_size_ =
      initial_size_hint ? initial_size_hint : kDefaultChunkSizeBytes;
}

HeapScatteredStreamWriterDelegate::~HeapScatteredStreamWriterDelegate() =
    default;

size_t HeapScatteredStreamWriterDelegate::GetTotalSize() {
  size_t total_size = 0;
  for (auto& chunk : chunks_) {
    total_size += chunk.size();
  }
  return total_size;
}

void HeapScatteredStreamWriterDelegate::AdjustUsedSizeOfCurrentChunk() {
  if (!chunks_.empty()) {
    DCHECK(writer_);
    chunks_.back().set_unused_bytes(writer_->bytes_available());
  }
}

protozero::ContiguousMemoryRange
HeapScatteredStreamWriterDelegate::GetNewBuffer() {
  AdjustUsedSizeOfCurrentChunk();

  chunks_.emplace_back(next_chunk_size_);
  next_chunk_size_ = std::min(kMaximumChunkSizeBytes, next_chunk_size_ * 2);
  return chunks_.back().GetTotalRange();
}

}  // namespace tracing
