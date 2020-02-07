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

#include <cassert>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>

#include "src/frame_buffer_utils.h"
#include "src/utils/common.h"

namespace libgav1 {
extern "C" {

int OnInternalFrameBufferSizeChanged(void* callback_private_data, int bitdepth,
                                     Libgav1ImageFormat image_format, int width,
                                     int height, int left_border,
                                     int right_border, int top_border,
                                     int bottom_border, int stride_alignment) {
  auto* buffer_list =
      static_cast<InternalFrameBufferList*>(callback_private_data);
  return buffer_list->OnFrameBufferSizeChanged(
      bitdepth, image_format, width, height, left_border, right_border,
      top_border, bottom_border, stride_alignment);
}

int GetInternalFrameBuffer(void* callback_private_data, int bitdepth,
                           Libgav1ImageFormat image_format, int width,
                           int height, int left_border, int right_border,
                           int top_border, int bottom_border,
                           int stride_alignment,
                           Libgav1FrameBuffer2* frame_buffer) {
  auto* buffer_list =
      static_cast<InternalFrameBufferList*>(callback_private_data);
  return buffer_list->GetFrameBuffer(
      bitdepth, image_format, width, height, left_border, right_border,
      top_border, bottom_border, stride_alignment, frame_buffer);
}

void ReleaseInternalFrameBuffer(void* callback_private_data,
                                void* buffer_private_data) {
  auto* buffer_list =
      static_cast<InternalFrameBufferList*>(callback_private_data);
  buffer_list->ReleaseFrameBuffer(buffer_private_data);
}

int V1GetInternalFrameBuffer(void* private_data, size_t y_plane_min_size,
                             size_t uv_plane_min_size,
                             FrameBuffer* frame_buffer) {
  auto* buffer_list = static_cast<InternalFrameBufferList*>(private_data);
  return buffer_list->V1GetFrameBuffer(y_plane_min_size, uv_plane_min_size,
                                       frame_buffer);
}

int V1ReleaseInternalFrameBuffer(void* private_data,
                                 FrameBuffer* frame_buffer) {
  auto* buffer_list = static_cast<InternalFrameBufferList*>(private_data);
  return buffer_list->V1ReleaseFrameBuffer(frame_buffer);
}

}  // extern "C"

int InternalFrameBufferList::OnFrameBufferSizeChanged(
    int bitdepth, Libgav1ImageFormat image_format, int width, int height,
    int left_border, int right_border, int top_border, int bottom_border,
    int stride_alignment) {
  static_cast<void>(bitdepth);
  static_cast<void>(image_format);
  static_cast<void>(width);
  static_cast<void>(height);
  static_cast<void>(left_border);
  static_cast<void>(right_border);
  static_cast<void>(top_border);
  static_cast<void>(bottom_border);
  static_cast<void>(stride_alignment);
  return 0;
}

int InternalFrameBufferList::GetFrameBuffer(
    int bitdepth, Libgav1ImageFormat image_format, int width, int height,
    int left_border, int right_border, int top_border, int bottom_border,
    int stride_alignment, Libgav1FrameBuffer2* frame_buffer) {
#if LIBGAV1_MAX_BITDEPTH < 10
  static_cast<void>(bitdepth);
#endif
  // stride_alignment must be a power of 2.
  assert((stride_alignment & (stride_alignment - 1)) == 0);

  bool is_monochrome;
  int8_t subsampling_x;
  int8_t subsampling_y;
  DecomposeImageFormat(image_format, &is_monochrome, &subsampling_x,
                       &subsampling_y);

  // Calculate y_stride (in bytes). It is padded to a multiple of
  // |stride_alignment| bytes.
  int y_stride = width + left_border + right_border;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) y_stride *= sizeof(uint16_t);
#endif
  y_stride = Align(y_stride, stride_alignment);
  // Size of the Y plane in bytes.
  const uint64_t y_plane_size =
      (height + top_border + bottom_border) * static_cast<uint64_t>(y_stride) +
      (stride_alignment - 1);

  const int uv_width = is_monochrome ? 0 : width >> subsampling_x;
  const int uv_height = is_monochrome ? 0 : height >> subsampling_y;
  const int uv_left_border = is_monochrome ? 0 : left_border >> subsampling_x;
  const int uv_right_border = is_monochrome ? 0 : right_border >> subsampling_x;
  const int uv_top_border = is_monochrome ? 0 : top_border >> subsampling_y;
  const int uv_bottom_border =
      is_monochrome ? 0 : bottom_border >> subsampling_y;

  // Calculate uv_stride (in bytes). It is padded to a multiple of
  // |stride_alignment| bytes.
  int uv_stride = uv_width + uv_left_border + uv_right_border;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) uv_stride *= sizeof(uint16_t);
