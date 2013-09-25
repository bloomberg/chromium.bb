// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_H_
#define REMOTING_CODEC_VIDEO_DECODER_H_

#include "base/basictypes.h"
#include "remoting/proto/video.pb.h"

namespace webrtc {
class DesktopRect;
class DesktopRegion;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

// Interface for a decoder that takes a stream of bytes from the network and
// outputs frames of data.
class VideoDecoder {
 public:
  static const int kBytesPerPixel = 4;

  VideoDecoder() {}
  virtual ~VideoDecoder() {}

  // Initializes the decoder and sets the output dimensions.
  // |screen size| must not be empty.
  virtual void Initialize(const webrtc::DesktopSize& screen_size) = 0;

  // Feeds more data into the decoder. Returns true if |packet| was processed
  // and the frame can be displayed now.
  virtual bool DecodePacket(const VideoPacket& packet) = 0;

  // Marks the specified |region| of the view for update the next time
  // RenderFrame() is called.  |region| is expressed in |view_size| coordinates.
  // |view_size| must not be empty.
  virtual void Invalidate(const webrtc::DesktopSize& view_size,
                          const webrtc::DesktopRegion& region) = 0;

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
  virtual void RenderFrame(const webrtc::DesktopSize& view_size,
                           const webrtc::DesktopRect& clip_area,
                           uint8* image_buffer,
                           int image_stride,
                           webrtc::DesktopRegion* output_region) = 0;

  // Returns the "shape", if any, of the most recently rendered frame.
  // The shape is returned in source dimensions.
  virtual const webrtc::DesktopRegion* GetImageShape() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_H_
