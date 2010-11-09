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
      consumer_(consumer) {
}

RectangleUpdateDecoder::~RectangleUpdateDecoder() {
}

void RectangleUpdateDecoder::Initialize(const SessionConfig* config) {
  screen_size_ = gfx::Size(config->initial_resolution().width,
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

  if (!decoder_->IsReadyForData()) {
    InitializeDecoder(
        NewTracedMethod(this,
                        &RectangleUpdateDecoder::ProcessPacketData,
                        packet, done_runner.release()));
  } else {
    ProcessPacketData(packet, done_runner.release());
  }
}

void RectangleUpdateDecoder::ProcessPacketData(
    const VideoPacket* packet, Task* done) {
  AutoTaskRunner done_runner(done);

  if (!decoder_->IsReadyForData()) {
    // TODO(ajwong): This whole thing should move into an invalid state.
    LOG(ERROR) << "Decoder is unable to process data. Dropping packet.";
    return;
  }

  TraceContext::tracer()->PrintString("Executing Decode.");

  Decoder::DecodeResult result = decoder_->DecodePacket(packet);

  if (result == Decoder::DECODE_DONE) {
    UpdatedRects* rects = new UpdatedRects();
    decoder_->GetUpdatedRects(rects);
    consumer_->OnPartialFrameOutput(frame_, rects,
                                    new PartialFrameCleanup(frame_, rects));
  }
}

void RectangleUpdateDecoder::InitializeDecoder(Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewTracedMethod(this,
                        &RectangleUpdateDecoder::InitializeDecoder, done));
    return;
  }
  AutoTaskRunner done_runner(done);

  // Check if we need to request a new frame.
  if (!frame_ ||
      frame_->width() != static_cast<size_t>(screen_size_.width()) ||
      frame_->height() != static_cast<size_t>(screen_size_.height())) {
    if (frame_) {
      TraceContext::tracer()->PrintString("Releasing old frame.");
      consumer_->ReleaseFrame(frame_);
      frame_ = NULL;
    }
    TraceContext::tracer()->PrintString("Requesting new frame.");
    consumer_->AllocateFrame(media::VideoFrame::RGB32,
                             screen_size_.width(), screen_size_.height(),
                             base::TimeDelta(), base::TimeDelta(),
                             &frame_,
                             NewTracedMethod(
                                 this,
                                 &RectangleUpdateDecoder::InitializeDecoder,
                                 done_runner.release()));
    return;
  }

  // TODO(ajwong): We need to handle the allocator failing to create a frame
  // and properly disable this class.
  CHECK(frame_);

  decoder_->Reset();
  decoder_->Initialize(frame_);

  TraceContext::tracer()->PrintString("Decoder is Initialized");
}

}  // namespace remoting
