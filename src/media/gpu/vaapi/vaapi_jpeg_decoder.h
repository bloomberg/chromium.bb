// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/geometry/size.h"

// This data type is defined in va/va.h using typedef, reproduced here.
typedef unsigned int VASurfaceID;

namespace media {

struct JpegFrameHeader;
class ScopedVAImage;
class VASurface;
class VaapiWrapper;

enum class VaapiJpegDecodeStatus : uint32_t {
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

constexpr unsigned int kInvalidVaRtFormat = 0u;

// Returns the internal format required for a JPEG image given its parsed
// |frame_header|. If the image's subsampling format is not one of 4:2:0, 4:2:2,
// or 4:4:4, returns kInvalidVaRtFormat.
unsigned int VaSurfaceFormatForJpeg(const JpegFrameHeader& frame_header);

// Encapsulates a VaapiWrapper for the purpose of performing
// hardware-accelerated JPEG decodes. Objects of this class are not thread-safe,
// but they are also not thread-affine, i.e., the caller is free to call the
// methods on any thread, but calls must be synchronized externally.
class VaapiJpegDecoder final {
 public:
  VaapiJpegDecoder();
  ~VaapiJpegDecoder();

  // Initializes |vaapi_wrapper_| in kDecode mode with VAProfileJPEGBaseline
  // profile and |error_uma_cb| for error reporting.
  bool Initialize(const base::RepeatingClosure& error_uma_cb);

  // Decodes a JPEG picture. It will fill VA-API parameters and call the
  // corresponding VA-API methods according to the JPEG in |encoded_image|.
  // The image will be decoded into an internally allocated VA surface. It
  // will be returned as an unowned VASurface, which remains valid until the
  // next Decode() call or destruction of this class. Returns nullptr on
  // failure and sets *|status| to the reason for failure.
  scoped_refptr<VASurface> Decode(base::span<const uint8_t> encoded_image,
                                  VaapiJpegDecodeStatus* status);

  // Get the decoded data from the last Decode() call as a ScopedVAImage. The
  // VAImage's format will be either |preferred_image_fourcc| if the conversion
  // from the internal format is supported or a fallback FOURCC (see
  // VaapiWrapper::GetJpegDecodeSuitableImageFourCC() for details). Returns
  // nullptr on failure and sets *|status| to the reason for failure.
  std::unique_ptr<ScopedVAImage> GetImage(uint32_t preferred_image_fourcc,
                                          VaapiJpegDecodeStatus* status);

 private:
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
