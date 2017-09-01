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
#include "platform/wtf/Time.h"

namespace blink {

class PLATFORM_EXPORT PNGImageDecoder final : public ImageDecoder {
  WTF_MAKE_NONCOPYABLE(PNGImageDecoder);

 public:
  PNGImageDecoder(AlphaOption,
                  const ColorBehavior&,
                  size_t max_decoded_bytes,
                  size_t offset = 0);
  ~PNGImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "png"; }
  bool SetSize(unsigned, unsigned) override;
  int RepetitionCount() const override;
  bool FrameIsReceivedAtIndex(size_t) const override;
  TimeDelta FrameDurationAtIndex(size_t) const override;
  bool SetFailed() override;

  // Callbacks from libpng
  void HeaderAvailable();
  void RowAvailable(unsigned char* row, unsigned row_index, int);
  void FrameComplete();

  void SetColorSpace();
  void SetRepetitionCount(int);

 private:
  using ParseQuery = PNGImageReader::ParseQuery;

  // ImageDecoder:
  void DecodeSize() override { Parse(ParseQuery::kSize); }
  void Decode(size_t) override;
  void Parse(ParseQuery);
  size_t DecodeFrameCount() override;
  void InitializeNewFrame(size_t) override;
  void ClearFrameBuffer(size_t) override;
  bool CanReusePreviousFrameBuffer(size_t) const override;

  std::unique_ptr<PNGImageReader> reader_;
  const unsigned offset_;
  size_t current_frame_;
  int repetition_count_;
  bool has_alpha_channel_;
  bool current_buffer_saw_alpha_;
};

}  // namespace blink

#endif
