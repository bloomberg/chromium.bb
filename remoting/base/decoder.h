// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECODER_H_
#define REMOTING_BASE_DECODER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
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
  virtual void Initialize(const SkISize& screen_size) = 0;

  // Feeds more data into the decoder.
  virtual DecodeResult DecodePacket(const VideoPacket* packet) = 0;

  // Returns true if decoder is ready to accept data via DecodePacket.
  virtual bool IsReadyForData() = 0;

  virtual VideoPacketFormat::Encoding Encoding() = 0;

  // Forces the decoder to include the specified |region| the next time
  // RenderFrame() is called. |region| is expressed in output coordinates.
  virtual void Invalidate(const SkISize& view_size,
                          const SkRegion& region) = 0;

  // Copies invalidated pixels of the video frame to |image_buffer|. Both
  // decoding a packet or Invalidate() call can result in parts of the frame
  // to be invalidated. Only the pixels within |clip_area| are copied.
  // Invalidated pixels outside of |clip_area| remain invalidated.
  //
  // The routine sets |output_region| to indicate the updated areas of
  // |image_buffer|. |output_region| is in output buffer coordinates.
  //
  // |image_buffer| is assumed to be large enough to hold entire |clip_area|
  // (RGBA32). The top left corner of the buffer corresponds to the top left
  // corner of |clip_area|. |image_stride| specifies the size of a single row
  // of the buffer in bytes.
  //
  // Both |clip_area| and |output_region| are expressed in output coordinates.
  virtual void RenderFrame(const SkISize& view_size,
                           const SkIRect& clip_area,
                           uint8* image_buffer,
                           int image_stride,
                           SkRegion* output_region) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_H_
