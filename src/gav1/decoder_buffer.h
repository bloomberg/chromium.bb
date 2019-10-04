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

#ifndef LIBGAV1_SRC_GAV1_DECODER_BUFFER_H_
#define LIBGAV1_SRC_GAV1_DECODER_BUFFER_H_

#include <cstdint>

#include "gav1/symbol_visibility.h"

// All the declarations in this file are part of the public ABI.

namespace libgav1 {

enum ChromaSamplePosition : uint8_t {
  kChromaSamplePositionUnknown,
  kChromaSamplePositionVertical,
  kChromaSamplePositionColocated,
  kChromaSamplePositionReserved
};

enum ImageFormat : uint8_t {
  kImageFormatYuv420,
  kImageFormatYuv422,
  kImageFormatYuv444,
  kImageFormatMonochrome400
};

struct LIBGAV1_PUBLIC DecoderBuffer {
  int NumPlanes() const {
    return (image_format == kImageFormatMonochrome400) ? 1 : 3;
  }

  ChromaSamplePosition chroma_sample_position;
  ImageFormat image_format;

  // TODO(wtc): Add the following members:
  // - color range
  //   * studio range: Y [16..235], UV [16..240]
  //   * full range: (YUV/RGB [0..255]
  // - CICP Color Primaries (cp)
  // - CICP Transfer Characteristics (tc)
  // - CICP Matrix Coefficients (mc)

  // Image storage dimensions.
  // NOTE: These fields are named w and h in vpx_image_t and aom_image_t.
  // uint32_t width;  // Stored image width.
  // uint32_t height;  // Stored image height.
  int bitdepth;  // Stored image bitdepth.

  // Image display dimensions.
  // NOTES:
  // 1. These fields are named d_w and d_h in vpx_image_t and aom_image_t.
  // 2. libvpx and libaom clients use d_w and d_h much more often than w and h.
  // 3. These fields can just be stored for the Y plane and the clients can
  //    calculate the values for the U and V planes if the image format or
  //    subsampling is exposed.
  int displayed_width[3];   // Displayed image width.
  int displayed_height[3];  // Displayed image height.

  int stride[3];
  uint8_t* plane[3];

  // The |user_private_data| argument passed to Decoder::EnqueueFrame().
  int64_t user_private_data;
  // The |private_data| field of FrameBuffer. Set by the get frame buffer
  // callback when it allocates a frame buffer.
  void* buffer_private_data;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_GAV1_DECODER_BUFFER_H_
