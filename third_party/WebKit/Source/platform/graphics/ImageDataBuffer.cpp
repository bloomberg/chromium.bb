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
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"

namespace blink {

ImageDataBuffer::ImageDataBuffer(scoped_refptr<StaticBitmapImage> image) {
  if (!image)
    return;
  retained_image_ = image->PaintImageForCurrentFrame().GetSkImage();
  if (!retained_image_)
    return;
  if (retained_image_->isTextureBacked()) {
    retained_image_ = retained_image_->makeNonTextureImage();
    if (!retained_image_)
      return;
  } else if (retained_image_->isLazyGenerated()) {
    // Call readPixels() to trigger decoding.
    SkImageInfo info = SkImageInfo::MakeN32(1, 1, retained_image_->alphaType());
    std::unique_ptr<uint8_t[]> pixel(new uint8_t[info.bytesPerPixel()]());
    retained_image_->readPixels(info, pixel.get(), info.minRowBytes(), 1, 1);
  }
  if (!retained_image_->peekPixels(&pixmap_))
    return;
  is_valid_ = true;
  uses_pixmap_ = true;
  size_ = IntSize(image->width(), image->height());
}

ImageDataBuffer::ImageDataBuffer(const IntSize& size,
                                 const unsigned char* data,
                                 const CanvasColorParams& color_params)
    : data_(data),
      color_params_(color_params),
      uses_pixmap_(false),
      is_valid_(true),
      size_(size) {
  is_valid_ = data && !size_.IsEmpty();
}

ImageDataBuffer::ImageDataBuffer(const SkPixmap& pixmap)
    : pixmap_(pixmap),
      uses_pixmap_(true),
      size_(IntSize(pixmap.width(), pixmap.height())) {
  is_valid_ = pixmap_.addr() && !size_.IsEmpty();
}

std::unique_ptr<ImageDataBuffer> ImageDataBuffer::Create(
    scoped_refptr<StaticBitmapImage> image) {
  std::unique_ptr<ImageDataBuffer> buffer =
      WTF::WrapUnique(new ImageDataBuffer(image));
  if (!buffer->IsValid())
    return nullptr;
  return buffer;
}

std::unique_ptr<ImageDataBuffer> ImageDataBuffer::Create(
    const IntSize& size,
    const unsigned char* data,
    const CanvasColorParams& color_params) {
  std::unique_ptr<ImageDataBuffer> buffer =
      WTF::WrapUnique(new ImageDataBuffer(size, data, color_params));
  if (!buffer->IsValid())
    return nullptr;
  return buffer;
}

std::unique_ptr<ImageDataBuffer> ImageDataBuffer::Create(
    const SkPixmap& pixmap) {
  std::unique_ptr<ImageDataBuffer> buffer =
      WTF::WrapUnique(new ImageDataBuffer(pixmap));
  if (!buffer->IsValid())
    return nullptr;
  return buffer;
}

const unsigned char* ImageDataBuffer::Pixels() const {
  DCHECK(is_valid_);
  if (uses_pixmap_)
    return static_cast<const unsigned char*>(pixmap_.addr());
  return data_;
}

bool ImageDataBuffer::EncodeImage(const String& mime_type,
                                  const double& quality,
                                  Vector<unsigned char>* encoded_image) const {
  DCHECK(is_valid_);
  SkPixmap src;
  if (uses_pixmap_) {
    src = pixmap_;
  } else {
    SkColorType color_type = kRGBA_8888_SkColorType;
    if (color_params_.GetSkColorType() != kN32_SkColorType)
      color_type = kRGBA_F16_SkColorType;
    SkImageInfo info = SkImageInfo::Make(size_.Width(), size_.Height(),
                                         color_type, kUnpremul_SkAlphaType,
                                         color_params_.GetSkColorSpace());
    src.reset(info, data_, info.minRowBytes());
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
  DCHECK(is_valid_);
  DCHECK(MIMETypeRegistry::IsSupportedImageMIMETypeForEncoding(mime_type));

  Vector<unsigned char> result;
  if (!EncodeImage(mime_type, quality, &result))
    return "data:,";

  return "data:" + mime_type + ";base64," + Base64Encode(result);
}

}  // namespace blink
