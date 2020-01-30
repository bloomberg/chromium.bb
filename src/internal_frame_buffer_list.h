/*
 * Copyright 2019 The libgav1 Authors
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

#ifndef LIBGAV1_SRC_INTERNAL_FRAME_BUFFER_LIST_H_
#define LIBGAV1_SRC_INTERNAL_FRAME_BUFFER_LIST_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "src/gav1/frame_buffer.h"
#include "src/gav1/frame_buffer2.h"
#include "src/utils/memory.h"

namespace libgav1 {

extern "C" int OnInternalFrameBufferSizeChanged(
    void* callback_private_data, int bitdepth, Libgav1ImageFormat image_format,
    int width, int height, int left_border, int right_border, int top_border,
    int bottom_border, int stride_alignment);

extern "C" int GetInternalFrameBuffer(void* callback_private_data, int bitdepth,
                                      Libgav1ImageFormat image_format,
                                      int width, int height, int left_border,
                                      int right_border, int top_border,
                                      int bottom_border, int stride_alignment,
                                      Libgav1FrameBuffer2* frame_buffer);

extern "C" void ReleaseInternalFrameBuffer(void* callback_private_data,
                                           void* buffer_private_data);

extern "C" int V1GetInternalFrameBuffer(void* private_data,
                                        size_t y_plane_min_size,
                                        size_t uv_plane_min_size,
                                        FrameBuffer* frame_buffer);

extern "C" int V1ReleaseInternalFrameBuffer(void* private_data,
                                            FrameBuffer* frame_buffer);

class InternalFrameBufferList : public Allocable {
 public:
  static std::unique_ptr<InternalFrameBufferList> Create(size_t num_buffers);

  // Not copyable or movable.
  InternalFrameBufferList(const InternalFrameBufferList&) = delete;
  InternalFrameBufferList& operator=(const InternalFrameBufferList&) = delete;

  ~InternalFrameBufferList() = default;

  int OnFrameBufferSizeChanged(int bitdepth, Libgav1ImageFormat image_format,
                               int width, int height, int left_border,
                               int right_border, int top_border,
                               int bottom_border, int stride_alignment);

  int GetFrameBuffer(int bitdepth, Libgav1ImageFormat image_format, int width,
                     int height, int left_border, int right_border,
                     int top_border, int bottom_border, int stride_alignment,
                     Libgav1FrameBuffer2* frame_buffer);

  void ReleaseFrameBuffer(void* buffer_private_data);

  int V1GetFrameBuffer(size_t y_plane_min_size, size_t uv_plane_min_size,
                       FrameBuffer* frame_buffer);
  int V1ReleaseFrameBuffer(FrameBuffer* frame_buffer);

 private:
  struct Buffer : public Allocable {
    std::unique_ptr<uint8_t[], MallocDeleter> data;
    size_t size = 0;
    bool in_use = false;
  };

  InternalFrameBufferList(std::unique_ptr<Buffer[]> buffers,
                          size_t num_buffers);

  const std::unique_ptr<Buffer[]> buffers_;
  const size_t num_buffers_;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_INTERNAL_FRAME_BUFFER_LIST_H_