#endif
  uv_stride = Align(uv_stride, stride_alignment);
  // Size of the U or V plane in bytes.
  const uint64_t uv_plane_size =
      is_monochrome ? 0
                    : (uv_height + uv_top_border + uv_bottom_border) *
                              static_cast<uint64_t>(uv_stride) +
                          (stride_alignment - 1);

  if (y_plane_size != static_cast<size_t>(y_plane_size) ||
      uv_plane_size != static_cast<size_t>(uv_plane_size)) {
    return -1;
  }

  const auto y_plane_min_size = static_cast<size_t>(y_plane_size);
  const auto uv_plane_min_size = static_cast<size_t>(uv_plane_size);

  if (uv_plane_min_size > SIZE_MAX / 2 ||
      y_plane_min_size > SIZE_MAX - 2 * uv_plane_min_size) {
    return -1;
  }
  const size_t min_size = y_plane_min_size + 2 * uv_plane_min_size;

  Buffer* buffer = nullptr;
  for (auto& buffer_ptr : buffers_) {
    if (!buffer_ptr->in_use) {
      buffer = buffer_ptr.get();
      break;
    }
  }
  if (buffer == nullptr) {
    std::unique_ptr<Buffer> new_buffer(new (std::nothrow) Buffer);
    if (new_buffer == nullptr || !buffers_.push_back(std::move(new_buffer))) {
      return -1;
    }
    buffer = buffers_.back().get();
  }

  if (buffer->size < min_size) {
    std::unique_ptr<uint8_t[], MallocDeleter> new_data(
        static_cast<uint8_t*>(malloc(min_size)));
    if (new_data == nullptr) return -1;
    buffer->data = std::move(new_data);
    buffer->size = min_size;
  }

  uint8_t* y_buffer = buffer->data.get();
  uint8_t* u_buffer = is_monochrome ? nullptr : y_buffer + y_plane_min_size;
  uint8_t* v_buffer = is_monochrome ? nullptr : u_buffer + uv_plane_min_size;
  int left_border_bytes = left_border;
  int uv_left_border_bytes = uv_left_border;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) {
    left_border_bytes *= sizeof(uint16_t);
    uv_left_border_bytes *= sizeof(uint16_t);
  }
#endif
  frame_buffer->plane[0] = AlignAddr(
      y_buffer + (top_border * y_stride) + left_border_bytes, stride_alignment);
  frame_buffer->plane[1] =
      AlignAddr(u_buffer + (uv_top_border * uv_stride) + uv_left_border_bytes,
                stride_alignment);
  frame_buffer->plane[2] =
      AlignAddr(v_buffer + (uv_top_border * uv_stride) + uv_left_border_bytes,
                stride_alignment);
  frame_buffer->stride[0] = y_stride;
  frame_buffer->stride[1] = frame_buffer->stride[2] = uv_stride;
  frame_buffer->private_data = buffer;
  buffer->in_use = true;
  return 0;
}

void InternalFrameBufferList::ReleaseFrameBuffer(void* buffer_private_data) {
  auto* const buffer = static_cast<Buffer*>(buffer_private_data);
  buffer->in_use = false;
}

int InternalFrameBufferList::V1GetFrameBuffer(size_t y_plane_min_size,
                                              size_t uv_plane_min_size,
                                              FrameBuffer* frame_buffer) {
  if (uv_plane_min_size > SIZE_MAX / 2 ||
      y_plane_min_size > SIZE_MAX - 2 * uv_plane_min_size) {
    return -1;
  }
  const size_t min_size = y_plane_min_size + 2 * uv_plane_min_size;

  Buffer* buffer = nullptr;
  for (auto& buffer_ptr : buffers_) {
    if (!buffer_ptr->in_use) {
      buffer = buffer_ptr.get();
      break;
    }
  }
  if (buffer == nullptr) {
    std::unique_ptr<Buffer> new_buffer(new (std::nothrow) Buffer);
    if (new_buffer == nullptr || !buffers_.push_back(std::move(new_buffer))) {
      return -1;
    }
    buffer = buffers_.back().get();
  }

  if (buffer->size < min_size) {
    std::unique_ptr<uint8_t[], MallocDeleter> new_data(
        static_cast<uint8_t*>(malloc(min_size)));
    if (new_data == nullptr) return -1;
    buffer->data = std::move(new_data);
    buffer->size = min_size;
  }

  frame_buffer->data[0] = buffer->data.get();
  frame_buffer->size[0] = y_plane_min_size;
  frame_buffer->data[1] = frame_buffer->data[0] + y_plane_min_size;
  frame_buffer->size[1] = uv_plane_min_size;
  frame_buffer->data[2] = frame_buffer->data[1] + uv_plane_min_size;
  frame_buffer->size[2] = uv_plane_min_size;
  frame_buffer->private_data = buffer;
  buffer->in_use = true;
  return 0;
}

int InternalFrameBufferList::V1ReleaseFrameBuffer(FrameBuffer* frame_buffer) {
  auto* const buffer = static_cast<Buffer*>(frame_buffer->private_data);
  buffer->in_use = false;
  return 0;
}

}  // namespace libgav1
