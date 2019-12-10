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

#ifndef LIBGAV1_SRC_YUV_BUFFER_H_
#define LIBGAV1_SRC_YUV_BUFFER_H_

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/gav1/frame_buffer.h"
#include "src/utils/constants.h"

namespace libgav1 {

class YuvBuffer {
 public:
  // If the memory was allocated by YuvBuffer directly, the memory is freed.
  ~YuvBuffer();

  // Allocates the buffer. Returns true on success. Returns false on failure.
  //
  // * |width| and |height| are the image dimensions in pixels.
  // * |subsampling_x| and |subsampling_y| (either 0 or 1) specify the
  //   subsampling of the width and height of the chroma planes, respectively.
  // * |border| is the size of the borders (on all four sides) in pixels.
  // * |byte_alignment| specifies the additional alignment requirement of the
  //   data buffers of the Y, U, and V planes. If |byte_alignment| is 0, there
  //   is no additional alignment requirement. Otherwise, |byte_alignment|
  //   must be a power of 2 and greater than or equal to 16.
  //   NOTE: The strides are a multiple of 16. Therefore only the first row in
  //   each plane is aligned to |byte_alignment|. Subsequent rows are only
  //   16-byte aligned.
  // * If |get_frame_buffer| is not null, it is invoked to allocate the memory.
  //   If |get_frame_buffer| is null, YuvBuffer allocates the memory directly
  //   and ignores the |private_data| and |frame_buffer| parameters, which
  //   should be null.
  //
  // Example: bitdepth=8 width=20 height=6 border=2. The diagram below shows
  // how Realloc() allocates the data buffer for the Y plane.
  //
  //   16-byte aligned
  //          |
  //          v
  //        BBBBBBBBBBBBBBBBBBBBBBBBBBBBpppp
  //        BBBBBBBBBBBBBBBBBBBBBBBBBBBBpppp
  //        BB01234567890123456789....BBpppp
  //        BB11234567890123456789....BBpppp
  //        BB21234567890123456789....BBpppp
  //        BB31234567890123456789....BBpppp
  //        BB41234567890123456789....BBpppp
  //        BB51234567890123456789....BBpppp
  //        BB........................BBpppp
  //        BB........................BBpppp
  //        BBBBBBBBBBBBBBBBBBBBBBBBBBBBpppp
  //        BBBBBBBBBBBBBBBBBBBBBBBBBBBBpppp
  //        |                              |
  //        |<-- stride (multiple of 16) ->|
  //
  // The video frame has 6 rows of 20 pixels each. Each row is shown as the
  // pattern r1234567890123456789, where |r| is 0, 1, 2, 3, 4, 5.
  //
  // Realloc() first aligns |width| and |height| to multiples of 8 pixels. The
  // pixels added in this step are shown as dots ('.'). In this example, the
  // aligned width is 24 pixels and the aligned height is 8 pixels. NOTE: The
  // purpose of this step is unknown. We should be able to remove this step.
  //
  // Realloc() then adds a border of 2 pixels around this region. The border
  // pixels are shown as capital 'B'. NOTE: This example uses a tiny border of
  // 2 pixels to keep the diagram small. The current implementation of
  // Realloc() actually requires that |border| be a multiple of 32. We should
  // be able to only require that |border| be a multiple of 2.
  //
  // Each row is now padded to a multiple of the default alignment in bytes,
  // which is 16. The padding bytes are shown as lowercase 'p'. (Since
  // |bitdepth| is 8 in this example, each pixel is one byte.) The padded size
  // in bytes is the stride. In this example, the stride is 32 bytes.
  //
  // Finally, Realloc() aligns the first byte of frame data, which is the '0'
  // pixel/byte in the upper left corner of the frame, to the default (16-byte)
  // alignment boundary and also the |byte_alignment| boundary, if
  // |byte_alignment| is nonzero.
  //
  // TODO(wtc): Add a check for width and height limits to defend against
  // invalid bitstreams.
  bool Realloc(int bitdepth, bool is_monochrome, int width, int height,
               int8_t subsampling_x, int8_t subsampling_y, int border,
               int byte_alignment, GetFrameBufferCallback get_frame_buffer,
               void* private_data, FrameBuffer* frame_buffer);

  int bitdepth() const { return bitdepth_; }

  bool is_monochrome() const { return is_monochrome_; }

  int8_t subsampling_x() const { return subsampling_x_; }
  int8_t subsampling_y() const { return subsampling_y_; }

  int aligned_width(int plane) const {
    return (plane == kPlaneY) ? y_width_ : uv_width_;
  }
  int aligned_height(int plane) const {
    return (plane == kPlaneY) ? y_height_ : uv_height_;
  }

