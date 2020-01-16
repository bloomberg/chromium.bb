// Copyright 2020 The libgav1 Authors
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

#include "src/frame_buffer_callback_adaptor.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <new>

#include "src/utils/common.h"
#include "src/utils/logging.h"

namespace libgav1 {

namespace {

// |align| must be a power of 2.
uint8_t* AlignAddr(uint8_t* const addr, const size_t align) {
  const auto value = reinterpret_cast<size_t>(addr);
  return reinterpret_cast<uint8_t*>(Align(value, align));
}

}  // namespace

extern "C" int OnFrameBufferSizeChangedAdaptor(
    void* callback_private_data, int bitdepth, bool is_monochrome,
    int8_t subsampling_x, int8_t subsampling_y, int width, int height,
    int left_border, int right_border, int top_border, int bottom_border,
    int stride_alignment) {
  static_cast<void>(callback_private_data);
  static_cast<void>(bitdepth);
  static_cast<void>(is_monochrome);
  static_cast<void>(subsampling_x);
  static_cast<void>(subsampling_y);
  static_cast<void>(width);
  static_cast<void>(height);
  static_cast<void>(left_border);
  static_cast<void>(right_border);
  static_cast<void>(top_border);
  static_cast<void>(bottom_border);
  static_cast<void>(stride_alignment);
  return 0;
}

// Size conventions:
// * Widths, heights, and border sizes are in pixels.
// * Strides and plane sizes are in bytes.
extern "C" int GetFrameBufferAdaptor(void* callback_private_data, int bitdepth,
                                     bool is_monochrome, int8_t subsampling_x,
                                     int8_t subsampling_y, int width,
                                     int height, int left_border,
                                     int right_border, int top_border,
                                     int bottom_border, int stride_alignment,
                                     Libgav1FrameBuffer2* frame_buffer2) {
#if LIBGAV1_MAX_BITDEPTH == 8
  static_cast<void>(bitdepth);
#endif
  auto* v1_callbacks =
      static_cast<V1FrameBufferCallbacks*>(callback_private_data);

  const int aligned_width = Align(width, 8);
  const int aligned_height = Align(height, 8);

  // Calculate y_stride (in bytes). It is padded to a multiple of 16 bytes.
  int y_stride = aligned_width + left_border + right_border;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) y_stride *= sizeof(uint16_t);
#endif
  y_stride = Align(y_stride, 16);
  // Size of the Y plane in bytes.
  const uint64_t y_plane_size = (aligned_height + top_border + bottom_border) *
                                    static_cast<uint64_t>(y_stride) +
                                (stride_alignment - 1);

  const int uv_width = is_monochrome ? 0 : aligned_width >> subsampling_x;
  const int uv_height = is_monochrome ? 0 : aligned_height >> subsampling_y;
  const int uv_left_border = is_monochrome ? 0 : left_border >> subsampling_x;
  const int uv_right_border = is_monochrome ? 0 : right_border >> subsampling_x;
  const int uv_top_border = is_monochrome ? 0 : top_border >> subsampling_y;
  const int uv_bottom_border =
      is_monochrome ? 0 : bottom_border >> subsampling_y;

  // Calculate uv_stride (in bytes). It is padded to a multiple of 16 bytes.
  int uv_stride = uv_width + uv_left_border + uv_right_border;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) uv_stride *= sizeof(uint16_t);
#endif
  uv_stride = Align(uv_stride, 16);
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

  std::unique_ptr<FrameBuffer> frame_buffer1(new (std::nothrow) FrameBuffer);
  if (frame_buffer1 == nullptr) return -1;

  if (v1_callbacks->get(v1_callbacks->callback_private_data,
                        static_cast<size_t>(y_plane_size),
                        static_cast<size_t>(uv_plane_size),
                        frame_buffer1.get()) < 0) {
    return -1;
  }

  if (frame_buffer1->data[0] == nullptr ||
      frame_buffer1->size[0] < y_plane_size ||
      (uv_plane_size != 0 && frame_buffer1->data[1] == nullptr) ||
      frame_buffer1->size[1] < uv_plane_size ||
      (uv_plane_size != 0 && frame_buffer1->data[2] == nullptr) ||
      frame_buffer1->size[2] < uv_plane_size) {
    assert(0 && "The buffer_pool->get_frame_buffer1_ callback malfunctioned.");
    LIBGAV1_DLOG(ERROR,
                 "The buffer_pool->get_frame_buffer1_ callback malfunctioned.");
    return -1;
  }

  uint8_t* y_buffer = frame_buffer1->data[0];
  uint8_t* u_buffer = !is_monochrome ? frame_buffer1->data[1] : nullptr;
  uint8_t* v_buffer = !is_monochrome ? frame_buffer1->data[2] : nullptr;

  int left_border_bytes = left_border;
  int uv_left_border_bytes = uv_left_border;
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth > 8) {
    left_border_bytes *= sizeof(uint16_t);
    uv_left_border_bytes *= sizeof(uint16_t);
  }
#endif
  frame_buffer2->plane[0] = AlignAddr(
      y_buffer + (top_border * y_stride) + left_border_bytes, stride_alignment);
  frame_buffer2->plane[1] =
      AlignAddr(u_buffer + (uv_top_border * uv_stride) + uv_left_border_bytes,
                stride_alignment);
  frame_buffer2->plane[2] =
      AlignAddr(v_buffer + (uv_top_border * uv_stride) + uv_left_border_bytes,
                stride_alignment);
  assert(!is_monochrome || frame_buffer2->plane[1] == nullptr);
  assert(!is_monochrome || frame_buffer2->plane[2] == nullptr);

  frame_buffer2->stride[0] = y_stride;
  frame_buffer2->stride[1] = frame_buffer2->stride[2] = uv_stride;

  frame_buffer2->private_data = frame_buffer1.release();

  return 0;
}

extern "C" int ReleaseFrameBufferAdaptor(void* callback_private_data,
                                         void* buffer_private_data) {
  auto* v1_callbacks =
      static_cast<V1FrameBufferCallbacks*>(callback_private_data);
  auto* frame_buffer1 = static_cast<FrameBuffer*>(buffer_private_data);
  assert(frame_buffer1->data[0] != nullptr);
  v1_callbacks->release(v1_callbacks->callback_private_data, frame_buffer1);
  delete frame_buffer1;
  return 0;
}

}  // namespace libgav1
