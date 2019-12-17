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

#include <algorithm>
#include <cassert>
#include <cstddef>
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
                        int bottom_border, int byte_alignment,
                        GetFrameBufferCallback get_frame_buffer,
                        void* private_data, FrameBuffer* frame_buffer) {
  // Only support allocating buffers that have borders that are a multiple of
  // 2. The border restriction is required because we may subsample the
  // borders in the chroma planes.
  if ((left_border & 1) != 0 || (right_border & 1) != 0 ||
      (top_border & 1) != 0 || (bottom_border & 1) != 0) {
    return false;
  }

  assert(byte_alignment == 0 || byte_alignment >= 16);
  const int byte_align = (byte_alignment == 0) ? 1 : byte_alignment;
  // byte_align must be a power of 2.
  assert((byte_align & (byte_align - 1)) == 0);
  // Every row in the plane buffers needs to be 16-byte aligned. In addition,
  // the plane buffers need to be |byte_align|-byte aligned. Since the strides
  // are multiples of 16 bytes, it suffices to just align the plane buffers to
  // the larger of 16 and |byte_align|.
  const int plane_align = std::max(16, byte_align);

  // aligned_width and aligned_height are width and height padded to a
  // multiple of 8 pixels.
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
                                (plane_align - 1);

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
                          (plane_align - 1);

  // Allocate unaligned y_buffer, u_buffer, and v_buffer.
  uint8_t* y_buffer = nullptr;
  uint8_t* u_buffer = nullptr;
  uint8_t* v_buffer = nullptr;

  if (get_frame_buffer != nullptr) {
    assert(frame_buffer != nullptr);

    if (y_plane_size != static_cast<size_t>(y_plane_size) ||
        uv_plane_size != static_cast<size_t>(uv_plane_size)) {
      return false;
    }

    // Allocation to hold larger frame, or first allocation.
    if (get_frame_buffer(private_data, static_cast<size_t>(y_plane_size),
                         static_cast<size_t>(uv_plane_size),
                         frame_buffer) < 0) {
      return false;
    }

    if (frame_buffer->data[0] == nullptr ||
        frame_buffer->size[0] < y_plane_size ||
        (uv_plane_size != 0 && frame_buffer->data[1] == nullptr) ||
        frame_buffer->size[1] < uv_plane_size ||
        (uv_plane_size != 0 && frame_buffer->data[2] == nullptr) ||
        frame_buffer->size[2] < uv_plane_size) {
      assert(0 && "The get_frame_buffer callback malfunctioned.");
      LIBGAV1_DLOG(ERROR, "The get_frame_buffer callback malfunctioned.");
      return false;
    }

    y_buffer = frame_buffer->data[0];
    if (!is_monochrome) {
      u_buffer = frame_buffer->data[1];
      v_buffer = frame_buffer->data[2];
    }
  } else {
    assert(private_data == nullptr);
    assert(frame_buffer == nullptr);

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
  }

  y_crop_width_ = width;
  y_crop_height_ = height;
  y_width_ = aligned_width;
  y_height_ = aligned_height;
  stride_[kPlaneY] = y_stride;
  left_border_[kPlaneY] = left_border;
  right_border_[kPlaneY] = right_border;
  top_border_[kPlaneY] = top_border;
  bottom_border_[kPlaneY] = bottom_border;

  uv_crop_width_ = is_monochrome ? 0 : (width + subsampling_x) >> subsampling_x;
  uv_crop_height_ =
      is_monochrome ? 0 : (height + subsampling_y) >> subsampling_y;
  uv_width_ = uv_width;
  uv_height_ = uv_height;
  stride_[kPlaneU] = stride_[kPlaneV] = uv_stride;
  left_border_[kPlaneU] = left_border_[kPlaneV] = uv_left_border;
  right_border_[kPlaneU] = right_border_[kPlaneV] = uv_right_border;
  top_border_[kPlaneU] = top_border_[kPlaneV] = uv_top_border;
  bottom_border_[kPlaneU] = bottom_border_[kPlaneV] = uv_bottom_border;

  subsampling_x_ = subsampling_x;
  subsampling_y_ = subsampling_y;

  bitdepth_ = bitdepth;
  is_monochrome_ = is_monochrome;
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
  assert(!is_monochrome || buffer_[kPlaneU] == nullptr);
  assert(!is_monochrome || buffer_[kPlaneV] == nullptr);

  return true;
}

}  // namespace libgav1
