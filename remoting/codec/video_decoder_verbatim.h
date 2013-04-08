// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_VERBATIM_H_
#define REMOTING_CODEC_VIDEO_DECODER_VERBATIM_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/codec/video_decoder.h"

namespace remoting {

// Video decoder implementations that decodes video packet encoded by
// VideoEncoderVerbatim. It just copies data from incoming packets to the
// video frames.
class VideoDecoderVerbatim : public VideoDecoder {
 public:
  virtual ~VideoDecoderVerbatim();

  VideoDecoderVerbatim();

  // VideoDecoder implementation.
  virtual bool IsReadyForData() OVERRIDE;
  virtual void Initialize(const SkISize& screen_size) OVERRIDE;
  virtual DecodeResult DecodePacket(const VideoPacket* packet) OVERRIDE;
  virtual VideoPacketFormat::Encoding Encoding() OVERRIDE;
  virtual void Invalidate(const SkISize& view_size,
                          const SkRegion& region) OVERRIDE;
  virtual void RenderFrame(const SkISize& view_size,
                           const SkIRect& clip_area,
                           uint8* image_buffer,
                           int image_stride,
                           SkRegion* output_region) OVERRIDE;

 private:
  enum State {
    kUninitialized,
    kReady,
    kProcessing,
    kPartitionDone,
    kDone,
    kError,
  };

  // Helper method. Called from DecodePacket to updated state of the decoder.
  void UpdateStateForPacket(const VideoPacket* packet);

  // The internal state of the decoder.
  State state_;

  // Keeps track of the updating rect.
  SkIRect clip_;

  // The position in the row that we are updating.
  int row_pos_;

  // The current row in the rect that we are updaing.
  int row_y_;

  // The region updated that hasn't been copied to the screen yet.
  SkRegion updated_region_;

  // Size of the remote screen.
  SkISize screen_size_;

  // The bitmap holding the remote screen bits.
  scoped_ptr<uint8[]> screen_buffer_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderVerbatim);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_VERBATIM_H_
