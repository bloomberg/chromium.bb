// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECODER_H_
#define REMOTING_BASE_DECODER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "media/base/video_frame.h"
#include "remoting/proto/video.pb.h"
#include "ui/gfx/rect.h"

namespace remoting {

typedef std::vector<gfx::Rect> UpdatedRects;

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

  // Initializes the decoder to draw into the given |frame|.
  virtual void Initialize(scoped_refptr<media::VideoFrame> frame) = 0;

  // Feeds more data into the decoder.
  virtual DecodeResult DecodePacket(const VideoPacket* packet) = 0;

  // Returns rects that were updated in the last frame. Can be called only
  // after DecodePacket returned DECODE_DONE. Caller keeps ownership of
  // |rects|. |rects| is kept empty if whole screen needs to be updated.
  // TODO(dmaclach): Move this over to using SkRegion.
  // http://crbug.com/92085
  virtual void GetUpdatedRects(UpdatedRects* rects) = 0;

  // Reset the decoder to an uninitialized state. Release all references to
  // the initialized |frame|.  Initialize() must be called before the decoder
  // is used again.
  virtual void Reset() = 0;

  // Returns true if decoder is ready to accept data via DecodePacket.
  virtual bool IsReadyForData() = 0;

  virtual VideoPacketFormat::Encoding Encoding() = 0;

  // Set the scale factors of the decoded output. If the decoder doesn't support
  // scaling then this all is ignored.
  // If both |horizontal_ratio| and |vertical_ratio| equal 1.0 then scaling is
  // turned off.
  virtual void SetScaleRatios(double horizontal_ratio, double vertical_ratio) {}

  // Set the clipping rectangle to the decoder. Decoder should respect this and
  // only output changes in this rectangle. The new clipping rectangle will be
  // effective on the next decoded video frame.
  //
  // When scaling is enabled clipping rectangles are ignored.
  virtual void SetClipRect(const gfx::Rect& clip_rect) {}

  // Force decoder to output a video frame with content in |rects| using the
  // last decoded video frame.
  //
  // Coordinates of rectangles supplied here are before scaling.
  virtual void RefreshRects(const std::vector<gfx::Rect>& rects) {}
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_H_
