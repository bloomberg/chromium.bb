/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#include "platform/graphics/ImageDataBuffer.h"

#include <memory>
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/image-encoders/ImageEncoder.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"

namespace blink {

const unsigned char* ImageDataBuffer::Pixels() const {
  if (uses_pixmap_)
    return static_cast<const unsigned char*>(pixmap_.addr());
  return data_;
}

std::unique_ptr<ImageDataBuffer> ImageDataBuffer::Create(
    scoped_refptr<StaticBitmapImage> image) {
  if (!image || !image->PaintImageForCurrentFrame().GetSkImage())
    return nullptr;
  return WTF::WrapUnique(new ImageDataBuffer(image));
}

ImageDataBuffer::ImageDataBuffer(scoped_refptr<StaticBitmapImage> image) {
  image_bitmap_ = image;
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  if (skia_image->isTextureBacked()) {
    skia_image = skia_image->makeNonTextureImage();
    image_bitmap_ = StaticBitmapImage::Create(skia_image);
  } else if (skia_image->isLazyGenerated()) {
    // Call readPixels() to trigger decoding.
    SkImageInfo info = SkImageInfo::MakeN32(1, 1, skia_image->alphaType());
    std::unique_ptr<uint8_t[]> pixel(new uint8_t[info.bytesPerPixel()]());
    skia_image->readPixels(info, pixel.get(), info.minRowBytes(), 1, 1);
  }
  bool peek_pixels = skia_image->peekPixels(&pixmap_);
  DCHECK(peek_pixels);
  uses_pixmap_ = true;
  size_ = IntSize(image->width(), image->height());
}

bool ImageDataBuffer::EncodeImage(const String& mime_type,
                                  const double& quality,
                                  Vector<unsigned char>* encoded_image) const {
  SkPixmap src;
  if (uses_pixmap_) {
    src = pixmap_;
  } else {
    SkImageInfo info =
        SkImageInfo::Make(Width(), Height(), kRGBA_8888_SkColorType,
                          kUnpremul_SkAlphaType, nullptr);
    const size_t rowBytes = info.minRowBytes();
    src.reset(info, Pixels(), rowBytes);
  }

  if (mime_type == "image/jpeg") {
    SkJpegEncoder::Options options;
    options.fQuality = ImageEncoder::ComputeJpegQuality(quality);
    options.fAlphaOption = SkJpegEncoder::AlphaOption::kBlendOnBlack;
    options.fBlendBehavior = SkTransferFunctionBehavior::kIgnore;
    if (options.fQuality == 100) {
      options.fDownsample = SkJpegEncoder::Downsample::k444;
    }
    return ImageEncoder::Encode(encoded_image, src, options);
  }

  if (mime_type == "image/webp") {
    SkWebpEncoder::Options options = ImageEncoder::ComputeWebpOptions(
        quality, SkTransferFunctionBehavior::kIgnore);
    return ImageEncoder::Encode(encoded_image, src, options);
  }

  DCHECK_EQ(mime_type, "image/png");
  SkPngEncoder::Options options;
  options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
  options.fZLibLevel = 3;
  options.fUnpremulBehavior = SkTransferFunctionBehavior::kIgnore;
  return ImageEncoder::Encode(encoded_image, src, options);
}

String ImageDataBuffer::ToDataURL(const String& mime_type,
                                  const double& quality) const {
  DCHECK(MIMETypeRegistry::IsSupportedImageMIMETypeForEncoding(mime_type));

  Vector<unsigned char> result;
  if (!EncodeImage(mime_type, quality, &result))
    return "data:,";

  return "data:" + mime_type + ";base64," + Base64Encode(result);
}

}  // namespace blink
