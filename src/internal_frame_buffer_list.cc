// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/internal_frame_buffer_list.h"

#include <cstdint>
#include <memory>
#include <new>
#include <utility>

namespace libgav1 {

extern "C" {

int GetInternalFrameBuffer(void* private_data, size_t y_plane_min_size,
                           size_t uv_plane_min_size,
                           FrameBuffer* frame_buffer) {
  auto* buffer_list = static_cast<InternalFrameBufferList*>(private_data);
  // buffer_list is a null pointer if the InternalFrameBufferList::Create()
  // call fails. For simplicity, we handle the unlikely failure of
  // InternalFrameBufferList::Create() here, rather than at the call sites.
  if (buffer_list == nullptr) return -1;
  return buffer_list->GetFrameBuffer(y_plane_min_size, uv_plane_min_size,
                                     frame_buffer);
}

int ReleaseInternalFrameBuffer(void* private_data, FrameBuffer* frame_buffer) {
  auto* buffer_list = static_cast<InternalFrameBufferList*>(private_data);
  return buffer_list->ReleaseFrameBuffer(frame_buffer);
}

}  // extern "C"

// static
std::unique_ptr<InternalFrameBufferList> InternalFrameBufferList::Create(
    size_t num_buffers) {
  std::unique_ptr<InternalFrameBufferList> buffer_list;
  std::unique_ptr<Buffer[]> buffers(new (std::nothrow) Buffer[num_buffers]);
  if (buffers != nullptr) {
    buffer_list.reset(new (std::nothrow) InternalFrameBufferList(
        std::move(buffers), num_buffers));
  }
  return buffer_list;
}

InternalFrameBufferList::InternalFrameBufferList(
    std::unique_ptr<Buffer[]> buffers, size_t num_buffers)
    : buffers_(std::move(buffers)), num_buffers_(num_buffers) {}

int InternalFrameBufferList::GetFrameBuffer(size_t y_plane_min_size,
                                            size_t uv_plane_min_size,
                                            FrameBuffer* frame_buffer) {
  if (uv_plane_min_size > SIZE_MAX / 2 ||
      y_plane_min_size > SIZE_MAX - 2 * uv_plane_min_size) {
    return -1;
  }
  const size_t min_size = y_plane_min_size + 2 * uv_plane_min_size;

  size_t i;
  for (i = 0; i < num_buffers_; ++i) {
    if (!buffers_[i].in_use) break;
  }
  if (i == num_buffers_) return -1;

  if (buffers_[i].size < min_size) {
    std::unique_ptr<uint8_t[], MallocDeleter> new_data(
        static_cast<uint8_t*>(malloc(min_size)));
    if (new_data == nullptr) return -1;
    buffers_[i].data = std::move(new_data);
    buffers_[i].size = min_size;
  }

  frame_buffer->data[0] = buffers_[i].data.get();
  frame_buffer->size[0] = y_plane_min_size;
  frame_buffer->data[1] = frame_buffer->data[0] + y_plane_min_size;
  frame_buffer->size[1] = uv_plane_min_size;
  frame_buffer->data[2] = frame_buffer->data[1] + uv_plane_min_size;
  frame_buffer->size[2] = uv_plane_min_size;
  frame_buffer->private_data = &buffers_[i];
  buffers_[i].in_use = true;
  return 0;
}

int InternalFrameBufferList::ReleaseFrameBuffer(FrameBuffer* frame_buffer) {
  auto* const buffer = static_cast<Buffer*>(frame_buffer->private_data);
  buffer->in_use = false;
  return 0;
}

}  // namespace libgav1
