// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/video_frame_converter.h"

namespace media {

VideoFrameConverter::VideoFrameConverter() = default;

VideoFrameConverter::~VideoFrameConverter() = default;

void VideoFrameConverter::set_parent_task_runner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  parent_task_runner_ = std::move(task_runner);
}

scoped_refptr<VideoFrame> VideoFrameConverter::ConvertFrame(
    scoped_refptr<VideoFrame> frame) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  return frame;
}

}  // namespace media
