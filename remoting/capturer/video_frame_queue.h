// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CAPTURER_VIDEO_FRAME_QUEUE_H_
#define REMOTING_CAPTURER_VIDEO_FRAME_QUEUE_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class VideoFrame;

// Represents a queue of reusable video frames. Provides access to the 'current'
// frame - the frame that the caller is working with at the moment, and to
// the 'previous' frame - the predecessor of the current frame swapped by
// DoneWithCurrentFrame() call, if any.
//
// The caller is expected to (re)allocate frames if current_frame_needs_update()
// is set. The caller can mark all frames in the queue for reallocation (when,
// say, frame dimensions  change). The queue records which frames need updating
// which the caller can query.
class VideoFrameQueue {
 public:
  VideoFrameQueue();
  ~VideoFrameQueue();

  // Moves to the next frame in the queue, moving the 'current' frame to become
  // the 'previous' one.
  void DoneWithCurrentFrame();

  // Replaces the current frame with a new one allocated by the caller.
  // The existing frame (if any) is destroyed.
  void ReplaceCurrentFrame(scoped_ptr<VideoFrame> frame);

  // Marks all frames obsolete and resets the previous frame pointer. No
  // frames are freed though as the caller can still access them.
  void SetAllFramesNeedUpdate();

  VideoFrame* current_frame() const {
    return frames_[current_].get();
  }

  bool current_frame_needs_update() const {
    return !current_frame() || needs_update_[current_];
  }

  VideoFrame* previous_frame() const { return previous_; }

 private:
  // Index of the current frame.
  int current_;

  static const int kQueueLength = 2;
  scoped_ptr<VideoFrame> frames_[kQueueLength];

  // True if the corresponding frame needs to be re-allocated.
  bool needs_update_[kQueueLength];

  // Points to the previous frame if any.
  VideoFrame* previous_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameQueue);
};

}  // namespace remoting

#endif  // REMOTING_CAPTURER_VIDEO_FRAME_QUEUE_H_
