// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H
#define REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "media/base/video_frame.h"
#include "remoting/base/decoder.h"  // For UpdatedRects.

class MessageLoop;

namespace remoting {

class FrameConsumer;
class RectangleFormat;
class RectangleUpdatePacket;

// TODO(ajwong): Re-examine this API, especially with regards to how error
// conditions on each step are reported.  Should they be CHECKs? Logs? Other?
class RectangleUpdateDecoder {
 public:
  RectangleUpdateDecoder(MessageLoop* message_loop,
                         FrameConsumer* consumer);
  ~RectangleUpdateDecoder();

  // Decodes the contents of |packet| calling OnPartialFrameOutput() in the
  // regsitered as data is avaialable. DecodePacket may keep a reference to
  // |packet| so the |packet| must remain alive and valid until |done| is
  // executed.
  //
  // TODO(ajwong): Should packet be a const pointer to make the lifetime
  // more clear?
  void DecodePacket(const RectangleUpdatePacket& packet, Task* done);

 private:
  static bool IsValidPacket(const RectangleUpdatePacket& packet);

  void InitializeDecoder(const RectangleFormat& format, Task* done);

  void ProcessPacketData(const RectangleUpdatePacket& packet, Task* done);

  // Pointers to infrastructure objects.  Not owned.
  MessageLoop* message_loop_;
  FrameConsumer* consumer_;

  scoped_ptr<Decoder> decoder_;
  UpdatedRects updated_rects_;

  // Framebuffer for the decoder.
  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::RectangleUpdateDecoder);

#endif  // REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H
