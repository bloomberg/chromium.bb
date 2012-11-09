// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/simple_video_frame_provider.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "media/base/video_frame.h"

namespace webkit_media {

SimpleVideoFrameProvider::SimpleVideoFrameProvider(
    const gfx::Size& size,
    const base::TimeDelta& frame_duration,
    const base::Closure& error_cb,
    const VideoFrameProvider::RepaintCB& repaint_cb)
    : message_loop_proxy_(base::MessageLoopProxy::current()),
      size_(size),
      state_(kStopped),
      frame_duration_(frame_duration),
      error_cb_(error_cb),
      repaint_cb_(repaint_cb) {
}

SimpleVideoFrameProvider::~SimpleVideoFrameProvider() {}

void SimpleVideoFrameProvider::Start() {
  DVLOG(1) << "SimpleVideoFrameProvider::Start";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  state_ = kStarted;
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&SimpleVideoFrameProvider::GenerateFrame, this));
}

void SimpleVideoFrameProvider::Stop() {
  DVLOG(1) << "SimpleVideoFrameProvider::Stop";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  state_ = kStopped;
}

void SimpleVideoFrameProvider::Play() {
  DVLOG(1) << "SimpleVideoFrameProvider::Play";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == kPaused)
    state_ = kStarted;
}

void SimpleVideoFrameProvider::Pause() {
  DVLOG(1) << "SimpleVideoFrameProvider::Pause";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == kStarted)
    state_ = kPaused;
}

void SimpleVideoFrameProvider::GenerateFrame() {
  DVLOG(1) << "SimpleVideoFrameProvider::GenerateFrame";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == kStopped)
    return;

  if (state_ == kStarted) {
    // Always allocate a new frame.
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                       size_,
                                       gfx::Rect(size_),
                                       size_,
                                       current_time_);

    // TODO(wjia): set pixel data to pre-defined patterns if it's desired to
    // verify frame content.

    repaint_cb_.Run(video_frame);
  }

  current_time_ += frame_duration_;
  message_loop_proxy_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SimpleVideoFrameProvider::GenerateFrame, this),
      frame_duration_);
}

}  // namespace webkit_media
