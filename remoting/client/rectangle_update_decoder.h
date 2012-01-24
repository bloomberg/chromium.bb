// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_
#define REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "remoting/base/decoder.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

class FrameConsumer;
class VideoPacket;

namespace protocol {
class SessionConfig;
}  // namespace protocol

// TODO(ajwong): Re-examine this API, especially with regards to how error
// conditions on each step are reported.  Should they be CHECKs? Logs? Other?
// TODO(sergeyu): Rename this class.
class RectangleUpdateDecoder :
    public base::RefCountedThreadSafe<RectangleUpdateDecoder> {
 public:
  RectangleUpdateDecoder(base::MessageLoopProxy* message_loop,
                         FrameConsumer* consumer);

  // Initializes decoder with the infromation from the protocol config.
  void Initialize(const protocol::SessionConfig& config);

  // Decodes the contents of |packet| calling OnPartialFrameOutput() in the
  // regsitered as data is avaialable. DecodePacket may keep a reference to
  // |packet| so the |packet| must remain alive and valid until |done| is
  // executed.
  void DecodePacket(const VideoPacket* packet, const base::Closure& done);

  // Set the output dimensions to scale video output to.
  void SetOutputSize(const SkISize& size);

  // Set a new clipping rectangle for the decoder. Decoder should respect
  // this clipping rectangle and only decode content in this rectangle and
  // report dirty rectangles accordingly to enhance performance.
  void UpdateClipRect(const SkIRect& clip_rect);

  // Force the decoder to output the last decoded video frame without any
  // clipping.
  void RefreshFullFrame();

 private:
  friend class base::RefCountedThreadSafe<RectangleUpdateDecoder>;
  friend class PartialFrameCleanup;

  ~RectangleUpdateDecoder();

  void AllocateFrame(const VideoPacket* packet, const base::Closure& done);
  void ProcessPacketData(const VideoPacket* packet, const base::Closure& done);

  // Obtain updated rectangles from decoder and submit it to the consumer.
  void SubmitToConsumer();

  // Use |refresh_rects_| to do a refresh to the backing video frame.
  // When done the affected rectangles are submitted to the consumer.
  void DoRefresh();

  // Callback for FrameConsumer::OnPartialFrameOutput()
  void OnFrameConsumed(RectVector* rects);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  FrameConsumer* consumer_;

  SkISize screen_size_;
  SkIRect clip_rect_;
  RectVector refresh_rects_;

  scoped_ptr<Decoder> decoder_;
  bool decoder_needs_reset_;

  // The video frame that the decoder writes to.
  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_
