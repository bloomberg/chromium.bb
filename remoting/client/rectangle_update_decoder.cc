// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/rectangle_update_decoder.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "media/base/callback.h"
#include "remoting/base/decoder.h"
#include "remoting/base/decoder_row_based.h"
#include "remoting/base/decoder_vp8.h"
#include "remoting/base/tracer.h"
#include "remoting/base/util.h"
#include "remoting/client/frame_consumer.h"
#include "remoting/protocol/session_config.h"

using media::AutoTaskRunner;
using remoting::protocol::ChannelConfig;
using remoting::protocol::SessionConfig;

namespace remoting {

namespace {

class PartialFrameCleanup : public Task {
 public:
  PartialFrameCleanup(media::VideoFrame* frame, UpdatedRects* rects)
      : frame_(frame), rects_(rects) {
  }

  virtual void Run() {
    delete rects_;
    frame_ = NULL;
  }

 private:
  scoped_refptr<media::VideoFrame> frame_;
  UpdatedRects* rects_;
};

}  // namespace

RectangleUpdateDecoder::RectangleUpdateDecoder(MessageLoop* message_loop,
                                               FrameConsumer* consumer)
    : message_loop_(message_loop),
      consumer_(consumer),
      frame_is_new_(false) {
}

RectangleUpdateDecoder::~RectangleUpdateDecoder() {
}

void RectangleUpdateDecoder::Initialize(const SessionConfig* config) {
  initial_screen_size_ = gfx::Size(config->initial_resolution().width,
                                   config->initial_resolution().height);

  // Initialize decoder based on the selected codec.
  ChannelConfig::Codec codec = config->video_config().codec;
  if (codec == ChannelConfig::CODEC_VERBATIM) {
    TraceContext::tracer()->PrintString("Creating Verbatim decoder.");
    decoder_.reset(DecoderRowBased::CreateVerbatimDecoder());
  } else if (codec == ChannelConfig::CODEC_ZIP) {
    TraceContext::tracer()->PrintString("Creating Zlib decoder");
    decoder_.reset(DecoderRowBased::CreateZlibDecoder());
    // TODO(sergeyu): Enable VP8 on ARM builds.
#if !defined(ARCH_CPU_ARM_FAMILY)
  } else if (codec == ChannelConfig::CODEC_VP8) {
      TraceContext::tracer()->PrintString("Creating VP8 decoder");
      decoder_.reset(new DecoderVp8());
#endif
  } else {
    NOTREACHED() << "Invalid Encoding found: " << codec;
  }
}

void RectangleUpdateDecoder::DecodePacket(const VideoPacket* packet,
                                          Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewTracedMethod(this,
                        &RectangleUpdateDecoder::DecodePacket, packet,
                        done));
    return;
  }
  AutoTaskRunner done_runner(done);

  TraceContext::tracer()->PrintString("Decode Packet called.");

  AllocateFrame(packet, done_runner.release());
}

void RectangleUpdateDecoder::AllocateFrame(const VideoPacket* packet,
                                           Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewTracedMethod(this,
                        &RectangleUpdateDecoder::AllocateFrame, packet, done));
    return;
  }
  AutoTaskRunner done_runner(done);

  TraceContext::tracer()->PrintString("AllocateFrame called.");

  // Find the required frame size.
  bool has_screen_size = packet->format().has_screen_width() &&
                         packet->format().has_screen_height();
  gfx::Size screen_size(packet->format().screen_width(),
                        packet->format().screen_height());
  if (!has_screen_size)
    screen_size = initial_screen_size_;

  // Find the current frame size.
  gfx::Size frame_size(0, 0);
  if (frame_)
    frame_size = gfx::Size(static_cast<int>(frame_->width()),
                           static_cast<int>(frame_->height()));

  // Allocate a new frame, if necessary.
  if ((!frame_) || (has_screen_size && (screen_size != frame_size))) {
    if (frame_) {
      TraceContext::tracer()->PrintString("Releasing old frame.");
      consumer_->ReleaseFrame(frame_);
      frame_ = NULL;
    }
    TraceContext::tracer()->PrintString("Requesting new frame.");

    consumer_->AllocateFrame(media::VideoFrame::RGB32,
                             screen_size.width(), screen_size.height(),
                             base::TimeDelta(), base::TimeDelta(),
                             &frame_,
                             NewRunnableMethod(this,
                                 &RectangleUpdateDecoder::ProcessPacketData,
                                 packet, done_runner.release()));
    frame_is_new_ = true;
    return;
  }
  ProcessPacketData(packet, done_runner.release());
}

void RectangleUpdateDecoder::ProcessPacketData(
    const VideoPacket* packet, Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewTracedMethod(this,
                        &RectangleUpdateDecoder::ProcessPacketData, packet,
                        done));
    return;
  }
  AutoTaskRunner done_runner(done);

  if (frame_is_new_) {
    decoder_->Reset();
    decoder_->Initialize(frame_);
    frame_is_new_ = false;
  }

  if (!decoder_->IsReadyForData()) {
    // TODO(ajwong): This whole thing should move into an invalid state.
    LOG(ERROR) << "Decoder is unable to process data. Dropping packet.";
    return;
  }

  TraceContext::tracer()->PrintString("Executing Decode.");

  if (decoder_->DecodePacket(packet) == Decoder::DECODE_DONE) {
    UpdatedRects* rects = new UpdatedRects();
    decoder_->GetUpdatedRects(rects);
    consumer_->OnPartialFrameOutput(frame_, rects,
                                    new PartialFrameCleanup(frame_, rects));
  }
}

}  // namespace remoting
