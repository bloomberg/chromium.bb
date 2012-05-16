// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECODER_H_
#define REMOTING_BASE_DECODER_H_

#include "base/basictypes.h"
#include "remoting/proto/video.pb.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSize.h"

namespace remoting {

// Interface for a decoder that takes a stream of bytes from the network and
// outputs frames of data.
//
// TODO(ajwong): Beef up this documentation once the API stablizes.
class Decoder {
 public:
  // DecodeResult is returned from DecodePacket() and indicates current state
  // of the decoder. DECODE_DONE means that last packet for the frame was
  // processed, and the frame can be displayed now. DECODE_IN_PROGRESS
  // indicates that the decoder must receive more data before the frame can be
  // displayed. DECODE_ERROR is returned if there was an error in the stream.
  enum DecodeResult {
    DECODE_ERROR = -1,
    DECODE_IN_PROGRESS,
    DECODE_DONE,
  };

  Decoder() {}
  virtual ~Decoder() {}

  // Initializes the decoder and sets the output dimensions.
  // |screen size| must not be empty.
  virtual void Initialize(const SkISize& screen_size) = 0;

  // Feeds more data into the decoder.
  virtual DecodeResult DecodePacket(const VideoPacket* packet) = 0;

  // Returns true if decoder is ready to accept data via DecodePacket.
  virtual bool IsReadyForData() = 0;

  virtual VideoPacketFormat::Encoding Encoding() = 0;

  // Marks the specified |region| of the view for update the next time
  // RenderFrame() is called.  |region| is expressed in |view_size| coordinates.
  // |view_size| must not be empty.
  virtual void Invalidate(const SkISize& view_size,
                          const SkRegion& region) = 0;

  // Copies invalidated pixels within |clip_area| to |image_buffer|. Pixels are
  // invalidated either by new data received in DecodePacket(), or by explicit
  // calls to Invalidate(). |clip_area| is specified in |view_size| coordinates.
  // If |view_size| differs from the source size then the copied pixels will be
  // scaled accordingly. |view_size| cannot be empty.
  //
  // |image_buffer|'s origin must correspond to the top-left of |clip_area|,
  // and the buffer must be large enough to hold |clip_area| RGBA32 pixels.
  // |image_stride| gives the output buffer's stride in pixels.
  //
  // On return, |output_region| contains the updated area, in |view_size|
  // coordinates.
  virtual void RenderFrame(const SkISize& view_size,
                           const SkIRect& clip_area,
                           uint8* image_buffer,
                           int image_stride,
                           SkRegion* output_region) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_H_
