// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_DECODER_H_
#define REMOTING_BASE_DECODER_H_

#include "base/task.h"
#include "base/scoped_ptr.h"
#include "gfx/rect.h"
#include "media/base/video_frame.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

typedef std::vector<gfx::Rect> UpdatedRects;

// Interface for a decoder that takes a stream of bytes from the network and
// outputs frames of data.
//
// TODO(ajwong): Beef up this documentation once the API stablizes.
class Decoder {
 public:
  Decoder() {}
  virtual ~Decoder() {}

  // TODO(ajwong): This API is incorrect in the face of a streaming decode
  // protocol like VP8.  However, it breaks the layering abstraction by
  // depending on the network packet protocol buffer type. I'm going to go
  // forward with it as is, and then refactor again to support streaming
  // decodes.

  // Initializes the decoder to draw into the given |frame|.  The |clip|
  // specifies the region to draw into.  The clip region must fit inside
  // the dimensions of frame. Failure to do so will CHECK Fail.
  //
  // TODO(ajwong): Should this take the source pixel format?
  // TODO(ajwong): Should the protocol be split into basic-types followed
  //   by packet types?  Basic types might include the format enum below.
  virtual void Initialize(scoped_refptr<media::VideoFrame> frame,
                          const gfx::Rect& clip, int bytes_per_src_pixel) = 0;

  // Reset the decoder to an uninitialized state. Release all references to
  // the initialized |frame|.  Initialize() must be called before the decoder
  // is used again.
  virtual void Reset() = 0;

  // Feeds more data into the decoder.
  virtual void DecodeBytes(const std::string& encoded_bytes) = 0;

  // Returns true if decoder is ready to accept data via ProcessRectangleData.
  virtual bool IsReadyForData() = 0;

  virtual UpdateStreamEncoding Encoding() = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_DECODER_H_
