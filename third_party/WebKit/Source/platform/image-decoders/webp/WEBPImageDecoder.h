/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef WEBPImageDecoder_h
#define WEBPImageDecoder_h

#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/core/SkData.h"
#include "webp/decode.h"
#include "webp/demux.h"
#include "wtf/RefPtr.h"

namespace blink {

class PLATFORM_EXPORT WEBPImageDecoder final : public ImageDecoder {
  WTF_MAKE_NONCOPYABLE(WEBPImageDecoder);

 public:
  WEBPImageDecoder(AlphaOption,
                   ColorSpaceOption,
                   sk_sp<SkColorSpace>,
                   size_t maxDecodedBytes);
  ~WEBPImageDecoder() override;

  // ImageDecoder:
  String filenameExtension() const override { return "webp"; }
  void onSetData(SegmentReader* data) override;
  int repetitionCount() const override;
  bool frameIsCompleteAtIndex(size_t) const override;
  float frameDurationAtIndex(size_t) const override;

 private:
  // ImageDecoder:
  virtual void decodeSize() { updateDemuxer(); }
  size_t decodeFrameCount() override;
  void initializeNewFrame(size_t) override;
  void decode(size_t) override;

  bool decodeSingleFrame(const uint8_t* dataBytes,
                         size_t dataSize,
                         size_t frameIndex);

  // For WebP images, the frame status needs to be FrameComplete to decode
  // subsequent frames that depend on frame |index|. The reason for this is that
  // WebP uses the previous frame for alpha blending, in applyPostProcessing().
  //
  // Before calling this, verify that frame |index| exists by checking that
  // |index| is smaller than |m_frameBufferCache|.size().
  bool frameStatusSufficientForSuccessors(size_t index) override {
    DCHECK(index < m_frameBufferCache.size());
    return m_frameBufferCache[index].getStatus() == ImageFrame::FrameComplete;
  }

  WebPIDecoder* m_decoder;
  WebPDecBuffer m_decoderBuffer;
  int m_formatFlags;
  bool m_frameBackgroundHasAlpha;

  void readColorProfile();
  bool updateDemuxer();

  // Set |m_frameBackgroundHasAlpha| based on this frame's characteristics.
  // Before calling this method, the caller must verify that the frame exists.
  void onInitFrameBuffer(size_t frameIndex) override;

  // When the blending method of this frame is BlendAtopPreviousFrame, the
  // previous frame's buffer is necessary to decode this frame in
  // applyPostProcessing, so we can't take over the data. Before calling this
  // method, the caller must verify that the frame exists.
  bool canReusePreviousFrameBuffer(size_t frameIndex) const override;

  void applyPostProcessing(size_t frameIndex);
  void clearFrameBuffer(size_t frameIndex) override;

  WebPDemuxer* m_demux;
  WebPDemuxState m_demuxState;
  bool m_haveAlreadyParsedThisData;
  int m_repetitionCount;
  int m_decodedHeight;

  typedef void (*AlphaBlendFunction)(ImageFrame&, ImageFrame&, int, int, int);
  AlphaBlendFunction m_blendFunction;

  void clear();
  void clearDecoder();

  // FIXME: Update libwebp's API so it does not require copying the data on each
  // update.
  sk_sp<SkData> m_consolidatedData;
};

}  // namespace blink

#endif
