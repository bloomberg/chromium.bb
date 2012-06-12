// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_
#define REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_

#include <list>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/decoder.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/client/frame_producer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace pp {
class ImageData;
};

namespace remoting {

class VideoPacket;

namespace protocol {
class SessionConfig;
}  // namespace protocol

// TODO(ajwong): Re-examine this API, especially with regards to how error
// conditions on each step are reported.  Should they be CHECKs? Logs? Other?
// TODO(sergeyu): Rename this class.
class RectangleUpdateDecoder
    : public base::RefCountedThreadSafe<RectangleUpdateDecoder>,
      public FrameProducer {
 public:
  // Creates an update decoder on |task_runner_|, outputting to |consumer|.
  // TODO(wez): Replace the ref-counted proxy with an owned FrameConsumer.
  RectangleUpdateDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<FrameConsumerProxy> consumer);

  // Initializes decoder with the information from the protocol config.
  void Initialize(const protocol::SessionConfig& config);

  // Decodes the contents of |packet|. DecodePacket may keep a reference to
  // |packet| so the |packet| must remain alive and valid until |done| is
  // executed.
  void DecodePacket(scoped_ptr<VideoPacket> packet, const base::Closure& done);

  // FrameProducer implementation.  These methods may be called before we are
  // Initialize()d, or we know the source screen size.
  virtual void DrawBuffer(pp::ImageData* buffer) OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& region) OVERRIDE;
  virtual void RequestReturnBuffers(const base::Closure& done) OVERRIDE;
  virtual void SetOutputSizeAndClip(const SkISize& view_size,
                                    const SkIRect& clip_area) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<RectangleUpdateDecoder>;
  virtual ~RectangleUpdateDecoder();

  // Paints the invalidated region to the next available buffer and returns it
  // to the consumer.
  void SchedulePaint();
  void DoPaint();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<FrameConsumerProxy> consumer_;
  scoped_ptr<Decoder> decoder_;

  // Remote screen size in pixels.
  SkISize source_size_;

  // The current dimentions of the frame consumer view.
  SkISize view_size_;
  SkIRect clip_area_;

  // The drawing buffers supplied by the frame consumer.
  std::list<pp::ImageData*> buffers_;

  // Flag used to coalesce runs of SchedulePaint()s into a single DoPaint().
  bool paint_scheduled_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_RECTANGLE_UPDATE_DECODER_H_
