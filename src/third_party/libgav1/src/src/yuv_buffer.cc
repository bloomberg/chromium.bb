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

#include "src/yuv_buffer.h"

#include <cassert>
#include <cstddef>
#include <new>

#include "src/frame_buffer_utils.h"
#include "src/utils/common.h"
#include "src/utils/logging.h"

namespace libgav1 {

// Size conventions:
// * Widths, heights, and border sizes are in pixels.
// * Strides and plane sizes are in bytes.
//
// YuvBuffer objects may be reused through the BufferPool. Realloc() must
// assume that data members (except buffer_alloc_ and buffer_alloc_size_) may
// contain stale values from the previous use, and must set all data members
// from scratch. In particular, Realloc() must not rely on the initial values
// of data members set by the YuvBuffer constructor.
bool YuvBuffer::Realloc(int bitdepth, bool is_monochrome, int width, int height,
                        int8_t subsampling_x, int8_t subsampling_y,
                        int left_border, int right_border, int top_border,
                        int bottom_border,
                        GetFrameBufferCallback get_frame_buffer,
                        void* callback_private_data,
                        void** buffer_private_data) {
  // Only support allocating buffers that have borders that are a multiple of
  // 2. The border restriction is required because we may subsample the
  // borders in the chroma planes.
  if (((left_border | right_border | top_border | bottom_border) & 1) != 0) {
    LIBGAV1_DLOG(ERROR,
                 "Borders must be a multiple of 2: left_border = %d, "
                 "right_border = %d, top_border = %d, bottom_border = %d.",
                 left_border, right_border, top_border, bottom_border);
    return false;
  }

  // Every row in the plane buffers needs to be 16-byte aligned. Since the
  // strides are multiples of 16 bytes, it suffices to just make the plane
  // buffers 16-byte aligned.
  const int plane_align = 16;

  const int uv_width =
      is_monochrome ? 0 : SubsampledValue(width, subsampling_x);
  const int uv_height =
      is_monochrome ? 0 : SubsampledValue(height, subsampling_y);
  const int uv_left_border = is_monochrome ? 0 : left_border >> subsampling_x;
  const int uv_right_border = is_monochrome ? 0 : right_border >> subsampling_x;
  const int uv_top_border = is_monochrome ? 0 : top_border >> subsampling_y;
  const int uv_bottom_border =
      is_monochrome ? 0 : bottom_border >> subsampling_y;

  if (get_frame_buffer != nullptr) {
    assert(buffer_private_data != nullptr);

    const Libgav1ImageFormat image_format =
        ComposeImageFormat(is_monochrome, subsampling_x, subsampling_y);
    FrameBuffer frame_buffer;
    if (get_frame_buffer(callback_private_data, bitdepth, image_format, width,
                         height, left_border, right_border, top_border,
                         bottom_border,
                         /*stride_alignment=*/16, &frame_buffer) != kStatusOk) {
      return false;
    }

    if (frame_buffer.plane[0] == nullptr ||
        (!is_monochrome && frame_buffer.plane[1] == nullptr) ||
        (!is_monochrome && frame_buffer.plane[2] == nullptr)) {
      assert(false && "The get_frame_buffer callback malfunctioned.");
      LIBGAV1_DLOG(ERROR, "The get_frame_buffer callback malfunctioned.");
      return false;
    }

    stride_[kPlaneY] = frame_buffer.stride[0];
    stride_[kPlaneU] = frame_buffer.stride[1];
    stride_[kPlaneV] = frame_buffer.stride[2];
    buffer_[kPlaneY] = frame_buffer.plane[0];
    buffer_[kPlaneU] = frame_buffer.plane[1];
    buffer_[kPlaneV] = frame_buffer.plane[2];
    *buffer_private_data = frame_buffer.private_data;
  } else {
    assert(callback_private_data == nullptr);
    assert(buffer_private_data == nullptr);

    // Calculate y_stride (in bytes). It is padded to a multiple of 16 bytes.
    int y_stride = width + left_border + right_border;
#if LIBGAV1_MAX_BITDEPTH >= 10
    if (bitdepth > 8) y_stride *= sizeof(uint16_t);
#endif
    y_stride = Align(y_stride, 16);
    // Size of the Y plane in bytes.
    const uint64_t y_plane_size = (height + top_border + bottom_border) *
                                      static_cast<uint64_t>(y_stride) +
                                  (plane_align - 1);

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
                            (plane_align - 1);

    // Allocate unaligned y_buffer, u_buffer, and v_buffer.
    uint8_t* y_buffer = nullptr;
    uint8_t* u_buffer = nullptr;
    uint8_t* v_buffer = nullptr;

    const uint64_t frame_size = y_plane_size + 2 * uv_plane_size;
    if (frame_size > buffer_alloc_size_) {
      // Allocation to hold larger frame, or first allocation.
      if (frame_size != static_cast<size_t>(frame_size)) return false;

      buffer_alloc_.reset(new (std::nothrow)
                              uint8_t[static_cast<size_t>(frame_size)]);
      if (buffer_alloc_ == nullptr) {
        buffer_alloc_size_ = 0;
        return false;
      }

      buffer_alloc_size_ = static_cast<size_t>(frame_size);
    }

    y_buffer = buffer_alloc_.get();
    if (!is_monochrome) {
      u_buffer = y_buffer + y_plane_size;
      v_buffer = u_buffer + uv_plane_size;
    }

    stride_[kPlaneY] = y_stride;
    stride_[kPlaneU] = stride_[kPlaneV] = uv_stride;

    int left_border_bytes = left_border;
    int uv_left_border_bytes = uv_left_border;
#if LIBGAV1_MAX_BITDEPTH >= 10
    if (bitdepth > 8) {
      left_border_bytes *= sizeof(uint16_t);
      uv_left_border_bytes *= sizeof(uint16_t);
    }
#endif
    buffer_[kPlaneY] = AlignAddr(
        y_buffer + (top_border * y_stride) + left_border_bytes, plane_align);
    buffer_[kPlaneU] =
        AlignAddr(u_buffer + (uv_top_border * uv_stride) + uv_left_border_bytes,
                  plane_align);
    buffer_[kPlaneV] =
        AlignAddr(v_buffer + (uv_top_border * uv_stride) + uv_left_border_bytes,
                  plane_align);
  }

  y_width_ = width;
  y_height_ = height;
  left_border_[kPlaneY] = left_border;
  right_border_[kPlaneY] = right_border;
  top_border_[kPlaneY] = top_border;
  bottom_border_[kPlaneY] = bottom_border;

  uv_width_ = uv_width;
  uv_height_ = uv_height;
  left_border_[kPlaneU] = left_border_[kPlaneV] = uv_left_border;
  right_border_[kPlaneU] = right_border_[kPlaneV] = uv_right_border;
  top_border_[kPlaneU] = top_border_[kPlaneV] = uv_top_border;
  bottom_border_[kPlaneU] = bottom_border_[kPlaneV] = uv_bottom_border;

  subsampling_x_ = subsampling_x;
  subsampling_y_ = subsampling_y;

  bitdepth_ = bitdepth;
  is_monochrome_ = is_monochrome;
  assert(!is_monochrome || stride_[kPlaneU] == 0);
  assert(!is_monochrome || stride_[kPlaneV] == 0);
  assert(!is_monochrome || buffer_[kPlaneU] == nullptr);
  assert(!is_monochrome || buffer_[kPlaneV] == nullptr);

  return true;
}

}  // namespace libgav1
