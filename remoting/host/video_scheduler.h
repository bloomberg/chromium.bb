// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VIDEO_SCHEDULER_H_
#define REMOTING_HOST_VIDEO_SCHEDULER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/capture_scheduler.h"
#include "remoting/proto/video.pb.h"
#include "third_party/skia/include/core/SkSize.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class CaptureData;
class CursorShapeInfo;
class VideoFrameCapturer;

namespace protocol {
class CursorShapeInfo;
class CursorShapeStub;
class VideoStub;
}  // namespace protocol

// Class responsible for scheduling frame captures from a VideoFrameCapturer,
// delivering them to a VideoEncoder to encode, and finally passing the encoded
// video packets to the specified VideoStub to send on the network.
//
// THREADING
//
// This class is supplied TaskRunners to use for capture, encode and network
// operations.  Capture, encode and network transmission tasks are interleaved
// as illustrated below:
//
// |       CAPTURE       ENCODE     NETWORK
// |    .............
// |    .  Capture  .
// |    .............
// |                  ............
// |                  .          .
// |    ............. .          .
// |    .  Capture  . .  Encode  .
// |    ............. .          .
// |                  .          .
// |                  ............
// |    ............. ............ ..........
// |    .  Capture  . .          . .  Send  .
// |    ............. .          . ..........
// |                  .  Encode  .
// |                  .          .
// |                  .          .
// |                  ............
// | Time
// v
//
// VideoScheduler would ideally schedule captures so as to saturate the slowest
// of the capture, encode and network processes.  However, it also needs to
// rate-limit captures to avoid overloading the host system, either by consuming
// too much CPU, or hogging the host's graphics subsystem.

class VideoScheduler : public base::RefCountedThreadSafe<VideoScheduler>,
                       public VideoFrameCapturer::Delegate {
 public:
  // Creates a VideoScheduler running capture, encode and network tasks on the
  // supplied TaskRunners.  Video and cursor shape updates will be pumped to
  // |video_stub| and |client_stub|, which must remain valid until Stop() is
  // called. |capturer| is used to capture frames.
  static scoped_refptr<VideoScheduler> Create(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_ptr<VideoFrameCapturer> capturer,
      scoped_ptr<VideoEncoder> encoder,
      protocol::CursorShapeStub* cursor_stub,
      protocol::VideoStub* video_stub);

  // VideoFrameCapturer::Delegate implementation.
  virtual void OnCaptureCompleted(
      scoped_refptr<CaptureData> capture_data) OVERRIDE;
  virtual void OnCursorShapeChanged(
      scoped_ptr<MouseCursorShape> cursor_shape) OVERRIDE;

  // Stop scheduling frame captures. This object cannot be re-used once
  // it has been stopped.
  void Stop();

  // Pauses or resumes scheduling of frame captures.  Pausing/resuming captures
  // only affects capture scheduling and does not stop/start the capturer.
  void Pause(bool pause);

  // Updates the sequence number embedded in VideoPackets.
  // Sequence numbers are used for performance measurements.
  void UpdateSequenceNumber(int64 sequence_number);

 private:
  friend class base::RefCountedThreadSafe<VideoScheduler>;

  VideoScheduler(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_ptr<VideoFrameCapturer> capturer,
      scoped_ptr<VideoEncoder> encoder,
      protocol::CursorShapeStub* cursor_stub,
      protocol::VideoStub* video_stub);
  virtual ~VideoScheduler();

  // Capturer thread ----------------------------------------------------------

  // Starts the capturer on the capture thread.
  void StartOnCaptureThread();

  // Stops scheduling frame captures on the capture thread, and posts
  // StopOnEncodeThread() to the network thread when done.
  void StopOnCaptureThread();

  // Schedules the next call to CaptureNextFrame.
  void ScheduleNextCapture();

  // Starts the next frame capture, unless there are already too many pending.
  void CaptureNextFrame();

  // Called when a frame capture has been encoded & sent to the client.
  void FrameCaptureCompleted();

  // Network thread -----------------------------------------------------------

  // Send |packet| to the client, unless we are in the process of stopping.
  void SendVideoPacket(scoped_ptr<VideoPacket> packet);

  // Callback passed to |video_stub_| for the last packet in each frame, to
  // rate-limit frame captures to network throughput.
  void VideoFrameSentCallback();

  // Send updated cursor shape to client.
  void SendCursorShape(scoped_ptr<protocol::CursorShapeInfo> cursor_shape);

  // Posted to the network thread to delete |capturer| on the thread that
  // created it.
  void StopOnNetworkThread(scoped_ptr<VideoFrameCapturer> capturer);

  // Encoder thread -----------------------------------------------------------

  // Encode a frame, passing generated VideoPackets to SendVideoPacket().
  void EncodeFrame(scoped_refptr<CaptureData> capture_data);

  void EncodedDataAvailableCallback(scoped_ptr<VideoPacket> packet);

  // Used to synchronize capture and encode thread teardown, notifying the
  // network thread when done.
  void StopOnEncodeThread(scoped_ptr<VideoFrameCapturer> capturer);

  // Task runners used by this class.
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Used to capture frames. Always accessed on the capture thread.
  scoped_ptr<VideoFrameCapturer> capturer_;

  // Used to encode captured frames. Always accessed on the encode thread.
  scoped_ptr<VideoEncoder> encoder_;

  // Interfaces through which video frames and cursor shapes are passed to the
  // client. These members are always accessed on the network thread.
  protocol::CursorShapeStub* cursor_stub_;
  protocol::VideoStub* video_stub_;

  // Timer used to schedule CaptureNextFrame().
  scoped_ptr<base::OneShotTimer<VideoScheduler> > capture_timer_;

  // Count the number of recordings (i.e. capture or encode) happening.
  int pending_captures_;

  // True if the previous scheduled capture was skipped.
  bool did_skip_frame_;

  // True if capture of video frames is paused.
  bool is_paused_;

  // This is a number updated by client to trace performance.
  int64 sequence_number_;

  // An object to schedule capturing.
  CaptureScheduler scheduler_;

  DISALLOW_COPY_AND_ASSIGN(VideoScheduler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_VIDEO_SCHEDULER_H_
