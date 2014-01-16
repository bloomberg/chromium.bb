// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/media_stream_frame_buffer.h"

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {

MediaStreamFrameBuffer::Delegate::~Delegate() {}

void MediaStreamFrameBuffer::Delegate::OnNewFrameEnqueued() {
}

MediaStreamFrameBuffer::MediaStreamFrameBuffer(Delegate* delegate)
   : delegate_(delegate),
     frame_size_(0),
     number_of_frames_(0) {
  DCHECK(delegate_);
}

MediaStreamFrameBuffer::~MediaStreamFrameBuffer() {
}

bool MediaStreamFrameBuffer::SetFrames(
    int32_t number_of_frames,
    int32_t frame_size,
    scoped_ptr<base::SharedMemory> shm,
    bool enqueue_all_frames) {
  DCHECK(shm);
  DCHECK(!shm_);
  DCHECK_GT(number_of_frames, 0);
  DCHECK_GT(frame_size, static_cast<int32_t>(sizeof(MediaStreamFrame::Header)));
  DCHECK_EQ(frame_size & 0x3, 0);

  number_of_frames_ = number_of_frames;
  frame_size_ = frame_size;

  int32_t size = number_of_frames_ * frame_size;
  shm_ = shm.Pass();
  if (!shm_->Map(size))
    return false;

  uint8_t* p = reinterpret_cast<uint8_t*>(shm_->memory());
  for (int32_t i = 0; i < number_of_frames; ++i) {
    if (enqueue_all_frames)
      frame_queue_.push_back(i);
    frames_.push_back(reinterpret_cast<MediaStreamFrame*>(p));
    p += frame_size_;
  }
  return true;
}

int32_t MediaStreamFrameBuffer::DequeueFrame() {
  if (frame_queue_.empty())
    return PP_ERROR_FAILED;
  int32_t frame = frame_queue_.front();
  frame_queue_.pop_front();
  return frame;
}

void MediaStreamFrameBuffer::EnqueueFrame(int32_t index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, number_of_frames_);
  frame_queue_.push_back(index);
  delegate_->OnNewFrameEnqueued();
}

MediaStreamFrame* MediaStreamFrameBuffer::GetFramePointer(
    int32_t index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, number_of_frames_);
  return frames_[index];
}

}  // namespace ppapi
