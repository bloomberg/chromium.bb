// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_MEDIA_STREAM_FRAME_BUFFER_H_
#define PPAPI_SHARED_IMPL_MEDIA_STREAM_FRAME_BUFFER_H_

#include <deque>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "ppapi/shared_impl/media_stream_frame.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// This class is used by both read side and write side of a MediaStreamTrack to
// maintain a queue of frames for reading or writing.
//
// An example:
//  1. The writer calls the writer's |frame_buffer_.Dequeue()| to get a free
//     frame.
//  2. The writer fills data into the frame.
//  3. The writer sends the frame index to the reader via an IPC message.
//  4. The reader receives the frame index and calls the reader's
//     |frame_buffer.Enqueue()| to put the frame into the read's queue.
//  5. The reader calls reader's |frame_buffer_.Dequeue()| to get a received
//     frame.
//  6. When the frame from the step 5 is consumed, the reader sends the frame
//     index back to writer via an IPC message.
//  7. The writer receives the frame index and puts it back to the writer's free
//     frame queue by calling the writer's |frame_buffer_.Enqueue()|.
//  8. Go back to step 1.
class PPAPI_SHARED_EXPORT MediaStreamFrameBuffer {
 public:
  class PPAPI_SHARED_EXPORT Delegate {
   public:
    virtual ~Delegate();
    // It is called when a new frame is enqueued.
    virtual void OnNewFrameEnqueued();
  };

  // MediaStreamFrameBuffer doesn't own |delegate|, the caller should keep
  // it alive during the MediaStreamFrameBuffer's lifecycle.
  explicit MediaStreamFrameBuffer(Delegate* delegate);

  ~MediaStreamFrameBuffer();

  int32_t number_of_frames() const { return number_of_frames_; }

  int32_t frame_size() const { return frame_size_; }

  // Initializes shared memory for frames transmission.
  bool SetFrames(int32_t number_of_frames,
                 int32_t frame_size,
                 scoped_ptr<base::SharedMemory> shm,
                 bool enqueue_all_frames);

  // Dequeues a frame from |frame_queue_|.
  int32_t DequeueFrame();

  // Puts a frame into |frame_queue_|.
  void EnqueueFrame(int32_t index);

  // Gets the frame address for the given frame index.
  MediaStreamFrame* GetFramePointer(int32_t index);

 private:
  Delegate* delegate_;

  // A queue of frame indexes.
  std::deque<int32_t> frame_queue_;

  // A vector of frame pointers. It is used for index to pointer converting.
  std::vector<MediaStreamFrame*> frames_;

  // The frame size in bytes.
  int32_t frame_size_;

  // The number of frames in the shared memory.
  int32_t number_of_frames_;

  // A memory block shared between renderer process and plugin process.
  scoped_ptr<base::SharedMemory> shm_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamFrameBuffer);
};

}  // namespace ppapi

#endif  // PPAPI_SHAERD_IMPL_MEDIA_STREAM_FRAME_BUFFER_H_
