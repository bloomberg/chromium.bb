// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/rectangle_update_decoder.h"

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

class PartialFrameCleanup : public Task {
 public:
  PartialFrameCleanup(media::VideoFrame* frame, UpdatedRects* rects,
                      RectangleUpdateDecoder* decoder)
      : frame_(frame), rects_(rects), decoder_(decoder) {
  }

  virtual void Run() {
    delete rects_;
    frame_ = NULL;

    // There maybe pending request to refresh rectangles.
    decoder_->OnFrameConsumed();
    decoder_ = NULL;
  }

 private:
  scoped_refptr<media::VideoFrame> frame_;
  UpdatedRects* rects_;
  scoped_refptr<RectangleUpdateDecoder> decoder_;
};

RectangleUpdateDecoder::RectangleUpdateDecoder(MessageLoop* message_loop,
                                               FrameConsumer* consumer)
    : message_loop_(message_loop),
      consumer_(consumer),
      frame_is_new_(false),
      frame_is_consuming_(false) {
}

RectangleUpdateDecoder::~RectangleUpdateDecoder() {
}

void RectangleUpdateDecoder::Initialize(const SessionConfig& config) {
  initial_screen_size_ = gfx::Size(config.initial_resolution().width,
                                   config.initial_resolution().height);

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
                                          Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &RectangleUpdateDecoder::DecodePacket, packet,
                          done));
    return;
  }
  base::ScopedTaskRunner done_runner(done);

  AllocateFrame(packet, done_runner.Release());
}

void RectangleUpdateDecoder::AllocateFrame(const VideoPacket* packet,
                                           Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &RectangleUpdateDecoder::AllocateFrame, packet, done));
    return;
  }
  base::ScopedTaskRunner done_runner(done);

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
      consumer_->ReleaseFrame(frame_);
      frame_ = NULL;
    }

    consumer_->AllocateFrame(media::VideoFrame::RGB32,
                             screen_size.width(), screen_size.height(),
                             base::TimeDelta(), base::TimeDelta(),
                             &frame_,
                             NewRunnableMethod(this,
                                 &RectangleUpdateDecoder::ProcessPacketData,
                                 packet, done_runner.Release()));
    frame_is_new_ = true;
    return;
  }
  ProcessPacketData(packet, done_runner.Release());
}

void RectangleUpdateDecoder::ProcessPacketData(
    const VideoPacket* packet, Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &RectangleUpdateDecoder::ProcessPacketData, packet,
                          done));
    return;
  }
  base::ScopedTaskRunner done_runner(done);

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

  if (decoder_->DecodePacket(packet) == Decoder::DECODE_DONE)
    SubmitToConsumer();
}

void RectangleUpdateDecoder::SetScaleRatios(double horizontal_ratio,
                                            double vertical_ratio) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &RectangleUpdateDecoder::SetScaleRatios,
                          horizontal_ratio,
                          vertical_ratio));
    return;
  }

  // TODO(hclam): If the scale ratio has changed we should reallocate a
  // VideoFrame of different size. However if the scale ratio is always
  // smaller than 1.0 we can use the same video frame.
  decoder_->SetScaleRatios(horizontal_ratio, vertical_ratio);
}

void RectangleUpdateDecoder::UpdateClipRect(const gfx::Rect& new_clip_rect) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &RectangleUpdateDecoder::UpdateClipRect, new_clip_rect));
    return;
  }

  if (new_clip_rect == clip_rect_ || !decoder_.get())
    return;

  // Find out the rectangles to show because of clip rect is updated.
  if (new_clip_rect.y() < clip_rect_.y()) {
    refresh_rects_.push_back(
        gfx::Rect(new_clip_rect.x(),
                  new_clip_rect.y(),
                  new_clip_rect.width(),
                  clip_rect_.y() - new_clip_rect.y()));
  }

  if (new_clip_rect.x() < clip_rect_.x()) {
    refresh_rects_.push_back(
        gfx::Rect(new_clip_rect.x(),
                  clip_rect_.y(),
                  clip_rect_.x() - new_clip_rect.x(),
                  clip_rect_.height()));
  }

  if (new_clip_rect.right() > clip_rect_.right()) {
    refresh_rects_.push_back(
        gfx::Rect(clip_rect_.right(),
                  clip_rect_.y(),
                  new_clip_rect.right() - clip_rect_.right(),
                  new_clip_rect.height()));
  }

  if (new_clip_rect.bottom() > clip_rect_.bottom()) {
    refresh_rects_.push_back(
        gfx::Rect(new_clip_rect.x(),
                  clip_rect_.bottom(),
                  new_clip_rect.width(),
                  new_clip_rect.bottom() - clip_rect_.bottom()));
  }

  clip_rect_ = new_clip_rect;
  decoder_->SetClipRect(new_clip_rect);
  DoRefresh();
}

void RectangleUpdateDecoder::RefreshFullFrame() {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RectangleUpdateDecoder::RefreshFullFrame));
    return;
  }

  // If a video frame or the decoder is not allocated yet then don't
  // save the refresh rectangle to avoid wasted computation.
  if (!frame_ || !decoder_.get())
    return;

  refresh_rects_.push_back(
      gfx::Rect(0, 0, static_cast<int>(frame_->width()),
                static_cast<int>(frame_->height())));
  DoRefresh();
}

void RectangleUpdateDecoder::SubmitToConsumer() {
  // A frame is not allocated yet, we can reach here because of a refresh
  // request.
  if (!frame_)
    return;

  UpdatedRects* dirty_rects = new UpdatedRects();
  decoder_->GetUpdatedRects(dirty_rects);

  frame_is_consuming_ = true;
  consumer_->OnPartialFrameOutput(
      frame_, dirty_rects,
      new PartialFrameCleanup(frame_, dirty_rects, this));
}

void RectangleUpdateDecoder::DoRefresh() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (refresh_rects_.empty())
    return;

  decoder_->RefreshRects(refresh_rects_);
  refresh_rects_.clear();
  SubmitToConsumer();
}

void RectangleUpdateDecoder::OnFrameConsumed() {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RectangleUpdateDecoder::OnFrameConsumed));
    return;
  }

  frame_is_consuming_ = false;
  DoRefresh();
}

}  // namespace remoting
