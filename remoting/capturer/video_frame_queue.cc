// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/capturer/video_frame_queue.h"

#include <algorithm>

#include "base/basictypes.h"
#include "remoting/capturer/video_frame.h"

namespace remoting {

VideoFrameQueue::VideoFrameQueue()
    : current_(0),
      previous_(NULL) {
  SetAllFramesNeedUpdate();
}

VideoFrameQueue::~VideoFrameQueue() {
}

void VideoFrameQueue::DoneWithCurrentFrame() {
  previous_ = current_frame();
  current_ = (current_ + 1) % kQueueLength;
}

void VideoFrameQueue::ReplaceCurrentFrame(scoped_ptr<VideoFrame> frame) {
  frames_[current_] = frame.Pass();
  needs_update_[current_] = false;
}

void VideoFrameQueue::SetAllFramesNeedUpdate() {
  std::fill(needs_update_, needs_update_ + arraysize(needs_update_), true);
  previous_ = NULL;
}

}  // namespace remoting
