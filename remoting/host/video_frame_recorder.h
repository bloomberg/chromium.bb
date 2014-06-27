// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VIDEO_FRAME_RECORDER_H_
#define REMOTING_HOST_VIDEO_FRAME_RECORDER_H_

#include <stdint.h>
#include <list>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"

namespace webrtc {
class DesktopFrame;
}

namespace remoting {

class VideoEncoder;

// Allows sequences of DesktopFrames passed to a VideoEncoder to be recorded.
//
// VideoFrameRecorder is design to support applications which use a dedicated
// thread for video encoding, but need to manage that process from a "main"
// or "control" thread.
//
// On the control thread:
// 1. Create the VideoFrameRecorder on the controlling thread.
// 2. Specify the amount of memory that may be used for recording.
// 3. Call WrapVideoEncoder(), passing the actual VideoEncoder that will be
//    used to encode frames.
// 4. Hand the returned wrapper VideoEncoder of to the video encoding thread,
//    to call in place of the actual VideoEncoder.
// 5. Start/stop frame recording as necessary.
// 6. Use NextFrame() to read each recorded frame in sequence.
//
// The wrapper VideoEncoder is designed to be handed off to the video encoding
// thread, and used and torn down there.
//
// The VideoFrameRecorder and VideoEncoder may be torn down in any order; frame
// recording will stop as soon as either is destroyed.

class VideoFrameRecorder {
 public:
  VideoFrameRecorder();
  virtual ~VideoFrameRecorder();

  // Wraps the supplied VideoEncoder, returning a replacement VideoEncoder that
  // will route frames to the recorder, as well as passing them for encoding.
  // This may be called at most once on each VideoFrameRecorder instance.
  scoped_ptr<VideoEncoder> WrapVideoEncoder(scoped_ptr<VideoEncoder> encoder);

  // Enables/disables frame recording. Frame recording is initially disabled.
  void SetEnableRecording(bool enable_recording);

  // Sets the maximum number of bytes of pixel data that may be recorded.
  // When this maximum is reached older frames will be discard to make space
  // for new ones.
  void SetMaxContentBytes(int64_t max_content_bytes);

  // Pops the next recorded frame in the sequence, and returns it.
  scoped_ptr<webrtc::DesktopFrame> NextFrame();

 private:
  class RecordingVideoEncoder;
  friend class RecordingVideoEncoder;

  void SetEncoderTaskRunner(scoped_refptr<base::TaskRunner> task_runner);
  void RecordFrame(scoped_ptr<webrtc::DesktopFrame> frame);

  // The recorded frames, in sequence.
  std::list<webrtc::DesktopFrame*> recorded_frames_;

  // Size of the recorded frames' content, in bytes.
  int64_t content_bytes_;

  // Size that recorded frames' content must not exceed.
  int64_t max_content_bytes_;

  // True if recording is started, false otherwise.
  bool enable_recording_;

  // Task runner on which the wrapper VideoEncoder is being run.
  scoped_refptr<base::TaskRunner> encoder_task_runner_;

  // Weak reference to the wrapper VideoEncoder, to use to control it.
  base::WeakPtr<RecordingVideoEncoder> recording_encoder_;

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  base::WeakPtrFactory<VideoFrameRecorder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameRecorder);
};

}  // namespace remoting

#endif  // REMOTING_HOST_VIDEO_FRAME_RECORDER_H_
