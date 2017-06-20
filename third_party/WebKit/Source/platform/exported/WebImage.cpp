/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/platform/WebImage.h"

#include <algorithm>
#include <memory>
#include "platform/SharedBuffer.h"
#include "platform/graphics/Image.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebData.h"
#include "public/platform/WebSize.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

WebImage WebImage::FromData(const WebData& data, const WebSize& desired_size) {
  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
      data, true, ImageDecoder::kAlphaPremultiplied, ColorBehavior::Ignore()));
  if (!decoder || !decoder->IsSizeAvailable())
    return WebImage();

  // Frames are arranged by decreasing size, then decreasing bit depth.
  // Pick the frame closest to |desiredSize|'s area without being smaller,
  // which has the highest bit depth.
  const size_t frame_count = decoder->FrameCount();
  size_t index = 0;  // Default to first frame if none are large enough.
  int frame_area_at_index = 0;
  for (size_t i = 0; i < frame_count; ++i) {
    const IntSize frame_size = decoder->FrameSizeAtIndex(i);
    if (WebSize(frame_size) == desired_size) {
      index = i;
      break;  // Perfect match.
    }

    const int frame_area = frame_size.Width() * frame_size.Height();
    if (frame_area < (desired_size.width * desired_size.height))
      break;  // No more frames that are large enough.

    if (!i || (frame_area < frame_area_at_index)) {
      index = i;  // Closer to desired area than previous best match.
      frame_area_at_index = frame_area;
    }
  }

  ImageFrame* frame = decoder->FrameBufferAtIndex(index);
  return (frame && !decoder->Failed()) ? WebImage(frame->Bitmap()) : WebImage();
}

WebVector<WebImage> WebImage::FramesFromData(const WebData& data) {
  // This is to protect from malicious images. It should be big enough that it's
  // never hit in practice.
  const size_t kMaxFrameCount = 8;

  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::Create(
      data, true, ImageDecoder::kAlphaPremultiplied, ColorBehavior::Ignore()));
  if (!decoder || !decoder->IsSizeAvailable())
    return WebVector<WebImage>();

  // Frames are arranged by decreasing size, then decreasing bit depth.
  // Keep the first frame at every size, has the highest bit depth.
  const size_t frame_count = decoder->FrameCount();
  IntSize last_size;

  Vector<WebImage> frames;
  for (size_t i = 0; i < std::min(frame_count, kMaxFrameCount); ++i) {
    const IntSize frame_size = decoder->FrameSizeAtIndex(i);
    if (frame_size == last_size)
      continue;
    last_size = frame_size;

    ImageFrame* frame = decoder->FrameBufferAtIndex(i);
    if (!frame)
      continue;

    SkBitmap bitmap = frame->Bitmap();
    if (!bitmap.isNull() && frame->GetStatus() == ImageFrame::kFrameComplete)
      frames.push_back(WebImage(bitmap));
  }

  return frames;
}

void WebImage::Reset() {
  bitmap_.reset();
}

void WebImage::Assign(const WebImage& image) {
  bitmap_ = image.bitmap_;
}

bool WebImage::IsNull() const {
  return bitmap_.isNull();
}

WebSize WebImage::Size() const {
  return WebSize(bitmap_.width(), bitmap_.height());
}

WebImage::WebImage(PassRefPtr<Image> image) {
  if (!image)
    return;

  if (sk_sp<SkImage> sk_image = image->ImageForCurrentFrame())
    sk_image->asLegacyBitmap(&bitmap_, SkImage::kRO_LegacyBitmapMode);
}

}  // namespace blink
