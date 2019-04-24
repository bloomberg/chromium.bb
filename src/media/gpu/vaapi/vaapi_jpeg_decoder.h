// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/geometry/size.h"

// This data type is defined in va/va.h using typedef, reproduced here.
typedef unsigned int VASurfaceID;

namespace media {

class ScopedVAImage;
class VaapiWrapper;

enum class VaapiJpegDecodeStatus {
  kSuccess,
  kParseJpegFailed,
  kUnsupportedJpeg,
  kUnsupportedSubsampling,
  kSurfaceCreationFailed,
  kSubmitPicParamsFailed,
  kSubmitIQMatrixFailed,
  kSubmitHuffmanFailed,
  kSubmitSliceParamsFailed,
  kSubmitSliceDataFailed,
  kExecuteDecodeFailed,
  kUnsupportedSurfaceFormat,
  kCannotGetImage,
  kInvalidState,
};

class VaapiJpegDecoder final {
 public:
  VaapiJpegDecoder();
  ~VaapiJpegDecoder();

  // Initializes |vaapi_wrapper_| in kDecode mode with VAProfileJPEGBaseline
  // profile and |error_uma_cb| for error reporting.
  bool Initialize(const base::RepeatingClosure& error_uma_cb);

  // Decodes a JPEG picture. It will fill VA-API parameters and call the
  // corresponding VA-API methods according to the JPEG in |encoded_image|.
  // Decoded data will be returned as a ScopedVAImage. Returns nullptr on
  // failure and sets *|status| to the reason for failure.
  std::unique_ptr<ScopedVAImage> DoDecode(
      base::span<const uint8_t> encoded_image,
      VaapiJpegDecodeStatus* status);

 private:
  // TODO(andrescj): move vaapi_utils tests out of vaapi_jpeg_decoder_unittest
  // and remove this friend declaration.
  friend class VaapiJpegDecoderTest;
  FRIEND_TEST_ALL_PREFIXES(VaapiJpegDecoderTest, ScopedVAImage);

  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  // The current VA surface for decoding.
  VASurfaceID va_surface_id_;
  // The coded size associated with |va_surface_id_|.
  gfx::Size coded_size_;
  // The VA RT format associated with |va_surface_id_|.
  unsigned int va_rt_format_;

  DISALLOW_COPY_AND_ASSIGN(VaapiJpegDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_
