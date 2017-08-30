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

#ifndef DeferredImageDecoder_h
#define DeferredImageDecoder_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "third_party/skia/include/core/SkRWBuffer.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class ImageFrameGenerator;
class SharedBuffer;
struct DeferredFrameData;

class PLATFORM_EXPORT DeferredImageDecoder final {
  WTF_MAKE_NONCOPYABLE(DeferredImageDecoder);
  USING_FAST_MALLOC(DeferredImageDecoder);

 public:
  static std::unique_ptr<DeferredImageDecoder> Create(RefPtr<SharedBuffer> data,
                                                      bool data_complete,
                                                      ImageDecoder::AlphaOption,
                                                      const ColorBehavior&);

  static std::unique_ptr<DeferredImageDecoder> CreateForTesting(
      std::unique_ptr<ImageDecoder>);

  ~DeferredImageDecoder();

  String FilenameExtension() const;

  sk_sp<PaintImageGenerator> CreateGeneratorAtIndex(size_t index);

  PassRefPtr<SharedBuffer> Data();
  void SetData(PassRefPtr<SharedBuffer> data, bool all_data_received);

  bool IsSizeAvailable();
  bool HasEmbeddedColorSpace() const;
  IntSize Size() const;
  IntSize FrameSizeAtIndex(size_t index) const;
  size_t FrameCount();
  int RepetitionCount() const;
  size_t ClearCacheExceptFrame(size_t index);
  bool FrameHasAlphaAtIndex(size_t index) const;
  bool FrameIsReceivedAtIndex(size_t index) const;
  // Duration is reported in seconds.
  // TODO(vmpstr): Use something like TimeDelta here.
  float FrameDurationAtIndex(size_t index) const;
  size_t FrameBytesAtIndex(size_t index) const;
  ImageOrientation OrientationAtIndex(size_t index) const;
  bool HotSpot(IntPoint&) const;

 private:
  explicit DeferredImageDecoder(std::unique_ptr<ImageDecoder> metadata_decoder);

  friend class DeferredImageDecoderTest;
  ImageFrameGenerator* FrameGenerator() { return frame_generator_.Get(); }

  void ActivateLazyDecoding();
  void PrepareLazyDecodedFrames();

  void SetDataInternal(RefPtr<SharedBuffer> data,
                       bool all_data_received,
                       bool push_data_to_decoder);

  // Copy of the data that is passed in, used by deferred decoding.
  // Allows creating readonly snapshots that may be read in another thread.
  std::unique_ptr<SkRWBuffer> rw_buffer_;
  std::unique_ptr<ImageDecoder> metadata_decoder_;

  String filename_extension_;
  IntSize size_;
  int repetition_count_;
  bool has_embedded_color_space_ = false;
  bool all_data_received_;
  bool can_yuv_decode_;
  bool has_hot_spot_;
  sk_sp<SkColorSpace> color_space_for_sk_images_;
  IntPoint hot_spot_;

  // Caches frame state information.
  Vector<DeferredFrameData> frame_data_;
  RefPtr<ImageFrameGenerator> frame_generator_;
};

}  // namespace blink

#endif
