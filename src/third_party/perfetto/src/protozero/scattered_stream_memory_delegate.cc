/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/protozero/scattered_stream_memory_delegate.h"

namespace perfetto {

ScatteredStreamMemoryDelegate::ScatteredStreamMemoryDelegate(size_t chunk_size)
    : chunk_size_(chunk_size) {}

ScatteredStreamMemoryDelegate::~ScatteredStreamMemoryDelegate() {}

protozero::ContiguousMemoryRange ScatteredStreamMemoryDelegate::GetNewBuffer() {
  PERFETTO_CHECK(writer_);
  if (!chunks_.empty()) {
    size_t used = chunk_size_ - writer_->bytes_available();
    chunks_used_size_.push_back(used);
  }
  std::unique_ptr<uint8_t[]> chunk(new uint8_t[chunk_size_]);
  uint8_t* begin = chunk.get();
  memset(begin, 0xff, chunk_size_);
  chunks_.push_back(std::move(chunk));
  return {begin, begin + chunk_size_};
}

std::vector<uint8_t> ScatteredStreamMemoryDelegate::StitchChunks() {
  std::vector<uint8_t> buffer;
  size_t i = 0;
  for (const auto& chunk : chunks_) {
    size_t chunk_size = (i < chunks_used_size_.size())
                            ? chunks_used_size_[i]
                            : (chunk_size_ - writer_->bytes_available());
    PERFETTO_CHECK(chunk_size <= chunk_size_);
    buffer.insert(buffer.end(), chunk.get(), chunk.get() + chunk_size);
    i++;
  }
  return buffer;
}

}  // namespace perfetto
