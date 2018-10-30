// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_HEAP_SCATTERED_STREAM_DELEGATE_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_HEAP_SCATTERED_STREAM_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_writer.h"

namespace tracing {

// This is used to supply Perfetto's protozero library with
// a temporary heap-backed storage, which can then later be memcpy'd
// into the shared memory buffer.
class COMPONENT_EXPORT(TRACING_CPP) HeapScatteredStreamWriterDelegate
    : public protozero::ScatteredStreamWriter::Delegate {
 public:
  class Chunk {
   public:
    explicit Chunk(size_t size);
    Chunk(Chunk&& chunk);
    ~Chunk();

    protozero::ContiguousMemoryRange GetTotalRange() const;
    protozero::ContiguousMemoryRange GetUsedRange() const;

    uint8_t* start() const { return buffer_.get(); }
    size_t size() const { return size_; }
    size_t unused_bytes() const { return unused_bytes_; }
    void set_unused_bytes(size_t unused_bytes) { unused_bytes_ = unused_bytes; }

   private:
    std::unique_ptr<uint8_t[]> buffer_;
    const size_t size_;
    size_t unused_bytes_;
  };

  explicit HeapScatteredStreamWriterDelegate(size_t initial_size_hint);
  ~HeapScatteredStreamWriterDelegate() override;

  const std::vector<Chunk>& chunks() const { return chunks_; }

  void set_writer(protozero::ScatteredStreamWriter* writer) {
    writer_ = writer;
  }

  size_t GetTotalSize();
  void AdjustUsedSizeOfCurrentChunk();

  // protozero::ScatteredStreamWriter::Delegate implementation.
  protozero::ContiguousMemoryRange GetNewBuffer() override;

 private:
  std::vector<Chunk> chunks_;
  size_t next_chunk_size_;
  protozero::ScatteredStreamWriter* writer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HeapScatteredStreamWriterDelegate);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_HEAP_SCATTERED_STREAM_DELEGATE_H_
