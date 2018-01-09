/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef ImageFrameGenerator_h
#define ImageFrameGenerator_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "platform/PlatformExport.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/SegmentReader.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/Vector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkTypes.h"

struct SkYUVSizeInfo;

namespace blink {

class ImageDecoder;

class PLATFORM_EXPORT ImageDecoderFactory {
  USING_FAST_MALLOC(ImageDecoderFactory);
  WTF_MAKE_NONCOPYABLE(ImageDecoderFactory);

 public:
  ImageDecoderFactory() = default;
  virtual ~ImageDecoderFactory() = default;
  virtual std::unique_ptr<ImageDecoder> Create() = 0;
};

class PLATFORM_EXPORT ImageFrameGenerator final
    : public ThreadSafeRefCounted<ImageFrameGenerator> {
  WTF_MAKE_NONCOPYABLE(ImageFrameGenerator);

 public:
  static scoped_refptr<ImageFrameGenerator> Create(
      const SkISize& full_size,
      bool is_multi_frame,
      const ColorBehavior& color_behavior,
      std::vector<SkISize> supported_sizes) {
    return base::AdoptRef(new ImageFrameGenerator(
        full_size, is_multi_frame, color_behavior, std::move(supported_sizes)));
  }

  ~ImageFrameGenerator();

  // Decodes and scales the specified frame at |index|. The dimensions and
  // output format are given in SkImageInfo. Decoded pixels are written into
  // |pixels| with a stride of |rowBytes|. Returns true if decoding was
  // successful.
  bool DecodeAndScale(SegmentReader*,
                      bool all_data_received,
                      size_t index,
                      const SkImageInfo&,
                      void* pixels,
                      size_t row_bytes,
                      ImageDecoder::AlphaOption);

  // Decodes YUV components directly into the provided memory planes. Must not
  // be called unless getYUVComponentSizes has been called and returned true.
  // YUV decoding does not currently support progressive decoding. In order to
  // support it, ImageDecoder needs something analagous to its ImageFrame cache
  // to hold partial planes, and the GPU code needs to handle them.
  bool DecodeToYUV(SegmentReader*,
                   size_t index,
                   const SkISize component_sizes[3],
                   void* planes[3],
                   const size_t row_bytes[3]);

  const SkISize& GetFullSize() const { return full_size_; }

  SkISize GetSupportedDecodeSize(const SkISize& requested_size) const;

  bool IsMultiFrame() const { return is_multi_frame_; }
  bool DecodeFailed() const { return decode_failed_; }

  bool HasAlpha(size_t index);

  // Must not be called unless the SkROBuffer has all the data. YUV decoding
  // does not currently support progressive decoding. See comment above on
  // decodeToYUV().
  bool GetYUVComponentSizes(SegmentReader*, SkYUVSizeInfo*);

 private:
  ImageFrameGenerator(const SkISize& full_size,
                      bool is_multi_frame,
                      const ColorBehavior&,
                      std::vector<SkISize> supported_sizes);

  friend class ImageFrameGeneratorTest;
  friend class DeferredImageDecoderTest;
  // For testing. |factory| will overwrite the default ImageDecoder creation
  // logic if |factory->create()| returns non-zero.
  void SetImageDecoderFactory(std::unique_ptr<ImageDecoderFactory> factory) {
    image_decoder_factory_ = std::move(factory);
  }

  void SetHasAlpha(size_t index, bool has_alpha);

  SkBitmap TryToResumeDecode(SegmentReader*,
                             bool all_data_received,
                             size_t index,
                             const SkISize& scaled_size,
                             SkBitmap::Allocator&,
                             ImageDecoder::AlphaOption);
  // This method should only be called while decode_mutex_ is locked.
  // Returns a pointer to frame |index|'s ImageFrame, if available.
  // Sets |used_external_allocator| to true if the the image was decoded into
  // |external_allocator|'s memory.
  ImageFrame* Decode(SegmentReader*,
                     bool all_data_received,
                     size_t index,
                     ImageDecoder**,
                     SkBitmap::Allocator& external_allocator,
                     ImageDecoder::AlphaOption,
                     const SkISize& scaled_size,
                     bool& used_external_allocator);

  const SkISize full_size_;

  // Parameters used to create internal ImageDecoder objects.
  const ColorBehavior decoder_color_behavior_;

  const bool is_multi_frame_;
  bool decode_failed_;
  bool yuv_decoding_failed_;
  size_t frame_count_;
  Vector<bool> has_alpha_;
  std::vector<SkISize> supported_sizes_;

  std::unique_ptr<ImageDecoderFactory> image_decoder_factory_;

  // Prevents multiple decode operations on the same data.
  Mutex decode_mutex_;

  // Protect concurrent access to has_alpha_.
  Mutex alpha_mutex_;
};

}  // namespace blink

#endif
