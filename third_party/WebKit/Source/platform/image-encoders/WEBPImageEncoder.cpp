/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/image-encoders/WEBPImageEncoder.h"

#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "webp/encode.h"

namespace blink {

static int WriteOutput(const uint8_t* data,
                       size_t size,
                       const WebPPicture* const picture) {
  static_cast<Vector<unsigned char>*>(picture->custom_ptr)->Append(data, size);
  return 1;
}

static bool EncodePixels(const IntSize& image_size,
                         const unsigned char* pixels,
                         int quality,
                         Vector<unsigned char>* output) {
  if (image_size.Width() <= 0 || image_size.Width() > WEBP_MAX_DIMENSION)
    return false;
  if (image_size.Height() <= 0 || image_size.Height() > WEBP_MAX_DIMENSION)
    return false;

  WebPConfig config;
  if (!WebPConfigInit(&config))
    return false;
  WebPPicture picture;
  if (!WebPPictureInit(&picture))
    return false;

  picture.width = image_size.Width();
  picture.height = image_size.Height();

  bool use_lossless_encoding = (quality >= 100);
  if (use_lossless_encoding)
    picture.use_argb = 1;
  if (!WebPPictureImportRGBA(&picture, pixels, picture.width * 4))
    return false;

  picture.custom_ptr = output;
  picture.writer = &WriteOutput;

  if (use_lossless_encoding) {
    config.lossless = 1;
    config.quality = 75;
    config.method = 0;
  } else {
    config.quality = quality;
    config.method = 3;
  }

  bool success = WebPEncode(&config, &picture);
  WebPPictureFree(&picture);
  return success;
}

bool WEBPImageEncoder::Encode(const ImageDataBuffer& image_data,
                              int quality,
                              Vector<unsigned char>* output) {
  if (!image_data.Pixels())
    return false;

  return EncodePixels(image_data.size(), image_data.Pixels(), quality, output);
}

}  // namespace blink
