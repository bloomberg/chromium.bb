/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
 * Copyright (C) 2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/ImageSource.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

ImageSource::ImageSource() {}

ImageSource::~ImageSource() {}

size_t ImageSource::ClearCacheExceptFrame(size_t clear_except_frame) {
  return decoder_ ? decoder_->ClearCacheExceptFrame(clear_except_frame) : 0;
}

PassRefPtr<SharedBuffer> ImageSource::Data() {
  return decoder_ ? decoder_->Data() : nullptr;
}

bool ImageSource::SetData(RefPtr<SharedBuffer> data, bool all_data_received) {
  all_data_received_ = all_data_received;

  if (decoder_) {
    decoder_->SetData(std::move(data), all_data_received);
    // If the decoder is pre-instantiated, it means we've already validated the
    // data/signature at some point.
    return true;
  }

  ColorBehavior color_behavior =
      RuntimeEnabledFeatures::ColorCorrectRenderingEnabled()
          ? ColorBehavior::Tag()
          : ColorBehavior::TransformToGlobalTarget();
  decoder_ = DeferredImageDecoder::Create(data, all_data_received,
                                          ImageDecoder::kAlphaPremultiplied,
                                          color_behavior);

  // Insufficient data is not a failure.
  return decoder_ || !ImageDecoder::HasSufficientDataToSniffImageType(*data);
}

String ImageSource::FilenameExtension() const {
  return decoder_ ? decoder_->FilenameExtension() : String();
}

bool ImageSource::IsSizeAvailable() {
  return decoder_ && decoder_->IsSizeAvailable();
}

bool ImageSource::HasColorProfile() const {
  return decoder_ && decoder_->HasEmbeddedColorSpace();
}

IntSize ImageSource::size(
    RespectImageOrientationEnum should_respect_orientation) const {
  return FrameSizeAtIndex(0, should_respect_orientation);
}

IntSize ImageSource::FrameSizeAtIndex(
    size_t index,
    RespectImageOrientationEnum should_respect_orientation) const {
  if (!decoder_)
    return IntSize();

  IntSize size = decoder_->FrameSizeAtIndex(index);
  if ((should_respect_orientation == kRespectImageOrientation) &&
      decoder_->OrientationAtIndex(index).UsesWidthAsHeight())
    return size.TransposedSize();

  return size;
}

bool ImageSource::GetHotSpot(IntPoint& hot_spot) const {
  return decoder_ ? decoder_->HotSpot(hot_spot) : false;
}

int ImageSource::RepetitionCount() {
  return decoder_ ? decoder_->RepetitionCount() : kAnimationNone;
}

size_t ImageSource::FrameCount() const {
  return decoder_ ? decoder_->FrameCount() : 0;
}

sk_sp<SkImage> ImageSource::CreateFrameAtIndex(size_t index) {
  if (!decoder_)
    return nullptr;

  return decoder_->CreateFrameAtIndex(index);
}

float ImageSource::FrameDurationAtIndex(size_t index) const {
  if (!decoder_)
    return 0;

  // Many annoying ads specify a 0 duration to make an image flash as quickly as
  // possible. We follow Firefox's behavior and use a duration of 100 ms for any
  // frames that specify a duration of <= 10 ms. See <rdar://problem/7689300>
  // and <http://webkit.org/b/36082> for more information.
  const float duration = decoder_->FrameDurationAtIndex(index) / 1000.0f;
  if (duration < 0.011f)
    return 0.100f;
  return duration;
}

ImageOrientation ImageSource::OrientationAtIndex(size_t index) const {
  return decoder_ ? decoder_->OrientationAtIndex(index)
                  : kDefaultImageOrientation;
}

bool ImageSource::FrameHasAlphaAtIndex(size_t index) const {
  return !decoder_ || decoder_->FrameHasAlphaAtIndex(index);
}

bool ImageSource::FrameIsReceivedAtIndex(size_t index) const {
  return decoder_ && decoder_->FrameIsReceivedAtIndex(index);
}

size_t ImageSource::FrameBytesAtIndex(size_t index) const {
  return decoder_ ? decoder_->FrameBytesAtIndex(index) : 0;
}

}  // namespace blink
