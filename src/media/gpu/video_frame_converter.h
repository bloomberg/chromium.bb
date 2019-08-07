// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_VIDEO_FRAME_CONVERTER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Video decoders make use of a video frame pool to allocate output frames,
// which are sent to the client after decoding. However, the storage type of the
// allocated frame can be different from what the client expects. This class
// can be used to convert the type of output video frame in this case.
class MEDIA_GPU_EXPORT VideoFrameConverter {
 public:
  VideoFrameConverter();
  virtual ~VideoFrameConverter();

  // Setter method of |parent_task_runner_|. This method should be called before
  // any ConvertFrame() is called.
  void set_parent_task_runner(
      scoped_refptr<base::SequencedTaskRunner> parent_task_runner);

  // Convert the frame. The default implementation returns the passed frame
  // as-is.
  virtual scoped_refptr<VideoFrame> ConvertFrame(
      scoped_refptr<VideoFrame> frame);

 protected:
  // The working task runner. ConvertFrame() should be called on this.
  scoped_refptr<base::SequencedTaskRunner> parent_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoFrameConverter);
};

}  // namespace media
#endif  // MEDIA_GPU_VIDEO_FRAME_CONVERTER_H_
