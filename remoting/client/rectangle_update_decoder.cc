// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/rectangle_update_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "remoting/base/decoder.h"
#include "remoting/base/decoder_row_based.h"
#include "remoting/base/decoder_vp8.h"
#include "remoting/base/util.h"
#include "remoting/client/frame_consumer.h"
#include "remoting/protocol/session_config.h"

using remoting::protocol::ChannelConfig;
using remoting::protocol::SessionConfig;

namespace remoting {

RectangleUpdateDecoder::RectangleUpdateDecoder(MessageLoop* message_loop,
                                               FrameConsumer* consumer)
    : message_loop_(message_loop),
      consumer_(consumer),
      screen_size_(SkISize::Make(0, 0)),
      clip_rect_(SkIRect::MakeEmpty()),
      decoder_needs_reset_(false) {
}

RectangleUpdateDecoder::~RectangleUpdateDecoder() {
}

void RectangleUpdateDecoder::Initialize(const SessionConfig& config) {
  // Initialize decoder based on the selected codec.
  ChannelConfig::Codec codec = config.video_config().codec;
  if (codec == ChannelConfig::CODEC_VERBATIM) {
    decoder_.reset(DecoderRowBased::CreateVerbatimDecoder());
  } else if (codec == ChannelConfig::CODEC_ZIP) {
    decoder_.reset(DecoderRowBased::CreateZlibDecoder());
  } else if (codec == ChannelConfig::CODEC_VP8) {
    decoder_.reset(new DecoderVp8());
  } else {
    NOTREACHED() << "Invalid Encoding found: " << codec;
  }
}

void RectangleUpdateDecoder::DecodePacket(const VideoPacket* packet,
                                          const base::Closure& done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::DecodePacket,
                              this, packet, done));
    return;
  }
  AllocateFrame(packet, done);
}

void RectangleUpdateDecoder::AllocateFrame(const VideoPacket* packet,
                                           const base::Closure& done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::AllocateFrame,
                              this, packet, done));
    return;
  }
  base::ScopedClosureRunner done_runner(done);

  // If the packet includes a screen size, store it.
  if (packet->format().has_screen_width() &&
      packet->format().has_screen_height()) {
    screen_size_.set(packet->format().screen_width(),
                     packet->format().screen_height());
  }

  // If we've never seen a screen size, ignore the packet.
  if (screen_size_.isZero()) {
    return;
  }

  // Ensure the output frame is the right size.
  SkISize frame_size = SkISize::Make(0, 0);
  if (frame_)
    frame_size.set(frame_->width(), frame_->height());

  // Allocate a new frame, if necessary.
  if ((!frame_) || (screen_size_ != frame_size)) {
    if (frame_) {
      consumer_->ReleaseFrame(frame_);
      frame_ = NULL;
    }

    consumer_->AllocateFrame(
        media::VideoFrame::RGB32, screen_size_, &frame_,
        base::Bind(&RectangleUpdateDecoder::ProcessPacketData,
                   this, packet, done_runner.Release()));
    decoder_needs_reset_ = true;
    return;
  }
  ProcessPacketData(packet, done_runner.Release());
}

void RectangleUpdateDecoder::ProcessPacketData(
    const VideoPacket* packet, const base::Closure& done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::ProcessPacketData,
                              this, packet, done));
    return;
  }
  base::ScopedClosureRunner done_runner(done);

  if (decoder_needs_reset_) {
    decoder_->Reset();
    decoder_->Initialize(frame_);
    decoder_needs_reset_ = false;
  }

  if (!decoder_->IsReadyForData()) {
    // TODO(ajwong): This whole thing should move into an invalid state.
    LOG(ERROR) << "Decoder is unable to process data. Dropping packet.";
    return;
  }

  if (decoder_->DecodePacket(packet) == Decoder::DECODE_DONE)
    SubmitToConsumer();
}

void RectangleUpdateDecoder::SetOutputSize(const SkISize& size) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::SetOutputSize,
                              this, size));
    return;
  }

  // TODO(wez): Refresh the frame only if the ratio has changed.
  if (frame_) {
    SkIRect frame_rect = SkIRect::MakeWH(frame_->width(), frame_->height());
    refresh_rects_.push_back(frame_rect);
  }

  // TODO(hclam): If the scale ratio has changed we should reallocate a
  // VideoFrame of different size. However if the scale ratio is always
  // smaller than 1.0 we can use the same video frame.
  if (decoder_.get()) {
    decoder_->SetOutputSize(size);
    RefreshFullFrame();
  }
}

void RectangleUpdateDecoder::UpdateClipRect(const SkIRect& new_clip_rect) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::UpdateClipRect,
                              this, new_clip_rect));
    return;
  }

  if (new_clip_rect == clip_rect_ || !decoder_.get())
    return;

  // TODO(wez): Only refresh newly-exposed portions of the frame.
  if (frame_) {
    SkIRect frame_rect = SkIRect::MakeWH(frame_->width(), frame_->height());
    refresh_rects_.push_back(frame_rect);
  }

  clip_rect_ = new_clip_rect;
  decoder_->SetClipRect(new_clip_rect);

  // TODO(wez): Defer refresh so that multiple events can be batched.
  DoRefresh();
}

void RectangleUpdateDecoder::RefreshFullFrame() {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::RefreshFullFrame, this));
    return;
  }

  // If a video frame or the decoder is not allocated yet then don't
  // save the refresh rectangle to avoid wasted computation.
  if (!frame_ || !decoder_.get())
    return;

  refresh_rects_.push_back(
      SkIRect::MakeWH(static_cast<int>(frame_->width()),
                      static_cast<int>(frame_->height())));
  DoRefresh();
}

void RectangleUpdateDecoder::SubmitToConsumer() {
  // A frame is not allocated yet, we can reach here because of a refresh
  // request.
  if (!frame_)
    return;

  RectVector* dirty_rects = new RectVector();
  decoder_->GetUpdatedRects(dirty_rects);

  consumer_->OnPartialFrameOutput(frame_, dirty_rects, base::Bind(
      &RectangleUpdateDecoder::OnFrameConsumed, this, dirty_rects));
}

void RectangleUpdateDecoder::DoRefresh() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (refresh_rects_.empty())
    return;

  decoder_->RefreshRects(refresh_rects_);
  refresh_rects_.clear();
  SubmitToConsumer();
}

void RectangleUpdateDecoder::OnFrameConsumed(RectVector* rects) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::OnFrameConsumed,
                              this, rects));
    return;
  }

  delete rects;

  DoRefresh();
}

}  // namespace remoting