  int displayed_width(int plane) const {
    return (plane == kPlaneY) ? y_crop_width_ : uv_crop_width_;
  }
  int displayed_height(int plane) const {
    return (plane == kPlaneY) ? y_crop_height_ : uv_crop_height_;
  }

  // Returns border sizes in pixels.
  int left_border(int plane) const { return left_border_[plane]; }
  int right_border(int plane) const { return right_border_[plane]; }
  int top_border(int plane) const { return top_border_[plane]; }
  int bottom_border(int plane) const { return bottom_border_[plane]; }

  // Returns the alignment of frame buffer row in bytes.
  int alignment() const { return 16; }

  // Shifts the |plane| buffer horizontally by |horizontal_shift| pixels and
  // vertically by |vertical_shift| pixels. There must be enough border for the
  // shifts to be successful.
  // TODO(chengchen):
  // Warning: this implementation doesn't handle the byte_alignment requirement.
  // For example, if the frame is required to be 4K-byte aligned, this method
  // fails. Figure out alternative solutions if the feature of
  // byte_alignment is required in practice.
  void ShiftBuffer(int plane, int horizontal_shift, int vertical_shift) {
    assert(ValidHorizontalShift(plane, horizontal_shift));
    assert(ValidVerticalShift(plane, vertical_shift));
    left_border_[plane] += horizontal_shift;
    right_border_[plane] -= horizontal_shift;
    top_border_[plane] += vertical_shift;
    bottom_border_[plane] -= vertical_shift;
    const int pixel_size =
        static_cast<int>((bitdepth_ == 8) ? sizeof(uint8_t) : sizeof(uint16_t));
    buffer_[plane] +=
        vertical_shift * stride_[plane] + horizontal_shift * pixel_size;
  }

  // Returns the data buffer for |plane|.
  uint8_t* data(int plane) { return buffer_[plane]; }
  const uint8_t* data(int plane) const { return buffer_[plane]; }

  // Returns the stride in bytes for |plane|.
  int stride(int plane) const { return stride_[plane]; }

 private:
  // Frame buffer pointer, i.e., |buffer_[plane]| can only be shifted in loop
  // restoration. If loop restoration is applied on plane, |buffer_[plane]|
  // will be shifted kRestorationBorder rows above, and
  // kFrameBufferRowAlignment columns left.
  // |shift| is in pixels.
  // Positive vertical shift is a down shift. Negative vertical shift is an
  // up shift.
  bool ValidVerticalShift(int plane, int shift) const {
    return (shift >= 0) ? bottom_border_[plane] >= shift
                        : top_border_[plane] + shift >= 0;
  }
  // Positive horizontal shift is a right shift. Negative horizontal shift is
  // a left shift.
  bool ValidHorizontalShift(int plane, int shift) const {
    return (shift >= 0) ? right_border_[plane] >= shift
                        : left_border_[plane] + shift >= 0;
  }

  int bitdepth_ = 0;
  bool is_monochrome_ = false;

  // y_crop_width_ and y_crop_height_ are the original width and height (the
  // |width| and |height| arguments passed to the Realloc() method). y_width_
  // and y_height_ are the original width and height padded to a multiple of
  // 8.
  //
  // The UV widths and heights are computed from Y widths and heights as
  // follows:
  //   uv_crop_width_ = (y_crop_width_ + subsampling_x_) >> subsampling_x_
  //   uv_crop_height_ = (y_crop_height_ + subsampling_y_) >> subsampling_y_
  //   uv_width_ = y_width_ >> subsampling_x_
  //   uv_height_ = y_height_ >> subsampling_y_
  int y_width_ = 0;
  int uv_width_ = 0;
  int y_height_ = 0;
  int uv_height_ = 0;
  int y_crop_width_ = 0;
  int uv_crop_width_ = 0;
  int y_crop_height_ = 0;
  int uv_crop_height_ = 0;

  int left_border_[kMaxPlanes] = {};
  int right_border_[kMaxPlanes] = {};
  int top_border_[kMaxPlanes] = {};
  int bottom_border_[kMaxPlanes] = {};

  int stride_[kMaxPlanes] = {};
  uint8_t* buffer_[kMaxPlanes] = {};

  // buffer_alloc_ and buffer_alloc_size_ are only used if the
  // get_frame_buffer callback is null and we allocate the buffer ourselves.
  uint8_t* buffer_alloc_ = nullptr;
  size_t buffer_alloc_size_ = 0;

  int8_t subsampling_x_ = 0;  // 0 or 1.
  int8_t subsampling_y_ = 0;  // 0 or 1.
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_YUV_BUFFER_H_
