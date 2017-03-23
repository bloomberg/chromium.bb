/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef PNGImageDecoder_h
#define PNGImageDecoder_h

#include <memory>
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/png/PNGImageReader.h"

namespace blink {

class PLATFORM_EXPORT PNGImageDecoder final : public ImageDecoder {
  WTF_MAKE_NONCOPYABLE(PNGImageDecoder);

 public:
  PNGImageDecoder(AlphaOption,
                  const ColorBehavior&,
                  size_t maxDecodedBytes,
                  size_t offset = 0);
  ~PNGImageDecoder() override;

  // ImageDecoder:
  String filenameExtension() const override { return "png"; }
  bool setSize(unsigned, unsigned) override;
  int repetitionCount() const override;
  bool frameIsCompleteAtIndex(size_t) const override;
  float frameDurationAtIndex(size_t) const override;
  bool setFailed() override;

  // Callbacks from libpng
  void headerAvailable();
  void rowAvailable(unsigned char* row, unsigned rowIndex, int);
  void frameComplete();

  void setColorSpace();
  void setRepetitionCount(int);

 private:
  using ParseQuery = PNGImageReader::ParseQuery;

  // ImageDecoder:
  void decodeSize() override { parse(ParseQuery::Size); }
  void decode(size_t) override;
  void parse(ParseQuery);
  size_t decodeFrameCount() override;
  void initializeNewFrame(size_t) override;
  void clearFrameBuffer(size_t) override;
  bool canReusePreviousFrameBuffer(size_t) const override;

  std::unique_ptr<PNGImageReader> m_reader;
  const unsigned m_offset;
  size_t m_currentFrame;
  int m_repetitionCount;
  bool m_hasAlphaChannel;
  bool m_currentBufferSawAlpha;
};

}  // namespace blink

#endif
