/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DecodingImageGenerator_h
#define DecodingImageGenerator_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/image-decoders/SegmentReader.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "third_party/skia/include/core/SkImageInfo.h"

class SkData;

namespace blink {

class ImageFrameGenerator;

// Implements SkImageGenerator, used by SkPixelRef to populate a discardable
// memory with a decoded image frame. ImageFrameGenerator does the actual
// decoding.
class PLATFORM_EXPORT DecodingImageGenerator final
    : public PaintImageGenerator {
  USING_FAST_MALLOC(DecodingImageGenerator);
  WTF_MAKE_NONCOPYABLE(DecodingImageGenerator);

 public:
  static std::unique_ptr<SkImageGenerator> Create(SkData*);

  DecodingImageGenerator(PassRefPtr<ImageFrameGenerator>,
                         const SkImageInfo&,
                         PassRefPtr<SegmentReader>,
                         bool all_data_received,
                         size_t index);
  ~DecodingImageGenerator() override;

  void SetCanYUVDecode(bool yes) { can_yuv_decode_ = yes; }

  // PaintImageGenerator implementation.
  sk_sp<SkData> GetEncodedData() const override;
  bool GetPixels(const SkImageInfo&,
                 void* pixels,
                 size_t row_bytes,
                 uint32_t lazy_pixel_ref) override;
  bool QueryYUV8(SkYUVSizeInfo*, SkYUVColorSpace*) const override;
  bool GetYUV8Planes(const SkYUVSizeInfo&,
                     void* planes[3],
                     uint32_t lazy_pixel_ref) override;

 private:
  RefPtr<ImageFrameGenerator> frame_generator_;
  const RefPtr<SegmentReader> data_;  // Data source.
  const bool all_data_received_;
  const size_t frame_index_;
  bool can_yuv_decode_;
};

}  // namespace blink

#endif  // DecodingImageGenerator_h_
